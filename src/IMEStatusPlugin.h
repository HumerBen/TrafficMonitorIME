#pragma once
#include "PluginInterface.h"
#include "IMEItem.h"
#include <string>

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

    // ---- 插件初始化：保存主程序接口并载入颜色配置 ----
    virtual void OnInitialize(ITrafficMonitor* pApp) override;

    // ---- 选项设置对话框：自定义中 / 英文字的颜色与字体 ----
    virtual OptionReturn ShowOptionsDialog(void* hParent) override;

private:
    CIMEStatusPlugin();
    ~CIMEStatusPlugin();

    // 插件配置文件所在的目录
    std::wstring GetConfigDir() const;

    ITrafficMonitor* m_app = nullptr;   // 主程序接口，用于取配置目录
    CIMEItem m_item;                    // 本插件只提供一个显示项目
};
