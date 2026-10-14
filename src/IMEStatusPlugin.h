#pragma once
#include "PluginInterface.h"
#include "IMEItem.h"

// 插件主类：实现 ITMPlugin 接口
class CIMEStatusPlugin : public ITMPlugin
{
public:
    // 单例：框架通过 TMPluginGetInstance() 获取这个唯一实例，不由框架释放
    static CIMEStatusPlugin& Instance();

    // ---- ITMPlugin 必须实现的纯虚函数 ----
    virtual IPluginItem* GetItem(int index) override;
    virtual void DataRequired() override;
    virtual const wchar_t* GetInfo(PluginInfoIndex index) override;

private:
    CIMEStatusPlugin();
    ~CIMEStatusPlugin();

    CIMEItem m_item;   // 本插件只提供一个显示项目
};
