#include "credentials.h"
#include <windows.h>
#include <dpapi.h>
#include <aclapi.h>
#include <sddl.h>
#include <shlobj.h>
#include <algorithm>
#include <fstream>
#include <limits>

namespace safetouch {
namespace {
#pragma pack(push,1)
struct Header {
    char magic[8];
    uint32_t version,headerSize,totalSize,userBytes,domainBytes,sidBytes,authBytes,cipherBytes,cardSource;
    Id16 deviceId,cardId;
    std::array<uint8_t,12> iv;
    std::array<uint8_t,16> tag;
};
#pragma pack(pop)
constexpr char Magic[8]={'S','T','C','R','E','D','1','\0'};
std::vector<uint8_t> Entropy(const CredentialRecord& r){std::vector<uint8_t> v(r.deviceId.begin(),r.deviceId.end());v.insert(v.end(),r.cardId.begin(),r.cardId.end());return v;}
std::vector<uint8_t> Aad(const CredentialRecord& r){
    std::vector<uint8_t> v(r.deviceId.begin(),r.deviceId.end());v.insert(v.end(),r.cardId.begin(),r.cardId.end());
    auto append=[&](const std::wstring& s){const auto* p=reinterpret_cast<const uint8_t*>(s.data());v.insert(v.end(),p,p+s.size()*sizeof(wchar_t));};append(r.user);append(r.domain);append(r.sid);return v;
}
bool Fits32(size_t n){return n<=std::numeric_limits<uint32_t>::max();}
bool HardenAcl(const std::filesystem::path& path){
    PSECURITY_DESCRIPTOR sd=nullptr;if(!ConvertStringSecurityDescriptorToSecurityDescriptorW(L"D:P(A;;FA;;;SY)(A;;FA;;;BA)",SDDL_REVISION_1,&sd,nullptr))return false;
    BOOL present=FALSE,def=FALSE;PACL dacl=nullptr;bool ok=GetSecurityDescriptorDacl(sd,&present,&dacl,&def)&&SetNamedSecurityInfoW(const_cast<LPWSTR>(path.c_str()),SE_FILE_OBJECT,DACL_SECURITY_INFORMATION|PROTECTED_DACL_SECURITY_INFORMATION,nullptr,nullptr,dacl,nullptr)==ERROR_SUCCESS;LocalFree(sd);return ok;
}
}
std::filesystem::path DefaultCredentialsPath(){PWSTR raw=nullptr;if(SUCCEEDED(SHGetKnownFolderPath(FOLDERID_ProgramData,KF_FLAG_DEFAULT,nullptr,&raw))){std::filesystem::path result=std::filesystem::path(raw)/L"SafeVoid"/L"credentials.dat";CoTaskMemFree(raw);return result;}return std::filesystem::path(L"C:\\ProgramData\\SafeVoid\\credentials.dat");}
std::filesystem::path LegacyCredentialsPath(){PWSTR raw=nullptr;if(SUCCEEDED(SHGetKnownFolderPath(FOLDERID_ProgramData,KF_FLAG_DEFAULT,nullptr,&raw))){std::filesystem::path result=std::filesystem::path(raw)/L"SafeTouch"/L"credentials.dat";CoTaskMemFree(raw);return result;}return std::filesystem::path(L"C:\\ProgramData\\SafeTouch\\credentials.dat");}
bool SaveCredentialRecord(const std::filesystem::path& path,CredentialRecord& record,const Key32& auth,const Key32& wrap,std::span<const wchar_t> password,std::wstring& error){
    auto entropy=Entropy(record);DATA_BLOB input{static_cast<DWORD>(auth.size()),const_cast<BYTE*>(auth.data())},extra{static_cast<DWORD>(entropy.size()),entropy.data()},protectedBlob{};
    if(!CryptProtectData(&input,L"SafeVoid proof verifier",&extra,nullptr,nullptr,CRYPTPROTECT_LOCAL_MACHINE|CRYPTPROTECT_UI_FORBIDDEN,&protectedBlob)){error=L"CryptProtectData failed";return false;}
    record.protectedAuthKey.assign(protectedBlob.pbData,protectedBlob.pbData+protectedBlob.cbData);SecureZeroMemory(protectedBlob.pbData,protectedBlob.cbData);LocalFree(protectedBlob.pbData);
    auto aad=Aad(record);const auto* passwordBytes=reinterpret_cast<const uint8_t*>(password.data());size_t passwordSize=password.size()*sizeof(wchar_t);
    if(!AesGcmEncrypt(wrap,std::span<const uint8_t>(passwordBytes,passwordSize),aad,record.iv,record.ciphertext,record.tag)){error=L"AES-256-GCM encryption failed";return false;}
    size_t ub=record.user.size()*sizeof(wchar_t),db=record.domain.size()*sizeof(wchar_t),sb=record.sid.size()*sizeof(wchar_t),total=sizeof(Header)+ub+db+sb+record.protectedAuthKey.size()+record.ciphertext.size();
    if(!Fits32(total)||!Fits32(ub)||!Fits32(db)||!Fits32(sb)){error=L"Credential record is too large";return false;}
    Header h{};std::copy(std::begin(Magic),std::end(Magic),h.magic);h.version=1;h.headerSize=sizeof(h);h.totalSize=static_cast<uint32_t>(total);h.userBytes=static_cast<uint32_t>(ub);h.domainBytes=static_cast<uint32_t>(db);h.sidBytes=static_cast<uint32_t>(sb);h.authBytes=static_cast<uint32_t>(record.protectedAuthKey.size());h.cipherBytes=static_cast<uint32_t>(record.ciphertext.size());h.cardSource=record.cardSource;h.deviceId=record.deviceId;h.cardId=record.cardId;h.iv=record.iv;h.tag=record.tag;
    std::error_code ec;std::filesystem::create_directories(path.parent_path(),ec);auto temporary=path;temporary+=L".tmp";std::ofstream out(temporary,std::ios::binary|std::ios::trunc);if(!out){error=L"Cannot create credentials.dat";return false;}
    out.write(reinterpret_cast<const char*>(&h),sizeof(h));auto writeWide=[&](const std::wstring& s){out.write(reinterpret_cast<const char*>(s.data()),static_cast<std::streamsize>(s.size()*sizeof(wchar_t)));};writeWide(record.user);writeWide(record.domain);writeWide(record.sid);out.write(reinterpret_cast<const char*>(record.protectedAuthKey.data()),record.protectedAuthKey.size());out.write(reinterpret_cast<const char*>(record.ciphertext.data()),record.ciphertext.size());out.close();
    if(!out){error=L"Cannot finish credentials.dat";return false;}if(!HardenAcl(temporary)){error=L"Cannot restrict credentials.dat ACL";DeleteFileW(temporary.c_str());return false;}
    if(!MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)){error=L"Cannot atomically install credentials.dat";DeleteFileW(temporary.c_str());return false;}return true;
}
bool LoadCredentialRecord(const std::filesystem::path& path,CredentialRecord& r,std::wstring& error){
    std::ifstream in(path,std::ios::binary|std::ios::ate);if(!in){error=L"credentials.dat is missing";return false;}auto length=in.tellg();if(length<static_cast<std::streamoff>(sizeof(Header))||length>1024*1024){error=L"Invalid credentials.dat size";return false;}in.seekg(0);Header h{};in.read(reinterpret_cast<char*>(&h),sizeof(h));
    uint64_t expected=sizeof(h)+static_cast<uint64_t>(h.userBytes)+h.domainBytes+h.sidBytes+h.authBytes+h.cipherBytes;
    if(memcmp(h.magic,Magic,8)||h.version!=1||h.headerSize!=sizeof(h)||h.totalSize!=expected||expected!=static_cast<uint64_t>(length)||h.userBytes%sizeof(wchar_t)||h.domainBytes%sizeof(wchar_t)||h.sidBytes%sizeof(wchar_t)||h.authBytes>4096){error=L"Invalid credentials.dat header";return false;}
    auto readWide=[&](std::wstring& s,uint32_t bytes){s.resize(bytes/sizeof(wchar_t));in.read(reinterpret_cast<char*>(s.data()),bytes);};readWide(r.user,h.userBytes);readWide(r.domain,h.domainBytes);readWide(r.sid,h.sidBytes);r.protectedAuthKey.resize(h.authBytes);r.ciphertext.resize(h.cipherBytes);in.read(reinterpret_cast<char*>(r.protectedAuthKey.data()),h.authBytes);in.read(reinterpret_cast<char*>(r.ciphertext.data()),h.cipherBytes);if(!in){error=L"Truncated credentials.dat";return false;}r.deviceId=h.deviceId;r.cardId=h.cardId;r.cardSource=h.cardSource;r.iv=h.iv;r.tag=h.tag;return true;
}
bool UnprotectAuthKey(const CredentialRecord& r,Key32& key){auto entropy=Entropy(r);DATA_BLOB input{static_cast<DWORD>(r.protectedAuthKey.size()),const_cast<BYTE*>(r.protectedAuthKey.data())},extra{static_cast<DWORD>(entropy.size()),entropy.data()},output{};if(!CryptUnprotectData(&input,nullptr,&extra,nullptr,nullptr,CRYPTPROTECT_UI_FORBIDDEN,&output)||output.cbData!=key.size()){if(output.pbData){SecureZeroMemory(output.pbData,output.cbData);LocalFree(output.pbData);}return false;}std::copy_n(output.pbData,key.size(),key.begin());SecureZeroMemory(output.pbData,output.cbData);LocalFree(output.pbData);return true;}
bool DecryptPassword(const CredentialRecord& r,const Key32& wrap,SecureVector<wchar_t>& password){auto aad=Aad(r);std::vector<uint8_t> plain;if(!AesGcmDecrypt(wrap,r.ciphertext,aad,r.iv,r.tag,plain)||plain.size()%sizeof(wchar_t)){if(!plain.empty())SecureZeroMemory(plain.data(),plain.size());return false;}password.resize(plain.size()/sizeof(wchar_t)+1);memcpy(password.data(),plain.data(),plain.size());password.data()[password.size()-1]=L'\0';SecureZeroMemory(plain.data(),plain.size());return true;}
}
