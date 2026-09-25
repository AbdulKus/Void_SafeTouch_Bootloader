#include "crypto.h"
#include "safetouch_protocol.h"
#include <windows.h>
#include <bcrypt.h>
#include <algorithm>
#include <cstring>

namespace safetouch {
namespace {
struct AlgHandle { BCRYPT_ALG_HANDLE value{}; ~AlgHandle(){ if(value) BCryptCloseAlgorithmProvider(value,0); } };
struct HashHandle { BCRYPT_HASH_HANDLE value{}; ~HashHandle(){ if(value) BCryptDestroyHash(value); } };
struct KeyHandle { BCRYPT_KEY_HANDLE value{}; ~KeyHandle(){ if(value) BCryptDestroyKey(value); } };
bool Ok(NTSTATUS status) { return status >= 0; }
bool Hash(bool hmac, std::span<const uint8_t> key, std::span<const uint8_t> data, Key32& output) {
    AlgHandle alg; HashHandle hash;
    if(!Ok(BCryptOpenAlgorithmProvider(&alg.value,BCRYPT_SHA256_ALGORITHM,nullptr,hmac?BCRYPT_ALG_HANDLE_HMAC_FLAG:0))) return false;
    PUCHAR keyData=hmac?const_cast<PUCHAR>(key.data()):nullptr; ULONG keySize=hmac?static_cast<ULONG>(key.size()):0;
    if(!Ok(BCryptCreateHash(alg.value,&hash.value,nullptr,0,keyData,keySize,0))) return false;
    if(!data.empty()&&!Ok(BCryptHashData(hash.value,const_cast<PUCHAR>(data.data()),static_cast<ULONG>(data.size()),0))) return false;
    return Ok(BCryptFinishHash(hash.value,output.data(),static_cast<ULONG>(output.size()),0));
}
bool Aes(bool encrypt,const Key32& key,std::span<const uint8_t> input,std::span<const uint8_t> aad,
         const std::array<uint8_t,12>& iv,std::vector<uint8_t>& output,std::array<uint8_t,16>& tag) {
    AlgHandle alg; KeyHandle handle;
    if(!Ok(BCryptOpenAlgorithmProvider(&alg.value,BCRYPT_AES_ALGORITHM,nullptr,0))) return false;
    if(!Ok(BCryptSetProperty(alg.value,BCRYPT_CHAINING_MODE,
        reinterpret_cast<PUCHAR>(const_cast<wchar_t*>(BCRYPT_CHAIN_MODE_GCM)),sizeof(BCRYPT_CHAIN_MODE_GCM),0))) return false;
    if(!Ok(BCryptGenerateSymmetricKey(alg.value,&handle.value,nullptr,0,const_cast<PUCHAR>(key.data()),static_cast<ULONG>(key.size()),0))) return false;
    BCRYPT_AUTHENTICATED_CIPHER_MODE_INFO info; BCRYPT_INIT_AUTH_MODE_INFO(info);
    info.pbNonce=const_cast<PUCHAR>(iv.data());info.cbNonce=static_cast<ULONG>(iv.size());
    info.pbAuthData=aad.empty()?nullptr:const_cast<PUCHAR>(aad.data());info.cbAuthData=static_cast<ULONG>(aad.size());
    info.pbTag=tag.data();info.cbTag=static_cast<ULONG>(tag.size());
    output.resize(input.size());ULONG written=0;
    NTSTATUS status=encrypt?
      BCryptEncrypt(handle.value,const_cast<PUCHAR>(input.data()),static_cast<ULONG>(input.size()),&info,nullptr,0,output.data(),static_cast<ULONG>(output.size()),&written,0):
      BCryptDecrypt(handle.value,const_cast<PUCHAR>(input.data()),static_cast<ULONG>(input.size()),&info,nullptr,0,output.data(),static_cast<ULONG>(output.size()),&written,0);
    if(!Ok(status)){if(!output.empty())SecureZeroMemory(output.data(),output.size());output.clear();return false;}output.resize(written);return true;
}
}
bool RandomBytes(std::span<uint8_t> output){return Ok(BCryptGenRandom(nullptr,output.data(),static_cast<ULONG>(output.size()),BCRYPT_USE_SYSTEM_PREFERRED_RNG));}
bool Sha256(std::span<const uint8_t> data,Key32& output){return Hash(false,{},data,output);}
bool HmacSha256(std::span<const uint8_t> key,std::span<const uint8_t> data,Key32& output){return Hash(true,key,data,output);}
bool ConstantTimeEqual(std::span<const uint8_t> a,std::span<const uint8_t> b){if(a.size()!=b.size())return false;uint8_t d=0;for(size_t i=0;i<a.size();++i)d|=a[i]^b[i];return d==0;}
bool DeriveDeviceKeys(const Key32& secret,Key32& auth,Key32& wrap,Id16& id){
    const auto bytes=[](const char* s){return std::span<const uint8_t>(reinterpret_cast<const uint8_t*>(s),strlen(s));};Key32 full{};
    if(!HmacSha256(secret,bytes(ST_LABEL_AUTH_KEY),auth)||!HmacSha256(secret,bytes(ST_LABEL_WRAP_KEY),wrap))return false;
    std::vector<uint8_t> material(secret.begin(),secret.end());material.insert(material.end(),ST_LABEL_DEVICE_ID,ST_LABEL_DEVICE_ID+strlen(ST_LABEL_DEVICE_ID));
    bool ok=Sha256(material,full);if(ok)std::copy_n(full.begin(),id.size(),id.begin());SecureZeroMemory(full.data(),full.size());SecureZeroMemory(material.data(),material.size());return ok;
}
bool ComputeAuthProof(const Key32& auth,const Key32& nonce,const Id16& card,const Id16& device,std::array<uint8_t,16>& proof){
    std::vector<uint8_t> data(reinterpret_cast<const uint8_t*>(ST_LABEL_AUTH_PROOF),reinterpret_cast<const uint8_t*>(ST_LABEL_AUTH_PROOF)+strlen(ST_LABEL_AUTH_PROOF));
    data.insert(data.end(),nonce.begin(),nonce.end());data.insert(data.end(),card.begin(),card.end());data.insert(data.end(),device.begin(),device.end());Key32 full{};
    bool ok=HmacSha256(auth,data,full);if(ok)std::copy_n(full.begin(),proof.size(),proof.begin());SecureZeroMemory(full.data(),full.size());return ok;
}
bool AesGcmEncrypt(const Key32& key,std::span<const uint8_t> plain,std::span<const uint8_t> aad,std::array<uint8_t,12>& iv,std::vector<uint8_t>& cipher,std::array<uint8_t,16>& tag){if(!RandomBytes(iv))return false;tag.fill(0);return Aes(true,key,plain,aad,iv,cipher,tag);}
bool AesGcmDecrypt(const Key32& key,std::span<const uint8_t> cipher,std::span<const uint8_t> aad,const std::array<uint8_t,12>& iv,const std::array<uint8_t,16>& inputTag,std::vector<uint8_t>& plain){auto tag=inputTag;return Aes(false,key,cipher,aad,iv,plain,tag);}
}
