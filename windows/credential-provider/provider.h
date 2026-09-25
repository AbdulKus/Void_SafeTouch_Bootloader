#pragma once
#include <windows.h>
#include <credentialprovider.h>
#include <atomic>
#include <mutex>

class SafeTouchCredential;

class SafeTouchProvider final : public ICredentialProvider {
public:
    SafeTouchProvider();
    ~SafeTouchProvider();
    IFACEMETHODIMP QueryInterface(REFIID riid, void** value) override;
    IFACEMETHODIMP_(ULONG) AddRef() override;
    IFACEMETHODIMP_(ULONG) Release() override;
    IFACEMETHODIMP SetUsageScenario(CREDENTIAL_PROVIDER_USAGE_SCENARIO cpus,DWORD flags) override;
    IFACEMETHODIMP SetSerialization(const CREDENTIAL_PROVIDER_CREDENTIAL_SERIALIZATION*) override;
    IFACEMETHODIMP Advise(ICredentialProviderEvents* events,UINT_PTR context) override;
    IFACEMETHODIMP UnAdvise() override;
    IFACEMETHODIMP GetFieldDescriptorCount(DWORD* count) override;
    IFACEMETHODIMP GetFieldDescriptorAt(DWORD index,CREDENTIAL_PROVIDER_FIELD_DESCRIPTOR** descriptor) override;
    IFACEMETHODIMP GetCredentialCount(DWORD* count,DWORD* defaultIndex,BOOL* autoLogon) override;
    IFACEMETHODIMP GetCredentialAt(DWORD index,ICredentialProviderCredential** credential) override;
    void CredentialBecameActive();
    void AuthenticationReady();
    CREDENTIAL_PROVIDER_USAGE_SCENARIO Scenario() const { return scenario_; }
private:
    std::atomic<ULONG> references_{1};
    CREDENTIAL_PROVIDER_USAGE_SCENARIO scenario_{CPUS_INVALID};
    SafeTouchCredential* credential_{};
    std::mutex eventsMutex_;
    ICredentialProviderEvents* events_{};
    UINT_PTR adviseContext_{};
};
