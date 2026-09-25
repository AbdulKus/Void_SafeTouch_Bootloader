#include "provider.h"
#include "module.h"
#include <unknwn.h>
#include <atomic>
#include <new>

class Factory final : public IClassFactory {
public:
    Factory(){InterlockedIncrement(&g_objectCount);}~Factory(){InterlockedDecrement(&g_objectCount);}
    HRESULT STDMETHODCALLTYPE QueryInterface(REFIID id,void** value) override{if(!value)return E_POINTER;*value=nullptr;if(id==IID_IUnknown||id==IID_IClassFactory)*value=static_cast<IClassFactory*>(this);else return E_NOINTERFACE;AddRef();return S_OK;}
    ULONG STDMETHODCALLTYPE AddRef() override{return ++references_;}ULONG STDMETHODCALLTYPE Release() override{ULONG n=--references_;if(!n)delete this;return n;}
    HRESULT STDMETHODCALLTYPE CreateInstance(IUnknown* outer,REFIID id,void** value) override{if(outer)return CLASS_E_NOAGGREGATION;auto* provider=new(std::nothrow) SafeTouchProvider;if(!provider)return E_OUTOFMEMORY;HRESULT hr=provider->QueryInterface(id,value);provider->Release();return hr;}
    HRESULT STDMETHODCALLTYPE LockServer(BOOL lock) override{if(lock)InterlockedIncrement(&g_objectCount);else InterlockedDecrement(&g_objectCount);return S_OK;}
private:std::atomic<ULONG> references_{1};
};

HRESULT CreateClassFactory(REFIID id,void** value){auto* factory=new(std::nothrow) Factory;if(!factory)return E_OUTOFMEMORY;HRESULT hr=factory->QueryInterface(id,value);factory->Release();return hr;}
