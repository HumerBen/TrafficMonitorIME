#pragma once
#include <Windows.h>
#include "IMEItem.h"

// 弹出本插件的选项对话框（由 TrafficMonitor 的“插件管理 -> 选项”调用）。
// 用户在对话框里点击“确定”时，把选好的外观（颜色 + 字体）写入 cfg 并返回 true；
// 点击“取消”或直接关闭窗口时返回 false，cfg 保持不变。
bool ShowIMEColorOptions(HWND hParent, IMEAppearance& cfg);
