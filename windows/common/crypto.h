#pragma once
#include <array>
#include <cstdint>
#include <span>
#include <vector>

namespace safetouch {

using Key32 = std::array<uint8_t, 32>;
using Id16 = std::array<uint8_t, 16>;

bool RandomBytes(std::span<uint8_t> output);
bool Sha256(std::span<const uint8_t> data, Key32& output);
bool HmacSha256(std::span<const uint8_t> key, std::span<const uint8_t> data, Key32& output);
bool ConstantTimeEqual(std::span<const uint8_t> left, std::span<const uint8_t> right);
bool DeriveDeviceKeys(const Key32& deviceSecret, Key32& authKey, Key32& wrapKey, Id16& deviceId);
bool ComputeAuthProof(const Key32& authKey, const Key32& nonce,
                      const Id16& cardId, const Id16& deviceId,
                      std::array<uint8_t, 16>& proof);
bool AesGcmEncrypt(const Key32& key, std::span<const uint8_t> plaintext,
                   std::span<const uint8_t> aad, std::array<uint8_t, 12>& iv,
                   std::vector<uint8_t>& ciphertext, std::array<uint8_t, 16>& tag);
bool AesGcmDecrypt(const Key32& key, std::span<const uint8_t> ciphertext,
                   std::span<const uint8_t> aad, const std::array<uint8_t, 12>& iv,
                   const std::array<uint8_t, 16>& tag, std::vector<uint8_t>& plaintext);

}
