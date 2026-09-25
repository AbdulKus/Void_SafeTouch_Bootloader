#pragma once
#include "crypto.h"
#include "secure_buffer.h"
#include <array>
#include <filesystem>
#include <string>
#include <vector>

namespace safetouch {

struct CredentialRecord {
    std::wstring user;
    std::wstring domain;
    std::wstring sid;
    Id16 deviceId{};
    Id16 cardId{};
    uint32_t cardSource{};
    std::vector<uint8_t> protectedAuthKey;
    std::vector<uint8_t> ciphertext;
    std::array<uint8_t, 12> iv{};
    std::array<uint8_t, 16> tag{};
};

std::filesystem::path DefaultCredentialsPath();
bool SaveCredentialRecord(const std::filesystem::path& path, CredentialRecord& record,
                          const Key32& authKey, const Key32& wrapKey,
                          std::span<const wchar_t> password, std::wstring& error);
bool LoadCredentialRecord(const std::filesystem::path& path, CredentialRecord& record,
                          std::wstring& error);
bool UnprotectAuthKey(const CredentialRecord& record, Key32& authKey);
bool DecryptPassword(const CredentialRecord& record, const Key32& wrapKey,
                     SecureVector<wchar_t>& password);

}
