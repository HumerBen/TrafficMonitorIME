#include "IMEStatusPlugin.h"
#include "OptionsDialog.h"

CIMEStatusPlugin::CIMEStatusPlugin()
{
}

CIMEStatusPlugin::~CIMEStatusPlugin()
{
}

CIMEStatusPlugin& CIMEStatusPlugin::Instance()
{
    static CIMEStatusPlugin s_instance;
    return s_instance;
}

IPluginItem* CIMEStatusPlugin::GetItem(int index)
{
    if (index == 0)
        return &m_item;   // 第 0 个项目
    return nullptr;       // 其他 index 必须返回空指针
}

void CIMEStatusPlugin::DataRequired()
{
    // 框架每隔一段时间调用一次，这里刷新一次输入法状态
    m_item.Update();
}

const wchar_t* CIMEStatusPlugin::GetInfo(PluginInfoIndex index)
{
    switch (index)
    {
    case TMI_NAME:        return L"输入法状态";
    case TMI_DESCRIPTION: return L"在任务栏显示当前前台窗口的输入法状态（中 / 英）";
    case TMI_AUTHOR:      return L"Ben";
    case TMI_COPYRIGHT:   return L"Copyright (C) 2026";
    case TMI_VERSION:     return L"V2026.10.14.1";
    case TMI_URL:         return L"";
    default:              return L"";
    }
}

std::wstring CIMEStatusPlugin::GetConfigDir() const
{
    if (m_app != nullptr)
    {
        const wchar_t* dir = m_app->GetPluginConfigDir();
        if (dir != nullptr && dir[0] != L'\0')
            return dir;
    }

    // 回退：主程序接口不可用时，退到 TrafficMonitor.exe 所在目录
    wchar_t path[MAX_PATH]{};
    const DWORD len = ::GetModuleFileNameW(nullptr, path, MAX_PATH);
    if (len > 0 && len < MAX_PATH)
    {
        const std::wstring exePath(path, len);
        const size_t pos = exePath.find_last_of(L"\\/");
        if (pos != std::wstring::npos)
            return exePath.substr(0, pos);
    }
    return std::wstring();
}

void CIMEStatusPlugin::OnInitialize(ITrafficMonitor* pApp)
{
    m_app = pApp;

    // 载入用户之前保存的颜色与字体设置
    m_item.LoadAppearance(GetConfigDir());
}

ITMPlugin::OptionReturn CIMEStatusPlugin::ShowOptionsDialog(void* hParent)
{
    IMEAppearance cfg = m_item.GetAppearance();

    if (!ShowIMEColorOptions(static_cast<HWND>(hParent), cfg))
        return OR_OPTION_UNCHANGED;   // 用户取消，外观保持原样

    m_item.SetAppearance(cfg);
    m_item.SaveAppearance(GetConfigDir());

    // 请主程序立刻用新外观重绘显示区域。
    // GetMainWindowHwnd/GetTaskbarWindowHwnd 是接口 version 1 才有的，先判断版本。
    if (m_app != nullptr && m_app->GetAPIVersion() >= 1)
    {
        HWND hwnds[2] = { static_cast<HWND>(m_app->GetMainWindowHwnd()),
                          static_cast<HWND>(m_app->GetTaskbarWindowHwnd()) };
        for (HWND h : hwnds)
        {
            if (h != nullptr)
                ::InvalidateRect(h, nullptr, TRUE);
        }
    }

    return OR_OPTION_CHANGED;
}
