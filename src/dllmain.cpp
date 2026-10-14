#include <Windows.h>
#include "PluginInterface.h"
#include "IMEStatusPlugin.h"

// 框架要求的导出函数：返回 ITMPlugin 实例指针（单例，不释放）
extern "C" __declspec(dllexport) ITMPlugin* TMPluginGetInstance()
{
    return &CIMEStatusPlugin::Instance();
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved)
{
    UNREFERENCED_PARAMETER(hModule);
    UNREFERENCED_PARAMETER(ul_reason_for_call);
    UNREFERENCED_PARAMETER(lpReserved);
    return TRUE;
}
