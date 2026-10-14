#include "IMEItem.h"
#include <Windows.h>
#include <imm.h>
#include <cstdarg>
#pragma comment(lib, "imm32.lib")

// 这两个 WM_IME_CONTROL 的 wParam 值按 IMM32 约定存在，但 SDK 头文件未公开定义
#ifndef IMC_GETCONVERSIONMODE
#define IMC_GETCONVERSIONMODE   0x0001
#endif
#ifndef IMC_GETOPENSTATUS
#define IMC_GETOPENSTATUS       0x0005
#endif

namespace
{
    //------------------------------------------------------------------
    // 诊断日志：写到 %TEMP%\TrafficMonitorIME.log
    // 仅在查询结果发生变化时写一行，文件超过 256KB 自动清空
    //------------------------------------------------------------------
    std::wstring Format(const wchar_t* fmt, ...)
    {
        wchar_t buf[512]{};
        va_list ap;
        va_start(ap, fmt);
        _vsnwprintf_s(buf, _TRUNCATE, fmt, ap);
        va_end(ap);
        return std::wstring(buf);
    }

    void LogOnChange(const std::wstring& msg)
    {
        static std::wstring s_last;
        if (msg == s_last)
            return;
        s_last = msg;

        wchar_t dir[MAX_PATH]{};
        if (GetTempPathW(MAX_PATH, dir) == 0)
            return;
        std::wstring path = std::wstring(dir) + L"TrafficMonitorIME.log";

        WIN32_FILE_ATTRIBUTE_DATA fad{};
        if (GetFileAttributesExW(path.c_str(), GetFileExInfoStandard, &fad)
            && fad.nFileSizeLow > 256 * 1024)
        {
            DeleteFileW(path.c_str());
        }

        SYSTEMTIME st{};
        GetLocalTime(&st);

        FILE* f = nullptr;
        if (_wfopen_s(&f, path.c_str(), L"a, ccs=UTF-8") != 0 || f == nullptr)
            return;
        fwprintf(f, L"[%02d:%02d:%02d] %s\r\n", st.wHour, st.wMinute, st.wSecond, msg.c_str());
        fclose(f);
    }

    struct ImeQuery
    {
        bool valid = false;             // 本次是否成功取到 IME 状态
        bool chinese = false;           // 是否处于中文输入态
        const wchar_t* via = L"none";   // 通过哪条路径取到的
        std::wstring detail;            // 诊断细节
    };

    // 取“真正拥有焦点”的窗口：前台线程里可能是个编辑框子窗口
    HWND FindFocusWindow(HWND hFore, DWORD fgTid)
    {
        GUITHREADINFO gti{};
        gti.cbSize = sizeof(GUITHREADINFO);
        if (GetGUIThreadInfo(fgTid, &gti) && gti.hwndFocus != nullptr)
            return gti.hwndFocus;
        return hFore;
    }

    // 路径 1：给默认 IME 窗口发 WM_IME_CONTROL。
    // 走 Windows 消息机制，跨进程安全，且不会改动目标线程的输入状态。
    bool TryViaImeWindow(HWND hWnd, ImeQuery& r)
    {
        HWND hImeWnd = ImmGetDefaultIMEWnd(hWnd);
        if (hImeWnd == nullptr)
            return false;

        DWORD_PTR open = 0, conv = 0;
        LRESULT ok1 = SendMessageTimeout(hImeWnd, WM_IME_CONTROL, IMC_GETOPENSTATUS, 0,
                                         SMTO_ABORTIFHUNG | SMTO_BLOCK, 200, &open);
        LRESULT ok2 = SendMessageTimeout(hImeWnd, WM_IME_CONTROL, IMC_GETCONVERSIONMODE, 0,
                                         SMTO_ABORTIFHUNG | SMTO_BLOCK, 200, &conv);
        if (ok1 == 0 && ok2 == 0)
            return false;

        r.valid = true;
        r.chinese = (open != 0) && ((conv & IME_CMODE_NATIVE) != 0);
        r.via = L"WM_IME_CONTROL";
        r.detail = Format(L"open=%d conv=0x%04X", (int)open, (unsigned)conv);
        return true;
    }

    // 路径 2：AttachThreadInput 到前台线程后再 ImmGetContext。
    // 目标窗口与调用者同进程/同线程时最准确。
    bool TryViaImmContext(HWND hWnd, DWORD fgTid, ImeQuery& r)
    {
        DWORD curTid = GetCurrentThreadId();
        bool attached = false;
        if (fgTid != curTid)
            attached = (AttachThreadInput(curTid, fgTid, TRUE) != FALSE);

        HIMC hImc = ImmGetContext(hWnd);
        if (hImc == nullptr)
        {
            if (attached)
                AttachThreadInput(curTid, fgTid, FALSE);
            return false;
        }

        BOOL open = ImmGetOpenStatus(hImc);
        DWORD conv = 0, sent = 0;
        BOOL getOk = ImmGetConversionStatus(hImc, &conv, &sent);
        ImmReleaseContext(hWnd, hImc);

        if (attached)
            AttachThreadInput(curTid, fgTid, FALSE);

        if (!getOk)
            return false;

        r.valid = true;
        r.chinese = (open != FALSE) && ((conv & IME_CMODE_NATIVE) != 0);
        r.via = L"ImmGetContext";
        r.detail = Format(L"open=%d conv=0x%04X", (int)open, (unsigned)conv);
        return true;
    }

    ImeQuery QueryImeState()
    {
        ImeQuery r;

        HWND hFore = GetForegroundWindow();
        if (hFore == nullptr)
        {
            r.detail = L"GetForegroundWindow=null";
            return r;
        }

        DWORD fgTid = GetWindowThreadProcessId(hFore, nullptr);
        HWND hFocus = FindFocusWindow(hFore, fgTid);

        HKL hkl = GetKeyboardLayout(fgTid);
        r.detail = Format(L"lang=0x%04X", (unsigned)LOWORD((DWORD_PTR)hkl));

        HWND targets[2] = { hFocus, hFore };
        for (int i = 0; i < 2; ++i)
        {
            if (targets[i] == nullptr)
                continue;

            ImeQuery t;
            if (TryViaImeWindow(targets[i], t))
            {
                t.detail += L" | " + r.detail;
                return t;
            }
        }

        for (int i = 0; i < 2; ++i)
        {
            if (targets[i] == nullptr)
                continue;

            ImeQuery t;
            if (TryViaImmContext(targets[i], fgTid, t))
            {
                t.detail += L" | " + r.detail;
                return t;
            }
        }

        r.detail = L"all paths failed | " + r.detail;
        return r;
    }
}

CIMEItem::CIMEItem()
{
    m_value = L"英";
}

CIMEItem::~CIMEItem()
{
}

const wchar_t* CIMEItem::GetItemName() const
{
    return L"输入法状态";
}

const wchar_t* CIMEItem::GetItemId() const
{
    // 唯一 ID，只能含字母和数字
    return L"IMEStatus";
}

const wchar_t* CIMEItem::GetItemLableText() const
{
    // IsCustomDraw 为 true 时主程序会忽略此返回值，这里仅作占位
    return L"输入法";
}

const wchar_t* CIMEItem::GetItemValueText() const
{
    return m_value.c_str();
}

const wchar_t* CIMEItem::GetItemValueSampleText() const
{
    // 用于计算显示宽度 / 皮肤预览，返回最长可能的文本即可
    return L"中";
}

bool CIMEItem::IsCustomDraw() const
{
    // 返回 true：由插件自己用 GDI 绘制显示区域
    return true;
}

int CIMEItem::GetItemWidth() const
{
    // DPI=96(100%) 时的最小宽度，主程序会按系统 DPI 自动放大
    return 24;
}

void CIMEItem::Update()
{
    ImeQuery q = QueryImeState();

    // 关键：IME 状态是按“前台窗口线程”隔离的，而且 ImmGetContext 对别的进程的
    // 窗口会返回 NULL，所以这里用两条路径依次尝试，任一成功即采用。
    if (q.valid)
        m_isChinese = q.chinese;
    // 两条路径都失败时保留上一次的结果，避免状态来回跳

    m_value = m_isChinese ? L"中" : L"英";

    LogOnChange(Format(L"valid=%d queried=%d shown=%s via=%s | %s",
                       q.valid ? 1 : 0, q.chinese ? 1 : 0,
                       m_value.c_str(), q.via, q.detail.c_str()));
}

void CIMEItem::DrawItem(void* hDC, int x, int y, int w, int h, bool dark_mode)
{
    HDC dc = (HDC)hDC;
    if (dc == NULL)
        return;

    // 不绘制背景色块，只显示“中 / 英”文字。背景由主程序统一处理。
    // 文字配色仍适配深色 / 浅色模式，并保证在各自背景下清晰可见：
    //   中文用红色（提示已进入中文输入态），英文用中性灰。
    COLORREF fg = m_isChinese
        ? (dark_mode ? RGB(255, 120, 120) : RGB(200, 40, 40))
        : (dark_mode ? RGB(215, 215, 215) : RGB(60, 60, 60));

    // 画文字（透明背景）
    RECT rc = { x, y, x + w, y + h };
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, fg);
    DrawText(dc, m_value.c_str(), -1, &rc, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
}
