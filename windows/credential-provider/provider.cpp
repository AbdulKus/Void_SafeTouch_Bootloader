#include "provider.h"
#include "credential.h"
#include "module.h"
#include <shlwapi.h>
#include <shlguid.h>
#include <new>

namespace {
enum Fields : DWORD { FIELD_TITLE,FIELD_STATUS,FIELD_RETRY,FIELD_COUNT };
const CREDENTIAL_PROVIDER_FIELD_DESCRIPTOR descriptors[FIELD_COUNT]={
    {FIELD_TITLE,CPFT_LARGE_TEXT,const_cast<PWSTR>(L"SafeTouch"),CPFG_CREDENTIAL_PROVIDER_LABEL},
    {FIELD_STATUS,CPFT_SMALL_TEXT,const_cast<PWSTR>(L"Status"),GUID_NULL},
    {FIELD_RETRY,CPFT_COMMAND_LINK,const_cast<PWSTR>(L"Retry"),GUID_NULL}
};
HRESULT CopyDescriptor(const CREDENTIAL_PROVIDER_FIELD_DESCRIPTOR& in,CREDENTIAL_PROVIDER_FIELD_DESCRIPTOR** out){*out=static_cast<CREDENTIAL_PROVIDER_FIELD_DESCRIPTOR*>(CoTaskMemAlloc(sizeof(**out)));if(!*out)return E_OUTOFMEMORY;**out=in;HRESULT hr=SHStrDupW(in.pszLabel,&(*out)->pszLabel);if(FAILED(hr)){CoTaskMemFree(*out);*out=nullptr;}return hr;}
}
SafeTouchProvider::SafeTouchProvider(){InterlockedIncrement(&g_objectCount);credential_=new(std::nothrow) SafeTouchCredential(this);}
SafeTouchProvider::~SafeTouchProvider(){if(credential_)credential_->Release();if(events_)events_->Release();InterlockedDecrement(&g_objectCount);}
HRESULT SafeTouchProvider::QueryInterface(REFIID id,void** value){if(!value)return E_POINTER;*value=nullptr;if(id==IID_IUnknown||id==IID_ICredentialProvider)*value=static_cast<ICredentialProvider*>(this);else return E_NOINTERFACE;AddRef();return S_OK;}
ULONG SafeTouchProvider::AddRef(){return ++references_;}ULONG SafeTouchProvider::Release(){ULONG n=--references_;if(!n)delete this;return n;}
HRESULT SafeTouchProvider::SetUsageScenario(CREDENTIAL_PROVIDER_USAGE_SCENARIO cpus,DWORD){if(cpus!=CPUS_LOGON&&cpus!=CPUS_UNLOCK_WORKSTATION)return E_NOTIMPL;scenario_=cpus;return S_OK;}
HRESULT SafeTouchProvider::SetSerialization(const CREDENTIAL_PROVIDER_CREDENTIAL_SERIALIZATION*){return E_NOTIMPL;}
HRESULT SafeTouchProvider::Advise(ICredentialProviderEvents* events,UINT_PTR context){{std::lock_guard lock(eventsMutex_);if(events_)events_->Release();events_=events;adviseContext_=context;if(events_)events_->AddRef();}if(credential_)credential_->StartMonitoring();return S_OK;}
HRESULT SafeTouchProvider::UnAdvise(){{std::lock_guard lock(eventsMutex_);if(events_){events_->Release();events_=nullptr;}}if(credential_)credential_->StopMonitoring();return S_OK;}
HRESULT SafeTouchProvider::GetFieldDescriptorCount(DWORD* count){if(!count)return E_POINTER;*count=FIELD_COUNT;return S_OK;}
HRESULT SafeTouchProvider::GetFieldDescriptorAt(DWORD index,CREDENTIAL_PROVIDER_FIELD_DESCRIPTOR** value){if(index>=FIELD_COUNT)return E_INVALIDARG;return CopyDescriptor(descriptors[index],value);}
HRESULT SafeTouchProvider::GetCredentialCount(DWORD* count,DWORD* defaultIndex,BOOL* autoLogon){if(!count||!defaultIndex||!autoLogon)return E_POINTER;*count=credential_?1:0;*defaultIndex=credential_&&(credential_->WantsDefault()||credential_->Ready())?0:CREDENTIAL_PROVIDER_NO_DEFAULT;*autoLogon=credential_&&credential_->Ready();return S_OK;}
HRESULT SafeTouchProvider::GetCredentialAt(DWORD index,ICredentialProviderCredential** value){if(!value)return E_POINTER;*value=nullptr;if(index||!credential_)return E_INVALIDARG;return credential_->QueryInterface(IID_PPV_ARGS(value));}
void SafeTouchProvider::CredentialBecameActive(){AuthenticationReady();}
void SafeTouchProvider::AuthenticationReady(){ICredentialProviderEvents* callback=nullptr;UINT_PTR context=0;{std::lock_guard lock(eventsMutex_);if(events_){callback=events_;callback->AddRef();context=adviseContext_;}}if(callback){callback->CredentialsChanged(context);callback->Release();}}
