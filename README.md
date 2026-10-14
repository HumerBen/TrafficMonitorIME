# TrafficMonitor 输入法状态插件（TrafficMonitorIME）

在 TrafficMonitor 任务栏显示**当前前台窗口的输入法状态**：中文输入态显示红色「中」，英文态显示灰色「英」。支持在插件选项中自定义文字**颜色、字体、字号**，并自动适配系统深 / 浅色模式。

## 功能特性

- 实时显示前台窗口的输入法状态（中文 / 英文），约 1 秒轮询刷新。
- 任务栏只显示「中」「英」文字，无彩色背景块（`DrawItem` 仅 `DrawText`，去除背景绘制）。
- 可自定义中 / 英文字的颜色（深色模式、浅色模式各一套）。
- 可自定义字体与字号（调用系统标准 `ChooseFont` 对话框，一次选定字体名 + 字号）。
- 深 / 浅色模式自动适配，文字配色保证在对应背景下清晰可见。
- 配置持久化到 `TrafficMonitorIME.ini`，无需改代码。

## 原理

- 插件是 C++ DLL，通过 `TMPluginGetInstance()` 导出 `ITMPlugin` 实例；框架每隔约 1 秒调用一次 `ITMPlugin::DataRequired()`。
- 输入法状态是**按前台（焦点）窗口线程**的：`ImmGetContext()` 对别的进程的窗口会返回 `NULL`，因此插件采用多路径查询 `QueryImeState()`：
  1. 取前台窗口 + `GetGUIThreadInfo` 得到真正的焦点子窗口；
  2. 优先用 `ImmGetDefaultIMEWnd` + `SendMessageTimeout(WM_IME_CONTROL, IMC_GETOPENSTATUS / IMC_GETCONVERSIONMODE)`（跨进程安全，无副作用）；
  3. 兜底用 `AttachThreadInput` + `ImmGetContext` + `ImmGetOpenStatus` / `ImmGetConversionStatus`。
- 判定：`IME 开启` 且 `conv & IME_CMODE_NATIVE`（中文原生模式）时为「中」，否则为「英」。两条路径都失败时保留上次状态，避免抖动。
- `IMC_GETCONVERSIONMODE(0x0001)` / `IMC_GETOPENSTATUS(0x0005)` 在 SDK 的 `imm.h` 中未公开定义，已在 `IMEItem.cpp` 内 `#ifndef` 补上。

## 目录结构

```
TrafficMonitorIME/
├── TrafficMonitorIME.sln          # 解决方案
├── TrafficMonitorIME.vcxproj      # 工程（x64 / Unicode / 静态运行库 /MT）
├── include/
│   └── PluginInterface.h          # TrafficMonitor 官方插件接口（不要改）
└── src/
    ├── dllmain.cpp                # DLL 入口 + 导出 TMPluginGetInstance
    ├── IMEStatusPlugin.h/.cpp     # ITMPlugin 实现（初始化、选项对话框）
    ├── IMEItem.h/.cpp             # IPluginItem 实现（IME 检测 + 自绘 + 外观配置）
    └── OptionsDialog.h/.cpp       # 选项对话框（纯代码 Win32，无 .rc 资源）
```

## 编译步骤

1. 安装 **Visual Studio 2022 或更高版本**，勾选「使用 C++ 的桌面开发」工作负载（内含 MSVC 与 Windows SDK）。
2. 打开 `TrafficMonitorIME.sln`。
3. 配置选 `Release`、`x64`（**必须是 x64**：TrafficMonitor 是 64 位，32 位 DLL 无法加载）。
4. 生成解决方案（`Ctrl+Shift+B`）。
5. 产物：`x64\Release\TrafficMonitorIME.dll`。

> 工程已为所有 `ClCompile` 节点添加 `/utf-8`，可正确处理含中文注释的无 BOM UTF-8 源文件。
> 若使用旧版 Visual Studio，请把 `TrafficMonitorIME.vcxproj` 里的工具集版本（如 `v145`）改成对应版本（VS2019 → `v142`）。

## 部署

1. 将 `TrafficMonitorIME.dll` 复制到 TrafficMonitor 主程序目录下的 `plugins\` 文件夹（没有则新建）。
2. 重启 TrafficMonitor。
3. 右键任务栏 →「其他功能」→「插件管理」，启用「输入法状态」插件，并在「显示项目」中勾选它。

## 选项设置

通过「插件管理 → 选项(O)...」打开（纯 Win32 对话框，DPI 自适应）：

- **颜色**：分别设置「中」「英」在深色 / 浅色模式下的文字颜色；预览区左半模拟深色、右半模拟浅色，色块中央反色显示 `RRGGBB`；「恢复默认」重置为内置配色。
- **字体 / 字号**：点击「字体...」打开系统标准字体对话框，一次选定字体名与字号（point）；按钮文本显示当前「字体名 pxpt」或「系统默认字体」。清空 / 设为系统默认则沿用系统字体。「恢复默认」同时重置字体。

## 配置持久化

配置保存在插件配置目录（由 `ITrafficMonitor::GetPluginConfigDir()` 取得，失败回退到主程序所在目录）下的 `TrafficMonitorIME.ini`：

```ini
[Colors]
ChineseColorDark=   ; 0x00BBGGRR 的十进制值（深色模式下的“中”）
EnglishColorDark=   ; 深色模式下的“英”
ChineseColorLight=  ; 浅色模式下的“中”
EnglishColorLight=  ; 浅色模式下的“英”

[Font]
FaceName=           ; 字体名，空字符串 = 系统默认
Size=               ; 字号（point），0 或负数 = 系统默认
```

读写使用 `GetPrivateProfileIntW` / `WritePrivateProfileStringW`。

## 已知限制

- **约 1 秒延迟**：状态靠轮询刷新，不是实时事件；切换输入法后最多约 1 秒才翻面，但不会漏掉切换。
- 显示的是**前台窗口**的 IME：切到哪个窗口打字，就显示哪个窗口的状态（符合直觉）。
- 个别窗口（全屏游戏、部分 UWP）可能取不到 IME 上下文，会回退显示「英」。
- 想更跟手，可调小 TrafficMonitor 的「监控时间间隔」（如 200~500ms）。

## 版本

- 版本号格式：`V年.月.日.当日发布次数`（如 `V2026.10.14.1`），定义于 `src/IMEStatusPlugin.cpp` 的 `TMI_VERSION`。
