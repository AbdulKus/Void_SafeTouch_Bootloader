#include "hid_device.h"
#include "safetouch_protocol.h"
#include <hidsdi.h>
#include <setupapi.h>
#include <vector>

namespace safetouch {
HidDevice::~HidDevice(){Close();}
void HidDevice::Close(){if(handle_!=INVALID_HANDLE_VALUE){CloseHandle(handle_);handle_=INVALID_HANDLE_VALUE;}}
bool HidDevice::Open(std::wstring& error){Close();GUID guid{};HidD_GetHidGuid(&guid);HDEVINFO devices=SetupDiGetClassDevsW(&guid,nullptr,nullptr,DIGCF_PRESENT|DIGCF_DEVICEINTERFACE);if(devices==INVALID_HANDLE_VALUE){error=L"SetupDiGetClassDevs failed";return false;}
    SP_DEVICE_INTERFACE_DATA interfaceData{sizeof(interfaceData)};for(DWORD index=0;SetupDiEnumDeviceInterfaces(devices,nullptr,&guid,index,&interfaceData);++index){DWORD needed=0;SetupDiGetDeviceInterfaceDetailW(devices,&interfaceData,nullptr,0,&needed,nullptr);std::vector<uint8_t> storage(needed);auto* detail=reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(storage.data());detail->cbSize=sizeof(*detail);if(!SetupDiGetDeviceInterfaceDetailW(devices,&interfaceData,detail,needed,nullptr,nullptr))continue;
        HANDLE candidate=CreateFileW(detail->DevicePath,GENERIC_READ|GENERIC_WRITE,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_EXISTING,FILE_FLAG_OVERLAPPED,nullptr);if(candidate==INVALID_HANDLE_VALUE)continue;HIDD_ATTRIBUTES attributes{sizeof(attributes)};if(HidD_GetAttributes(candidate,&attributes)&&attributes.VendorID==ST_USB_VID&&attributes.ProductID==ST_USB_PID_LOGIN){handle_=candidate;SetupDiDestroyDeviceInfoList(devices);return true;}CloseHandle(candidate);}
    SetupDiDestroyDeviceInfoList(devices);error=L"SafeTouch Login HID (1209:B008) not found";return false;}
bool HidDevice::Command(const std::array<uint8_t,64>& request,std::array<uint8_t,64>& reply,DWORD timeout,std::wstring& error){if(handle_==INVALID_HANDLE_VALUE&&!Open(error))return false;std::array<uint8_t,65> output{};std::copy(request.begin(),request.end(),output.begin()+1);OVERLAPPED write{};write.hEvent=CreateEventW(nullptr,TRUE,FALSE,nullptr);DWORD transferred=0;BOOL started=WriteFile(handle_,output.data(),static_cast<DWORD>(output.size()),&transferred,&write);if(!started&&GetLastError()!=ERROR_IO_PENDING){CloseHandle(write.hEvent);error=L"HID write failed";return false;}if(!started){DWORD wait=WaitForSingleObject(write.hEvent,timeout);if(wait!=WAIT_OBJECT_0||!GetOverlappedResult(handle_,&write,&transferred,FALSE)){CancelIoEx(handle_,&write);CloseHandle(write.hEvent);error=L"HID write timed out";return false;}}CloseHandle(write.hEvent);
    OVERLAPPED read{};read.hEvent=CreateEventW(nullptr,TRUE,FALSE,nullptr);std::array<uint8_t,65> input{};started=ReadFile(handle_,input.data(),static_cast<DWORD>(input.size()),&transferred,&read);if(!started&&GetLastError()!=ERROR_IO_PENDING){CloseHandle(read.hEvent);error=L"HID read failed";return false;}if(!started){DWORD wait=WaitForSingleObject(read.hEvent,timeout);if(wait!=WAIT_OBJECT_0||!GetOverlappedResult(handle_,&read,&transferred,FALSE)){CancelIoEx(handle_,&read);CloseHandle(read.hEvent);error=L"HID response timed out";return false;}}CloseHandle(read.hEvent);
    size_t offset=transferred==65?1:0;if(transferred-offset<64){error=L"Short HID response";return false;}std::copy_n(input.begin()+offset,64,reply.begin());if(reply[0]!=request[0]){error=L"Mismatched HID response";return false;}return true;}
}
