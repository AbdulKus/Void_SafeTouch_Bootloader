#include "guids.h"
#include "module.h"
#include <windows.h>

HMODULE g_module{};volatile long g_objectCount{};
HRESULT CreateClassFactory(REFIID,void**);
BOOL WINAPI DllMain(HINSTANCE instance,DWORD reason,LPVOID){if(reason==DLL_PROCESS_ATTACH){g_module=instance;DisableThreadLibraryCalls(instance);}return TRUE;}
extern "C" HRESULT __declspec(dllexport) WINAPI DllCanUnloadNow(){return g_objectCount==0?S_OK:S_FALSE;}
extern "C" HRESULT __declspec(dllexport) WINAPI DllGetClassObject(REFCLSID clsid,REFIID id,void** value){return clsid==CLSID_SafeTouchProvider?CreateClassFactory(id,value):CLASS_E_CLASSNOTAVAILABLE;}
