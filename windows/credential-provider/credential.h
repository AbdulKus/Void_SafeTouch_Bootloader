#pragma once
#include "credentials.h"
#include <windows.h>
#include <credentialprovider.h>
#include <atomic>
#include <mutex>
#include <thread>

class SafeTouchProvider;

class SafeTouchCredential final : public ICredentialProviderCredential2 {
public:
    explicit SafeTouchCredential(SafeTouchProvider* provider);
    ~SafeTouchCredential() override;
    IFACEMETHODIMP QueryInterface(REFIID riid,void** value) override;
    IFACEMETHODIMP_(ULONG) AddRef() override;
    IFACEMETHODIMP_(ULONG) Release() override;
    IFACEMETHODIMP Advise(ICredentialProviderCredentialEvents* events,UINT_PTR context) override;
    IFACEMETHODIMP UnAdvise() override;
    IFACEMETHODIMP SetSelected(BOOL* autoLogon) override;
    IFACEMETHODIMP SetDeselected() override;
    IFACEMETHODIMP GetFieldState(DWORD id,CREDENTIAL_PROVIDER_FIELD_STATE* state,CREDENTIAL_PROVIDER_FIELD_INTERACTIVE_STATE* interactive) override;
    IFACEMETHODIMP GetStringValue(DWORD id,PWSTR* value) override;
    IFACEMETHODIMP GetBitmapValue(DWORD,HBITMAP*) override;
    IFACEMETHODIMP GetCheckboxValue(DWORD,BOOL*,PWSTR*) override;
    IFACEMETHODIMP GetSubmitButtonValue(DWORD,DWORD*) override;
    IFACEMETHODIMP GetComboBoxValueCount(DWORD,DWORD*,DWORD*) override;
    IFACEMETHODIMP GetComboBoxValueAt(DWORD,DWORD,PWSTR*) override;
    IFACEMETHODIMP SetStringValue(DWORD,PCWSTR) override;
    IFACEMETHODIMP SetCheckboxValue(DWORD,BOOL) override;
    IFACEMETHODIMP SetComboBoxSelectedValue(DWORD,DWORD) override;
    IFACEMETHODIMP CommandLinkClicked(DWORD id) override;
    IFACEMETHODIMP GetSerialization(CREDENTIAL_PROVIDER_GET_SERIALIZATION_RESPONSE* response,CREDENTIAL_PROVIDER_CREDENTIAL_SERIALIZATION* serialization,PWSTR* status,CREDENTIAL_PROVIDER_STATUS_ICON* icon) override;
    IFACEMETHODIMP ReportResult(NTSTATUS status,NTSTATUS substatus,PWSTR* text,CREDENTIAL_PROVIDER_STATUS_ICON* icon) override;
    IFACEMETHODIMP GetUserSid(PWSTR* sid) override;
    bool Ready() const { return ready_.load(); }
private:
    void Start();
    void Stop();
    void Worker();
    void SetStatus(const wchar_t* text);
    std::atomic<ULONG> references_{1};
    SafeTouchProvider* provider_;
    safetouch::CredentialRecord record_;
    std::wstring loadError_;
    std::wstring status_{L"Insert card"};
    std::mutex mutex_;
    ICredentialProviderCredentialEvents* events_{};
    UINT_PTR adviseContext_{};
    std::thread worker_;
    std::atomic_bool stop_{false},running_{false},ready_{false};
    safetouch::Key32 wrapKey_{};
};
