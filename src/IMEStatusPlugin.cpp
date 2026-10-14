#include "IMEStatusPlugin.h"

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
    case TMI_VERSION:     return L"1.0.0";
    case TMI_URL:         return L"";
    default:              return L"";
    }
}
