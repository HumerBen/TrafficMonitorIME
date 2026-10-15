# TrafficMonitor 输入法状态插件（TrafficMonitorIME）

在 TrafficMonitor 任务栏显示**当前前台窗口的输入法状态**：中文输入态显示红色「中」，英文态显示灰色「英」。支持在插件选项中自定义文字**颜色、字体、字号**，并自动适配系统深 / 浅色模式。

## 功能特性

- 实时显示前台窗口的输入法状态（中文 / 英文）。
- 内置「后台实时刷新」：插件自建定时器轮询输入法状态，状态一变即重绘（默认 200ms），与 TrafficMonitor 全局监控间隔无关（见下方「实时刷新设置」）。
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
- **后台实时刷新（可选）**：除框架的 `DataRequired()` 外，插件创建一个 message-only 隐藏窗口并用 `SetTimer` 在主线程派发 `WM_TIMER`，按设定间隔（默认 200ms）周期调用 `QueryImeState()`；当检测到「中↔英」翻转时，用 `ITrafficMonitor::GetMainWindowHwnd()/GetTaskbarWindowHwnd()` + `InvalidateRect` 主动让主程序立即重绘。这样输入法状态可「切换即刷新」，且无需缩小全局监控间隔，也不影响网速 / CPU 等其它显示的刷新频率。
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

### 实时刷新设置

「实时刷新」一组控件用于让输入法状态在切换时**立刻**更新，而不必等待 TrafficMonitor 的全局监控间隔：

- **启用实时刷新**（复选框，默认勾选）：开启后插件在后台自起定时器轮询输入法状态，状态一变就主动让主程序重绘；取消勾选则退回「仅由框架按全局监控间隔刷新」的被动模式。
- **刷新间隔（毫秒）**（数字框 + 上下微调按钮，默认 `200`，范围 `16~2000`）：后台轮询的周期。值越小越跟手、CPU 占用略增；值越大越省资源、切换后最多延迟该时长才翻面。一般 `100~300ms` 体感已足够跟手。
- 修改后点击「确定」即时生效（定时器会按新间隔重启）；「恢复默认」会重置为 `启用 + 200ms`。

> 若不希望插件常驻后台轮询，可取消「启用实时刷新」，改由全局监控间隔驱动；此时跟手程度取决于 TrafficMonitor 的「监控时间间隔」。

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

[Behavior]
RealtimePoll=       ; 1 = 启用后台实时刷新，0 = 关闭（退回全局监控间隔驱动）
PollIntervalMs=     ; 后台轮询间隔（毫秒），范围 16~2000，默认 200
```

读写使用 `GetPrivateProfileIntW` / `WritePrivateProfileStringW`。

## 已知限制

- **刷新延迟**：默认开启后台实时刷新（200ms），切换输入法后最多约 200ms 才翻面；若关闭「实时刷新」或取状态失败，则回退到框架按全局监控间隔（约 1 秒）刷新。无论哪种都不会漏掉切换。
- 显示的是**前台窗口**的 IME：切到哪个窗口打字，就显示哪个窗口的状态（符合直觉）。
- 个别窗口（全屏游戏、部分 UWP）可能取不到 IME 上下文，会回退显示「英」。
- 想更跟手：优先用插件选项里的「实时刷新 / 刷新间隔」调节，不必改动 TrafficMonitor 的全局「监控时间间隔」（那样会同时影响网速 / CPU 等所有显示）。

## 版本

- 版本号格式：`V年.月.日.当日发布次数`（如 `V2026.10.14.1`），定义于 `src/IMEStatusPlugin.cpp` 的 `TMI_VERSION`。
