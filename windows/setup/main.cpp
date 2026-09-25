#include "credentials.h"
#include "crypto.h"
#include "hid_device.h"
#include "safetouch_protocol.h"
#include <windows.h>
#include <lm.h>
#include <sddl.h>
#include <algorithm>
#include <array>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

namespace {
using namespace safetouch;
bool IsAdministrator(){BOOL member=FALSE;PSID sid=nullptr;SID_IDENTIFIER_AUTHORITY nt=SECURITY_NT_AUTHORITY;if(AllocateAndInitializeSid(&nt,2,SECURITY_BUILTIN_DOMAIN_RID,DOMAIN_ALIAS_RID_ADMINS,0,0,0,0,0,0,&sid)){CheckTokenMembership(nullptr,sid,&member);FreeSid(sid);}return member!=FALSE;}
std::vector<std::wstring> LocalAccounts(){std::vector<std::wstring> users;DWORD resume=0;do{LPUSER_INFO_1 buffer=nullptr;DWORD read=0,total=0;NET_API_STATUS status=NetUserEnum(nullptr,1,FILTER_NORMAL_ACCOUNT,reinterpret_cast<LPBYTE*>(&buffer),MAX_PREFERRED_LENGTH,&read,&total,&resume);if(status!=NERR_Success&&status!=ERROR_MORE_DATA)break;for(DWORD i=0;i<read;++i)if(!(buffer[i].usri1_flags&UF_ACCOUNTDISABLE))users.emplace_back(buffer[i].usri1_name);if(buffer)NetApiBufferFree(buffer);if(status!=ERROR_MORE_DATA)break;}while(true);return users;}
bool SplitAccount(const std::wstring& qualified,std::wstring& domain,std::wstring& user){auto slash=qualified.find(L'\\');if(slash==std::wstring::npos){wchar_t computer[MAX_COMPUTERNAME_LENGTH+1];DWORD count=std::size(computer);if(!GetComputerNameW(computer,&count))return false;domain.assign(computer,count);user=qualified;}else{domain=qualified.substr(0,slash);user=qualified.substr(slash+1);}return !domain.empty()&&!user.empty();}
bool AccountSid(const std::wstring& qualified,std::wstring& sidString){DWORD sidSize=0,domainSize=0;SID_NAME_USE use;LookupAccountNameW(nullptr,qualified.c_str(),nullptr,&sidSize,nullptr,&domainSize,&use);if(GetLastError()!=ERROR_INSUFFICIENT_BUFFER)return false;std::vector<uint8_t> sid(sidSize);std::vector<wchar_t> domain(domainSize);if(!LookupAccountNameW(nullptr,qualified.c_str(),sid.data(),&sidSize,domain.data(),&domainSize,&use))return false;LPWSTR text=nullptr;if(!ConvertSidToStringSidW(sid.data(),&text))return false;sidString=text;LocalFree(text);return true;}
SecureVector<wchar_t> ReadPassword(){SecureVector<wchar_t> value;HANDLE input=GetStdHandle(STD_INPUT_HANDLE);DWORD mode=0;GetConsoleMode(input,&mode);SetConsoleMode(input,mode&~ENABLE_ECHO_INPUT);std::wstring line;std::getline(std::wcin,line);SetConsoleMode(input,mode);std::wcout<<L"\n";value.resize(line.size());std::copy(line.begin(),line.end(),value.data());SecureZeroMemory(line.data(),line.size()*sizeof(wchar_t));return value;}
std::array<uint8_t,16> DisplayName(const std::wstring& user){std::array<uint8_t,16> out{};for(size_t i=0;i<std::min(user.size(),out.size());++i){wchar_t c=user[i];out[i]=static_cast<uint8_t>(c>=32&&c<=126?c:'?');}return out;}
bool Send(HidDevice& device,uint8_t command,std::array<uint8_t,64>& packet,std::array<uint8_t,64>& reply,std::wstring& error){packet.fill(0);packet[0]=command;return device.Command(packet,reply,5000,error);}
struct KeyGuard { Key32& first;Key32& second;Key32& third;~KeyGuard(){SecureZeroMemory(first.data(),first.size());SecureZeroMemory(second.data(),second.size());SecureZeroMemory(third.data(),third.size());} };
}

int wmain(int argc,wchar_t** argv){
    /* ATR-only cards are accepted by default. The card remains one factor;
       the registered SafeTouch and its GREEN-button confirmation are required. */
    bool allowAtr=true;std::wstring requested;for(int i=1;i<argc;++i){if(std::wstring_view(argv[i])==L"--allow-atr")allowAtr=true;else if(std::wstring_view(argv[i])==L"--require-emv")allowAtr=false;else if(std::wstring_view(argv[i])==L"--user"&&i+1<argc)requested=argv[++i];else{std::wcerr<<L"Usage: SafeTouchSetup [--user COMPUTER\\name] [--require-emv]\n";return 2;}}
    if(!IsAdministrator()){std::wcerr<<L"Run SafeTouchSetup from an elevated console.\n";return 1;}
    if(requested.empty()){auto users=LocalAccounts();if(users.empty()){std::wcerr<<L"No enabled local accounts found.\n";return 1;}std::wcout<<L"Select Windows account:\n";for(size_t i=0;i<users.size();++i)std::wcout<<L"  "<<i+1<<L") "<<users[i]<<L"\n";std::wcout<<L"> ";size_t selected=0;std::wcin>>selected;std::wcin.ignore((std::numeric_limits<std::streamsize>::max)(),'\n');if(!selected||selected>users.size()){std::wcerr<<L"Invalid selection.\n";return 1;}requested=users[selected-1];}
    std::wstring domain,user,sid;if(!SplitAccount(requested,domain,user)||!AccountSid(domain+L"\\"+user,sid)){std::wcerr<<L"Cannot resolve account SID.\n";return 1;}
    std::wcout<<L"Enter the Windows password for "<<domain<<L"\\"<<user<<L": ";auto password=ReadPassword();HANDLE token=nullptr;std::wstring passwordString(password.empty()?L"":password.data(),password.size());if(!LogonUserW(user.c_str(),domain.c_str(),passwordString.c_str(),LOGON32_LOGON_INTERACTIVE,LOGON32_PROVIDER_DEFAULT,&token)){SecureZeroMemory(passwordString.data(),passwordString.size()*sizeof(wchar_t));std::wcerr<<L"Password verification failed ("<<GetLastError()<<L").\n";return 1;}CloseHandle(token);SecureZeroMemory(passwordString.data(),passwordString.size()*sizeof(wchar_t));
    HidDevice device;std::wstring error;if(!device.Open(error)){std::wcerr<<error<<L"\n";return 1;}Key32 secret{},auth{},wrap{};KeyGuard keyGuard{secret,auth,wrap};Id16 expectedDevice{};if(!RandomBytes(secret)||!DeriveDeviceKeys(secret,auth,wrap,expectedDevice)){std::wcerr<<L"Key generation failed.\n";return 1;}
    std::array<uint8_t,64> request{},reply{};request[0]=ST_CMD_ENROLL_BEGIN;request[ST_ENROLL_FLAGS_OFFSET]=allowAtr?ST_ENROLL_ALLOW_ATR:0;request[ST_ENROLL_NAME_LENGTH_OFFSET]=static_cast<uint8_t>(std::min<size_t>(user.size(),16));std::copy(secret.begin(),secret.end(),request.begin()+ST_ENROLL_SECRET_OFFSET);auto display=DisplayName(user);std::copy(display.begin(),display.end(),request.begin()+ST_ENROLL_NAME_OFFSET);
    if(!device.Command(request,reply,5000,error)||reply[1]!=ST_RESULT_OK){std::wcerr<<(error.empty()?L"Enrollment rejected. Hold both buttons while starting setup to replace an existing enrollment.":error)<<L"\n";return 1;}
    std::wcout<<L"Insert the card and follow the SafeTouch display.\n";uint8_t previous=0xFF;bool enrolled=false;for(unsigned attempt=0;attempt<480;++attempt){Sleep(250);if(!Send(device,ST_CMD_STATUS,request,reply,error)){std::wcerr<<error<<L"\n";break;}if(reply[1]==ST_RESULT_CARD_WEAK_ID){std::wcerr<<L"The card exposes only a shared ATR fingerprint. Re-run without --require-emv to allow it.\n";break;}uint8_t state=reply[2];if(state!=previous){previous=state;if(state==ST_STATE_INSERT_CARD)std::wcout<<L"Insert card...\n";else if(state==ST_STATE_READING_CARD)std::wcout<<L"Reading card...\n";else if(state==ST_STATE_PRESS_GREEN)std::wcout<<L"Press GREEN on SafeTouch...\n";else if(state==ST_STATE_CANCELED){std::wcerr<<L"Canceled.\n";break;}else if(state==ST_STATE_ERROR){std::wcerr<<L"Device error.\n";break;}}
        if(state==ST_STATE_ENROLLED){enrolled=true;break;}}
    if(!enrolled){SecureZeroMemory(secret.data(),secret.size());SecureZeroMemory(auth.data(),auth.size());SecureZeroMemory(wrap.data(),wrap.size());return 1;}
    if(!Send(device,ST_CMD_INFO,request,reply,error)){std::wcerr<<error<<L"\n";return 1;}CredentialRecord record;std::copy_n(reply.begin()+ST_INFO_DEVICE_ID_OFFSET,16,record.deviceId.begin());std::copy_n(reply.begin()+ST_INFO_CARD_ID_OFFSET,16,record.cardId.begin());record.cardSource=reply[3];record.user=user;record.domain=domain;record.sid=sid;
    if(!ConstantTimeEqual(record.deviceId,expectedDevice)){std::wcerr<<L"Device identity verification failed.\n";return 1;}if(!SaveCredentialRecord(DefaultCredentialsPath(),record,auth,wrap,std::span<const wchar_t>(password.data(),password.size()),error)){std::wcerr<<error<<L"\n";return 1;}
    SecureZeroMemory(secret.data(),secret.size());SecureZeroMemory(auth.data(),auth.size());SecureZeroMemory(wrap.data(),wrap.size());std::wcout<<L"SafeTouch registered. credentials.dat was written with a SYSTEM/Administrators-only ACL.\n";return 0;
}
