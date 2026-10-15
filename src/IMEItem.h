#pragma once
#include "PluginInterface.h"
#include <Windows.h>
#include <string>

// 中 / 英文字的颜色配置。
// TrafficMonitor 的显示区域有深色和浅色两种模式，因此每种文字各保存两套颜色，
// 这两套默认值都保证在对应模式的背景上清晰可见。用户可在插件选项中自定义。
struct IMEColors
{
    COLORREF chinese_dark{ RGB(255, 120, 120) };   // 深色模式下的“中”
    COLORREF english_dark{ RGB(215, 215, 215) };   // 深色模式下的“英”
    COLORREF chinese_light{ RGB(200, 40, 40) };    // 浅色模式下的“中”
    COLORREF english_light{ RGB(60, 60, 60) };     // 浅色模式下的“英”
};

// 插件的完整外观配置：颜色 + 字体 + 刷新行为。
struct IMEAppearance
{
    IMEColors    colors;        // 中 / 英文字颜色（深 / 浅色各一套）
    std::wstring fontName;      // 字体名；空字符串表示“系统默认字体”
    int          fontSize = 0;  // 字号（点，point）；<=0 表示“系统默认”

    // 后台实时刷新行为：独立于 TrafficMonitor 的全局监控时间间隔，
    // 自己用定时器轮询输入法状态，状态一变就主动让主程序重绘。
    bool realtimePoll   = true; // 是否启用后台实时刷新
    int  pollIntervalMs = 200;  // 后台轮询间隔（毫秒），范围 16~2000
};

// 一个显示项目：输入法状态（中 / 英）
class CIMEItem : public IPluginItem
{
public:
    CIMEItem();
    virtual ~CIMEItem();

    // ---- IPluginItem 必须实现的纯虚函数 ----
    virtual const wchar_t* GetItemName() const override;
    virtual const wchar_t* GetItemId() const override;
    virtual const wchar_t* GetItemLableText() const override;
    virtual const wchar_t* GetItemValueText() const override;
    virtual const wchar_t* GetItemValueSampleText() const override;

    // ---- 自绘相关（只显示“中/英”文字，不画背景）----
    virtual bool IsCustomDraw() const override;
    virtual int GetItemWidth() const override;
    virtual void DrawItem(void* hDC, int x, int y, int w, int h, bool dark_mode) override;

    // ---- 刷新输入法状态。返回本次相比上次是否发生了变化 ----
    bool Update();

    // ---- 供后台实时刷新定时器使用：保存主程序接口以主动触发重绘 ----
    void SetApp(ITrafficMonitor* app);

    // ---- 外观配置（颜色 + 字体 + 刷新行为）。config_dir 为插件配置目录 ----
    const IMEAppearance& GetAppearance() const { return m_appearance; }
    void SetAppearance(const IMEAppearance& appearance);

    void LoadAppearance(const std::wstring& config_dir);
    void SaveAppearance(const std::wstring& config_dir) const;

private:
    std::wstring m_value;          // L"中" 或 L"英"
    bool m_isChinese = false;      // 是否处于中文输入态
    IMEAppearance m_appearance;    // 文字颜色、字体与刷新行为（可在插件选项中自定义）
    HFONT m_hFont{ nullptr };      // 缓存的显示字体（fontSize<=0 时为 nullptr，用系统默认）
    ITrafficMonitor* m_app = nullptr;  // 主程序接口，用于主动重绘

    void UpdateFont();             // 依据 m_appearance 重建 m_hFont
};
