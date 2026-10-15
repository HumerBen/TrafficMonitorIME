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

    // 后台实时刷新：创建一个 message-only 隐藏窗口并用 SetTimer 在其上派发
    // WM_TIMER（在主线程，跨线程安全）。定时器周期性调用 m_item.Update()，
    // 仅当状态变化时主动让主程序重绘。
    void StartRealtimePoll();                 // 按当前配置启动 / 重启定时器
    void StopRealtimePoll();                  // 停止定时器
    static LRESULT CALLBACK HiddenWndProc(HWND, UINT, WPARAM, LPARAM);

    ITrafficMonitor* m_app = nullptr;   // 主程序接口，用于取配置目录与主动重绘
    CIMEItem m_item;                    // 本插件只提供一个显示项目
    HWND m_hiddenWnd = nullptr;         // 仅用于接收 WM_TIMER 的 message-only 窗口
    UINT_PTR m_timerId = 0;             // SetTimer 返回的定时器 ID（0 表示未启动）
};
