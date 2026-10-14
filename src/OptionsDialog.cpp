#include "OptionsDialog.h"
#include <commdlg.h>
#include <cstdio>
#include <string>

#pragma comment(lib, "comdlg32.lib")

//=============================================================================
// 本工程没有 .rc 资源文件，所以选项对话框完全用代码创建：
// 手工注册窗口类 -> 动态创建子控件 -> 自己跑模态消息循环。
//=============================================================================

namespace
{
    const wchar_t* const kWndClass = L"TMIME_OptionsDialog";

    // 96 DPI(100%) 下的客户区尺寸，实际会按系统 DPI 放大
    const int kClientW = 360;
    const int kClientH = 300;

    // 控件 ID
    enum : int
    {
        IDC_LBL_DARK_HEADER = 1001,
        IDC_LBL_LIGHT_HEADER,
        IDC_LBL_CHINESE,
        IDC_LBL_ENGLISH,
        IDC_LBL_PREVIEW,
        IDC_LBL_FONT,

        IDC_BTN_CHINESE_DARK = 1010,
        IDC_BTN_CHINESE_LIGHT,
        IDC_BTN_ENGLISH_DARK,
        IDC_BTN_ENGLISH_LIGHT,
        IDC_PREVIEW,
        IDC_BTN_RESTORE_DEFAULT = 1020,
        IDC_BTN_FONT,
    };

    struct DialogContext
    {
        IMEAppearance cfg;                  // 编辑中的外观（颜色 + 字体）
        bool       ok{ false };             // 是否点了“确定”
        HFONT      hFont{ nullptr };        // 对话框 UI 字体
        int        scale{ 100 };            // DPI 缩放百分比（96 DPI = 100）
        HWND       hColorBtns[4]{};         // 4 个颜色按钮
        HWND       hPreview{ nullptr };     // 预览区
        HWND       hFontBtn{ nullptr };     // “字体...”按钮
    };

    // 把 96 DPI 下的尺寸换算成当前 DPI 下的像素
    int Scale(const DialogContext* ctx, int value)
    {
        return ::MulDiv(value, ctx->scale, 100);
    }

    // 取本 DLL 自身的模块句柄（窗口类要注册在本模块里）
    HINSTANCE GetSelfModule()
    {
        HMODULE mod = nullptr;
        ::GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                             reinterpret_cast<LPCWSTR>(&GetSelfModule), &mod);
        return reinterpret_cast<HINSTANCE>(mod);
    }

    // GetDpiForWindow 只在 Win10 1607 之后才有，动态调用以便在老系统上也能运行
    UINT GetDpiForWindowSafe(HWND hWnd)
    {
        typedef UINT(WINAPI * PFN_GetDpiForWindow)(HWND);
        static PFN_GetDpiForWindow pfn = reinterpret_cast<PFN_GetDpiForWindow>(
            ::GetProcAddress(::GetModuleHandleW(L"user32.dll"), "GetDpiForWindow"));

        if (pfn != nullptr && hWnd != nullptr)
        {
            const UINT dpi = pfn(hWnd);
            if (dpi != 0)
                return dpi;
        }

        UINT dpi = 96;
        HDC dc = ::GetDC(nullptr);
        if (dc != nullptr)
        {
            const int v = ::GetDeviceCaps(dc, LOGPIXELSX);
            ::ReleaseDC(nullptr, dc);
            if (v > 0)
                dpi = static_cast<UINT>(v);
        }
        return dpi;
    }

    // 用系统的消息字体，保证和系统界面一致
    HFONT CreateUiFont()
    {
        NONCLIENTMETRICSW ncm{};
        ncm.cbSize = sizeof(ncm);
        if (::SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(ncm), &ncm, 0))
            return ::CreateFontIndirectW(&ncm.lfMessageFont);
        return nullptr;
    }

    // 按“字体名 + 字号(point)”生成一个 GDI 字体（fontSize<=0 时调用方应使用系统默认）
    HFONT MakeFont(HDC dc, const std::wstring& name, int sizePt)
    {
        const int dpiY = ::GetDeviceCaps(dc, LOGPIXELSY);
        LOGFONTW lf{};
        lf.lfHeight = -::MulDiv(sizePt, dpiY, 72);
        lf.lfCharSet = DEFAULT_CHARSET;
        lf.lfQuality = CLEARTYPE_QUALITY;
        lf.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
        ::wcscpy_s(lf.lfFaceName, name.empty() ? L"Microsoft YaHei" : name.c_str());
        return ::CreateFontIndirectW(&lf);
    }

    HWND CreateChild(HWND hParent, const wchar_t* cls, const wchar_t* text, DWORD style,
                     int x, int y, int w, int h, int id, const DialogContext* ctx)
    {
        return ::CreateWindowExW(0, cls, text, WS_CHILD | WS_VISIBLE | style,
                                 Scale(ctx, x), Scale(ctx, y), Scale(ctx, w), Scale(ctx, h),
                                 hParent, reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),
                                 GetSelfModule(), nullptr);
    }

    // 在色块中央写 #RRGGBB，文字用反色，这样任何底色上都看得清
    void DrawColorText(HDC dc, const RECT& rc, COLORREF color, HFONT hFont)
    {
        wchar_t buf[16]{};
        swprintf_s(buf, L"%02X%02X%02X", GetRValue(color), GetGValue(color), GetBValue(color));

        HGDIOBJ oldFont = nullptr;
        if (hFont != nullptr)
            oldFont = ::SelectObject(dc, hFont);

        const int oldMode = ::SetBkMode(dc, TRANSPARENT);
        const COLORREF oldColor = ::SetTextColor(dc, color ^ 0x00FFFFFF);

        RECT r = rc;
        ::DrawTextW(dc, buf, -1, &r, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

        ::SetTextColor(dc, oldColor);
        ::SetBkMode(dc, oldMode);
        if (oldFont != nullptr)
            ::SelectObject(dc, oldFont);
    }

    void DrawColorButton(const DRAWITEMSTRUCT* dis, COLORREF color, HFONT hFont)
    {
        HDC dc = dis->hDC;
        const RECT rc = dis->rcItem;

        ::FillRect(dc, &rc, ::GetSysColorBrush(COLOR_BTNFACE));

        RECT swatch = rc;
        ::InflateRect(&swatch, -2, -2);

        HBRUSH br = ::CreateSolidBrush(color);
        ::FillRect(dc, &swatch, br);
        ::DeleteObject(br);

        ::FrameRect(dc, &swatch, ::GetSysColorBrush(COLOR_3DDKSHADOW));

        if ((dis->itemState & ODS_SELECTED) != 0)
        {
            RECT focus = swatch;
            ::InflateRect(&focus, -1, -1);
            ::FrameRect(dc, &focus, ::GetSysColorBrush(COLOR_HIGHLIGHT));
        }
        else if ((dis->itemState & ODS_FOCUS) != 0)
        {
            RECT focus = rc;
            ::InflateRect(&focus, -1, -1);
            ::DrawFocusRect(dc, &focus);
        }

        DrawColorText(dc, swatch, color, hFont);
    }

    void DrawPreviewPair(HDC dc, const RECT& area, COLORREF chinese, COLORREF english)
    {
        const int mid = area.left + (area.right - area.left) / 2;
        RECT r1 = { area.left, area.top, mid, area.bottom };
        RECT r2 = { mid, area.top, area.right, area.bottom };

        ::SetTextColor(dc, chinese);
        ::DrawTextW(dc, L"中", -1, &r1, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        ::SetTextColor(dc, english);
        ::DrawTextW(dc, L"英", -1, &r2, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    }

    // 预览区左半边模拟深色模式、右半边模拟浅色模式，让用户直接看到实际效果
    void DrawPreview(const DRAWITEMSTRUCT* dis, const DialogContext* ctx)
    {
        HDC dc = dis->hDC;
        const RECT rc = dis->rcItem;

        ::FillRect(dc, &rc, ::GetSysColorBrush(COLOR_BTNFACE));
        ::FrameRect(dc, &rc, ::GetSysColorBrush(COLOR_3DDKSHADOW));

        RECT inner = rc;
        ::InflateRect(&inner, -1, -1);

        const int mid = inner.left + (inner.right - inner.left) / 2;
        RECT darkRect = { inner.left, inner.top, mid, inner.bottom };
        RECT lightRect = { mid, inner.top, inner.right, inner.bottom };

        HBRUSH darkBr = ::CreateSolidBrush(RGB(32, 32, 32));
        ::FillRect(dc, &darkRect, darkBr);
        ::DeleteObject(darkBr);

        HBRUSH lightBr = ::CreateSolidBrush(RGB(250, 250, 250));
        ::FillRect(dc, &lightRect, lightBr);
        ::DeleteObject(lightBr);

        HFONT drawFont = (ctx->cfg.fontSize > 0) ? MakeFont(dc, ctx->cfg.fontName, ctx->cfg.fontSize) : nullptr;
        HGDIOBJ oldFont = (drawFont != nullptr) ? ::SelectObject(dc, drawFont) : nullptr;
        const int oldMode = ::SetBkMode(dc, TRANSPARENT);

        DrawPreviewPair(dc, darkRect, ctx->cfg.colors.chinese_dark, ctx->cfg.colors.english_dark);
        DrawPreviewPair(dc, lightRect, ctx->cfg.colors.chinese_light, ctx->cfg.colors.english_light);

        ::SetBkMode(dc, oldMode);
        if (oldFont != nullptr)
            ::SelectObject(dc, oldFont);
        if (drawFont != nullptr)
            ::DeleteObject(drawFont);
    }

    bool PickColor(HWND hOwner, COLORREF& color)
    {
        static COLORREF customColors[16]{};

        CHOOSECOLORW cc{};
        cc.lStructSize = sizeof(cc);
        cc.hwndOwner = hOwner;
        cc.rgbResult = color;
        cc.lpCustColors = customColors;
        cc.Flags = CC_FULLOPEN | CC_RGBINIT;

        if (::ChooseColorW(&cc) == FALSE)
            return false;

        color = cc.rgbResult;
        return true;
    }

    // 调出系统字体对话框，让用户同时选择字体名和字号（point）
    bool PickFont(HWND hOwner, std::wstring& fontName, int& fontSize)
    {
        LOGFONTW lf{};
        const int initSize = (fontSize > 0) ? fontSize : 12;
        lf.lfHeight = -::MulDiv(initSize, 96, 72);
        lf.lfCharSet = DEFAULT_CHARSET;
        lf.lfPitchAndFamily = DEFAULT_PITCH | FF_DONTCARE;
        ::wcscpy_s(lf.lfFaceName, fontName.empty() ? L"Microsoft YaHei" : fontName.c_str());

        CHOOSEFONTW cf{};
        cf.lStructSize = sizeof(cf);
        cf.hwndOwner = hOwner;
        cf.lpLogFont = &lf;
        cf.Flags = CF_SCREENFONTS | CF_NOSCRIPTSEL | CF_INITTOLOGFONTSTRUCT;

        if (::ChooseFontW(&cf) == FALSE)
            return false;

        fontName = lf.lfFaceName;
        HDC dc = ::GetDC(nullptr);
        const int dpiY = (dc != nullptr) ? ::GetDeviceCaps(dc, LOGPIXELSY) : 96;
        if (dc != nullptr)
            ::ReleaseDC(nullptr, dc);
        fontSize = ::MulDiv(-lf.lfHeight, 72, dpiY);
        if (fontSize < 1)
            fontSize = 1;
        return true;
    }

    void UpdateFontButtonText(HWND hBtn, const std::wstring& fontName, int fontSize)
    {
        if (hBtn == nullptr)
            return;
        std::wstring text;
        if (fontSize <= 0 || fontName.empty())
            text = L"系统默认字体";
        else
            text = fontName + L"  " + std::to_wstring(fontSize) + L"pt";
        ::SetWindowTextW(hBtn, text.c_str());
    }

    void RefreshControls(const DialogContext* ctx)
    {
        for (HWND hBtn : ctx->hColorBtns)
        {
            if (hBtn != nullptr)
                ::InvalidateRect(hBtn, nullptr, FALSE);
        }
        if (ctx->hPreview != nullptr)
            ::InvalidateRect(ctx->hPreview, nullptr, FALSE);
    }

    LRESULT CALLBACK OptionsWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        DialogContext* ctx = reinterpret_cast<DialogContext*>(::GetWindowLongPtrW(hWnd, GWLP_USERDATA));

        switch (msg)
        {
        case WM_CREATE:
        {
            const CREATESTRUCTW* cs = reinterpret_cast<const CREATESTRUCTW*>(lParam);
            ctx = static_cast<DialogContext*>(cs->lpCreateParams);
            ::SetWindowLongPtrW(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(ctx));

            ctx->hFont = CreateUiFont();

            // 先建分组框，保证它位于其它控件的下层，不会盖住文字
            CreateChild(hWnd, L"BUTTON", L"文字颜色", BS_GROUPBOX, 12, 12, 336, 196, -1, ctx);

            CreateChild(hWnd, L"STATIC", L"深色模式", SS_CENTER, 148, 34, 84, 18, IDC_LBL_DARK_HEADER, ctx);
            CreateChild(hWnd, L"STATIC", L"浅色模式", SS_CENTER, 238, 34, 84, 18, IDC_LBL_LIGHT_HEADER, ctx);

            CreateChild(hWnd, L"STATIC", L"中文状态：", SS_RIGHT, 24, 66, 112, 20, IDC_LBL_CHINESE, ctx);
            CreateChild(hWnd, L"STATIC", L"英文状态：", SS_RIGHT, 24, 104, 112, 20, IDC_LBL_ENGLISH, ctx);

            ctx->hColorBtns[0] = CreateChild(hWnd, L"BUTTON", L"", BS_OWNERDRAW | WS_TABSTOP, 148, 62, 84, 28, IDC_BTN_CHINESE_DARK, ctx);
            ctx->hColorBtns[1] = CreateChild(hWnd, L"BUTTON", L"", BS_OWNERDRAW | WS_TABSTOP, 238, 62, 84, 28, IDC_BTN_CHINESE_LIGHT, ctx);
            ctx->hColorBtns[2] = CreateChild(hWnd, L"BUTTON", L"", BS_OWNERDRAW | WS_TABSTOP, 148, 100, 84, 28, IDC_BTN_ENGLISH_DARK, ctx);
            ctx->hColorBtns[3] = CreateChild(hWnd, L"BUTTON", L"", BS_OWNERDRAW | WS_TABSTOP, 238, 100, 84, 28, IDC_BTN_ENGLISH_LIGHT, ctx);

            CreateChild(hWnd, L"STATIC", L"预览：", SS_RIGHT, 24, 146, 112, 20, IDC_LBL_PREVIEW, ctx);
            ctx->hPreview = CreateChild(hWnd, L"STATIC", L"", SS_OWNERDRAW, 148, 140, 174, 56, IDC_PREVIEW, ctx);

            // 字体设置（在颜色组下方），点击后弹出系统字体对话框
            CreateChild(hWnd, L"STATIC", L"字体：", SS_RIGHT, 24, 222, 112, 20, IDC_LBL_FONT, ctx);
            ctx->hFontBtn = CreateChild(hWnd, L"BUTTON", L"系统默认字体", BS_PUSHBUTTON | WS_TABSTOP, 148, 218, 174, 28, IDC_BTN_FONT, ctx);
            UpdateFontButtonText(ctx->hFontBtn, ctx->cfg.fontName, ctx->cfg.fontSize);

            CreateChild(hWnd, L"BUTTON", L"恢复默认", BS_PUSHBUTTON | WS_TABSTOP, 12, 262, 100, 32, IDC_BTN_RESTORE_DEFAULT, ctx);
            CreateChild(hWnd, L"BUTTON", L"确定", BS_DEFPUSHBUTTON | WS_TABSTOP, 176, 262, 84, 32, IDOK, ctx);
            CreateChild(hWnd, L"BUTTON", L"取消", BS_PUSHBUTTON | WS_TABSTOP, 268, 262, 84, 32, IDCANCEL, ctx);

            if (ctx->hFont != nullptr)
            {
                for (HWND hChild = ::GetWindow(hWnd, GW_CHILD); hChild != nullptr;
                     hChild = ::GetWindow(hChild, GW_HWNDNEXT))
                {
                    ::SendMessageW(hChild, WM_SETFONT, reinterpret_cast<WPARAM>(ctx->hFont), TRUE);
                }
            }
            return 0;
        }

        case WM_DRAWITEM:
        {
            if (ctx == nullptr)
                break;

            const DRAWITEMSTRUCT* dis = reinterpret_cast<const DRAWITEMSTRUCT*>(lParam);
            switch (dis->CtlID)
            {
            case IDC_BTN_CHINESE_DARK:  DrawColorButton(dis, ctx->cfg.colors.chinese_dark, ctx->hFont);  return TRUE;
            case IDC_BTN_CHINESE_LIGHT: DrawColorButton(dis, ctx->cfg.colors.chinese_light, ctx->hFont); return TRUE;
            case IDC_BTN_ENGLISH_DARK:  DrawColorButton(dis, ctx->cfg.colors.english_dark, ctx->hFont);  return TRUE;
            case IDC_BTN_ENGLISH_LIGHT: DrawColorButton(dis, ctx->cfg.colors.english_light, ctx->hFont); return TRUE;
            case IDC_PREVIEW:           DrawPreview(dis, ctx);                                       return TRUE;
            default:                    break;
            }
            break;
        }

        case WM_COMMAND:
        {
            if (ctx == nullptr)
                break;

            switch (LOWORD(wParam))
            {
            case IDC_BTN_CHINESE_DARK:
                if (PickColor(hWnd, ctx->cfg.colors.chinese_dark))
                    RefreshControls(ctx);
                return 0;
            case IDC_BTN_CHINESE_LIGHT:
                if (PickColor(hWnd, ctx->cfg.colors.chinese_light))
                    RefreshControls(ctx);
                return 0;
            case IDC_BTN_ENGLISH_DARK:
                if (PickColor(hWnd, ctx->cfg.colors.english_dark))
                    RefreshControls(ctx);
                return 0;
            case IDC_BTN_ENGLISH_LIGHT:
                if (PickColor(hWnd, ctx->cfg.colors.english_light))
                    RefreshControls(ctx);
                return 0;
            case IDC_BTN_FONT:
                if (PickFont(hWnd, ctx->cfg.fontName, ctx->cfg.fontSize))
                {
                    UpdateFontButtonText(ctx->hFontBtn, ctx->cfg.fontName, ctx->cfg.fontSize);
                    RefreshControls(ctx);
                }
                return 0;
            case IDC_BTN_RESTORE_DEFAULT:
                ctx->cfg = IMEAppearance{};
                UpdateFontButtonText(ctx->hFontBtn, ctx->cfg.fontName, ctx->cfg.fontSize);
                RefreshControls(ctx);
                return 0;
            case IDOK:
                ctx->ok = true;
                ::DestroyWindow(hWnd);
                return 0;
            case IDCANCEL:
                ::DestroyWindow(hWnd);
                return 0;
            default:
                break;
            }
            break;
        }

        case WM_CLOSE:
            ::DestroyWindow(hWnd);
            return 0;

        case WM_DESTROY:
            // 注意：这里不能 PostQuitMessage，这只是插件里的一个模态子窗口
            if (ctx != nullptr && ctx->hFont != nullptr)
            {
                ::DeleteObject(ctx->hFont);
                ctx->hFont = nullptr;
            }
            return 0;

        default:
            break;
        }

        return ::DefWindowProcW(hWnd, msg, wParam, lParam);
    }

    bool EnsureWindowClass()
    {
        static bool s_registered = false;
        if (s_registered)
            return true;

        WNDCLASSEXW wc{};
        wc.cbSize = sizeof(wc);
        wc.lpfnWndProc = OptionsWndProc;
        wc.hInstance = GetSelfModule();
        wc.hCursor = ::LoadCursorW(nullptr, IDC_ARROW);
        wc.hbrBackground = ::GetSysColorBrush(COLOR_BTNFACE);
        wc.lpszClassName = kWndClass;

        if (::RegisterClassExW(&wc) == 0 && ::GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
            return false;

        s_registered = true;
        return true;
    }
}

bool ShowIMEColorOptions(HWND hParent, IMEAppearance& cfg)
{
    if (!EnsureWindowClass())
        return false;

    DialogContext ctx;
    ctx.cfg = cfg;
    ctx.scale = static_cast<int>(GetDpiForWindowSafe(hParent) * 100 / 96);
    if (ctx.scale <= 0)
        ctx.scale = 100;

    RECT rc = { 0, 0, Scale(&ctx, kClientW), Scale(&ctx, kClientH) };
    const DWORD style = WS_POPUP | WS_CAPTION | WS_SYSMENU;
    ::AdjustWindowRectEx(&rc, style, FALSE, 0);
    const int wndW = rc.right - rc.left;
    const int wndH = rc.bottom - rc.top;

    // 居中到父窗口；没有父窗口时居中到主显示器工作区
    int x = 0;
    int y = 0;
    RECT parentRect{};
    if (hParent != nullptr && ::GetWindowRect(hParent, &parentRect))
    {
        x = parentRect.left + ((parentRect.right - parentRect.left) - wndW) / 2;
        y = parentRect.top + ((parentRect.bottom - parentRect.top) - wndH) / 2;
    }
    else
    {
        RECT wa{};
        ::SystemParametersInfoW(SPI_GETWORKAREA, 0, &wa, 0);
        x = wa.left + ((wa.right - wa.left) - wndW) / 2;
        y = wa.top + ((wa.bottom - wa.top) - wndH) / 2;
    }

    HWND hWnd = ::CreateWindowExW(WS_EX_CONTROLPARENT, kWndClass, L"输入法状态 - 选项", style,
                                  x, y, wndW, wndH, hParent, nullptr, GetSelfModule(), &ctx);
    if (hWnd == nullptr)
        return false;

    if (hParent != nullptr)
        ::EnableWindow(hParent, FALSE);

    ::ShowWindow(hWnd, SW_SHOW);
    ::UpdateWindow(hWnd);

    if (ctx.hColorBtns[0] != nullptr)
        ::SetFocus(ctx.hColorBtns[0]);

    MSG msg{};
    while (::IsWindow(hWnd))
    {
        const BOOL ret = ::GetMessageW(&msg, nullptr, 0, 0);
        if (ret <= 0)
        {
            if (ret == 0)
                ::PostQuitMessage(static_cast<int>(msg.wParam));
            break;
        }
        if (!::IsDialogMessageW(hWnd, &msg))
        {
            ::TranslateMessage(&msg);
            ::DispatchMessageW(&msg);
        }
    }

    if (hParent != nullptr)
    {
        ::EnableWindow(hParent, TRUE);
        ::SetActiveWindow(hParent);
    }

    if (ctx.ok)
        cfg = ctx.cfg;
    return ctx.ok;
}
