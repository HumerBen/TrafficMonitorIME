# TrafficMonitor 输入法状态插件（TrafficMonitorIME）

一个 TrafficMonitor 插件，在任务栏显示**当前前台窗口的输入法状态**：中文显示红色「中」，英文显示灰色「英」。

## 原理

- TrafficMonitor 插件本质是 C++ DLL，通过 `TMPluginGetInstance()` 导出 `ITMPlugin` 实例。
- 框架每隔一段时间（默认约 1 秒，对应 TrafficMonitor 的“监控时间间隔”）调用一次 `ITMPlugin::DataRequired()`，插件在此查询输入法状态。
- 输入法状态是**按前台（焦点）窗口线程**的，因此查询的是 `GetForegroundWindow()` 对应的 IME，而不是 TrafficMonitor 自身。
- 判定：调用 `ImmGetContext` → `ImmGetOpenStatus` + `ImmGetConversionStatus`，若 IME 开启且处于 `IME_CMODE_NATIVE`（中文原生）模式，则为中文态。

## 目录结构

```
TrafficMonitorIME/
├── TrafficMonitorIME.sln          # 解决方案
├── TrafficMonitorIME.vcxproj      # 工程（x64 / Unicode / 静态运行库 /MT）
├── include/
│   └── PluginInterface.h          # TrafficMonitor 官方插件接口（不要改）
└── src/
    ├── dllmain.cpp                # DLL 入口 + 导出 TMPluginGetInstance
    ├── IMEStatusPlugin.h/.cpp     # ITMPlugin 实现
    └── IMEItem.h/.cpp             # IPluginItem 实现（IME 检测 + 自绘）
```

## 编译步骤

1. 确认已安装 **Visual Studio** 且勾选了 **“使用 C++ 的桌面开发”** 工作负载（内含 MSVC 编译器 + Windows SDK）。
2. 用 Visual Studio 打开 `TrafficMonitorIME.sln`。
3. 顶部工具栏把 **解决方案配置** 设为 `Release`，**解决方案平台** 设为 `x64`（⚠️ 必须是 x64，TrafficMonitor 是 64 位，32 位 DLL 加载不进去）。
4. 菜单 **生成 → 生成解决方案**（或按 `Ctrl+Shift+B`）。
5. 编译产物：`x64\Release\TrafficMonitorIME.dll`。

> 若你用的是 **Visual Studio 2019**，请把 `TrafficMonitorIME.vcxproj` 里的 `v143` 改成 `v142`。

## 部署

1. 把生成的 `TrafficMonitorIME.dll` 复制到 TrafficMonitor 主程序所在目录下的 `plugins\` 文件夹（没有就新建）。
2. 重启 TrafficMonitor。
3. 右键任务栏 → “其他功能” → “插件管理”，应能看到“输入法状态”插件，勾选启用，并在“显示项目”里勾选它。

## 注意事项 / 已知限制

- **约 1 秒延迟**：状态靠轮询刷新，不是实时事件；切换输入法后最多约 1 秒才翻面，但不会漏掉切换。
- **显示的是前台窗口的 IME**：切到哪个窗口打字，就显示哪个窗口的状态（符合直觉）。
- **个别窗口取不到**：全屏游戏、部分 UWP 窗口可能拿不到 IME 上下文，会回退显示「英」。
- 想更跟手，可把 TrafficMonitor 的“监控时间间隔”调小（如 200~500ms）。

## 自定义

- 改颜色：编辑 `src\IMEItem.cpp` 中 `DrawItem()` 的 `bg` / `fg`。
- 改显示文本：编辑 `Update()` 里的 `m_value`（例如区分全角/半角、显示输入法名）。
- 想用纯文字（不画彩色块）：把 `IsCustomDraw()` 返回 `false`，并填好 `GetItemLableText` / `GetItemValueText`。
