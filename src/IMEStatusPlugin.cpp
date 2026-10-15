#include "IMEStatusPlugin.h"
#include "OptionsDialog.h"
#include <Windows.h>

namespace
{
    // 取本 DLL 自身的模块句柄（窗口类要注册在本模块里）
    HINSTANCE GetSelfModule()
    {
        HMODULE mod = nullptr;
        ::GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                             reinterpret_cast<LPCWSTR>(&GetSelfModule), &mod);
        return reinterpret_cast<HINSTANCE>(mod);
    }

    const wchar_t* const kHiddenClass = L"TMIME_HiddenPollWindow";
}

CIMEStatusPlugin::CIMEStatusPlugin()
{
}

CIMEStatusPlugin::~CIMEStatusPlugin()
{
    StopRealtimePoll();
    if (m_hiddenWnd != nullptr)
    {
        ::DestroyWindow(m_hiddenWnd);
        m_hiddenWnd = nullptr;
    }
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
    case TMI_URL:         return L"https://github.com/HumerBen/TrafficMonitorIME";
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

    // 载入用户之前保存的颜色、字体与刷新行为设置
    m_item.LoadAppearance(GetConfigDir());
    m_item.SetApp(m_app);

    // 按配置启动后台实时刷新（默认开启）
    if (m_item.GetAppearance().realtimePoll)
        StartRealtimePoll();
}

ITMPlugin::OptionReturn CIMEStatusPlugin::ShowOptionsDialog(void* hParent)
{
    IMEAppearance cfg = m_item.GetAppearance();

    if (!ShowIMEColorOptions(static_cast<HWND>(hParent), cfg))
        return OR_OPTION_UNCHANGED;   // 用户取消，外观保持原样

    m_item.SetAppearance(cfg);
    m_item.SaveAppearance(GetConfigDir());

    // 刷新行为可能发生变化，按新配置重启后台实时刷新
    StopRealtimePoll();
    if (m_item.GetAppearance().realtimePoll)
        StartRealtimePoll();

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

//------------------------------------------------------------------
// 后台实时刷新：用 message-only 隐藏窗口接收 WM_TIMER，在主线程派发，
// 因此调用 InvalidateRect 重绘主窗口是线程安全的。
//------------------------------------------------------------------
void CIMEStatusPlugin::StartRealtimePoll()
{
    if (m_hiddenWnd == nullptr)
    {
        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = HiddenWndProc;
        wc.hInstance = GetSelfModule();
        wc.lpszClassName = kHiddenClass;

        if (::RegisterClassExW(&wc) == 0 && ::GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
            return;

        m_hiddenWnd = ::CreateWindowExW(0, kHiddenClass, L"", 0,
                                        0, 0, 0, 0, HWND_MESSAGE, nullptr,
                                        GetSelfModule(), nullptr);
        if (m_hiddenWnd == nullptr)
            return;

        // 把 this 存进窗口 USERDATA，供 WndProc 取回
        ::SetWindowLongPtrW(m_hiddenWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));
    }

    if (m_timerId != 0)
        ::KillTimer(m_hiddenWnd, m_timerId);

    const UINT ms = static_cast<UINT>(m_item.GetAppearance().pollIntervalMs);
    m_timerId = ::SetTimer(m_hiddenWnd, 1, ms, nullptr);
}

void CIMEStatusPlugin::StopRealtimePoll()
{
    if (m_hiddenWnd != nullptr && m_timerId != 0)
        ::KillTimer(m_hiddenWnd, m_timerId);
    m_timerId = 0;
}

LRESULT CALLBACK CIMEStatusPlugin::HiddenWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    if (msg == WM_TIMER)
    {
        CIMEStatusPlugin* self = reinterpret_cast<CIMEStatusPlugin*>(
            ::GetWindowLongPtrW(hWnd, GWLP_USERDATA));
        if (self != nullptr && self->m_item.Update())
        {
            // 输入法状态发生变化，主动让主程序重绘（与 ShowOptionsDialog 同一手段）。
            // 此回调跑在主线程，调用 InvalidateRect 到主程序窗口是安全的。
            if (self->m_app != nullptr && self->m_app->GetAPIVersion() >= 1)
            {
                HWND hwnds[2] = { static_cast<HWND>(self->m_app->GetMainWindowHwnd()),
                                  static_cast<HWND>(self->m_app->GetTaskbarWindowHwnd()) };
                for (HWND h : hwnds)
                {
                    if (h != nullptr)
                        ::InvalidateRect(h, nullptr, TRUE);
                }
            }
        }
        return 0;
    }
    return ::DefWindowProcW(hWnd, msg, wParam, lParam);
}
