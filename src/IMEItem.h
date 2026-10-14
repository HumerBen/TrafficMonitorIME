#pragma once
#include "PluginInterface.h"
#include <string>

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

    // ---- 自绘相关（显示彩色“中/英”块）----
    virtual bool IsCustomDraw() const override;
    virtual int GetItemWidth() const override;
    virtual void DrawItem(void* hDC, int x, int y, int w, int h, bool dark_mode) override;

    // ---- 由插件框架在 DataRequired 中调用，刷新输入法状态 ----
    void Update();

private:
    std::wstring m_value;     // L"中" 或 L"英"
    bool m_isChinese = false; // 是否处于中文输入态
};
