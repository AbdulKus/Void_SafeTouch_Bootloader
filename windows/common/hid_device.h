#pragma once
#include <windows.h>
#include <array>
#include <cstdint>
#include <string>

namespace safetouch {

class HidDevice {
public:
    HidDevice() = default;
    ~HidDevice();
    HidDevice(const HidDevice&) = delete;
    HidDevice& operator=(const HidDevice&) = delete;
    bool Open(std::wstring& error);
    void Close();
    bool Command(const std::array<uint8_t,64>& request,std::array<uint8_t,64>& reply,DWORD timeoutMs,std::wstring& error);
private:
    HANDLE handle_{INVALID_HANDLE_VALUE};
};

}
