# P05 进程与日志验收

日期：2026-09-19。状态：**已验收（P05 范围）**。原项目代码、构建配置和根目录文档零修改；全部交付在 `gui-launcher/`。

## 交付行为

- 主窗口选择已有 `video-compare.exe`，保留原配套 DLL 所在目录；无需编译原项目。无视频也可“检查引擎”，执行版本命令，10 秒超时。
- “开始比较”使用既有参数生成器的会话快照，支持一个参考输入和多个启用的右侧输入。直接 `CreateProcessW`，不经过 shell，不将引擎链接进 GUI。
- `video-compare-runner.exe` 独立持有引擎进程与双路管道，GUI 每 200 ms 读取状态和有限日志。继承句柄仅 stdin/stdout/stderr，stdin 为 NUL。
- 状态区记录 PID、退出码和日志位置；stdout/stderr 标明来源，stderr 不直接判为错误。正常停止先发窗口关闭请求，5 秒后仍未退出才开放人工确认强制结束。
- 关闭启动器提供保留运行、正常结束后退出、取消三种选择。保留运行时助手继续读日志，直到引擎结束；只控制本次创建的进程。
- GUI 与 runner 两个 EXE 必须同目录；`dist/` 已安装当前 Release 版本。

## 源码、控件、范围与用例

完整 61 CLI / 11 RV 对应表继续使用 [COVERAGE.md](COVERAGE.md) 和 [P04-CONTROLS.md](P04-CONTROLS.md)。本轮在 COVERAGE 增加 GUI-001/003/004/005/006/009 的运行控件、作用范围和实测索引。

| 实现 | GUI 控件 / 范围 | 验收依据 |
|---|---|---|
| `src/main.cpp` / `process::Run::start` | 1001/1002 引擎路径；1046 独立版本检查；1043 当前会话启动 | 原生文件选择、无视频检查、有效输入启用、错误重试 |
| `src/process/runtime.cpp` / `runner.cpp` | 1300 状态、1301 日志；仅本次引擎 | 精确 argv、独立 cwd、Unicode/引号/空值、非零退出、缺失 DLL、查询超时 |
| 同上，日志限额与 UTF-8 解码 | 1305 跟随显示、1304 打开日志目录 | 双路大量输出、长行、分段 Unicode、非法字节、原始日志轮转、界面尾部限额 |
| `close_launcher` / `Run::request_stop` / `force_stop` | 1302 正常停止、1303 确认强杀、主窗口关闭三选项 | 正常关闭、超时与强杀、取消关闭、后台持续排空、停止后退出 |

## 实际结果

| 检查 | 结果 | 证据 |
|---|---|---|
| VS 发现 CP936 / CP65001 | PASS，中文字段保留，不改变终端编码 | `tools/verify-vs-discovery.ps1` |
| 源码、参数和控件静态对应 | PASS，61 CLI 全别名及 11 RV；9 主窗口＋52 分类控件 | `verify-coverage.ps1`、`verify-settings-coverage.ps1` |
| Debug / Release 构建与 CTest | 两配置各 10/10 组通过 | `work/p02/{Debug,Release}-tests.xml`、`*-cases.log` |
| 新增进程生命周期 | 两配置各 54 项通过 | `process.lifecycle`；`tests/process_tests.cpp` |
| 原有参数与分类测试 | 每配置 103 项核心参数、7 项原生输入事件、80 项设置测试通过 | 同上 CTest 日志 |
| 真实 GUI 测试引擎操作 | Debug/Release/Installed 各 14 项通过 | `work/p05/process-gui-check.json` |
| 输入与设置回归 | 三份 GUI 各 29 项输入、10 项设置通过 | `work/p03/input-check.json`、`work/p04/settings-check.json` |
| 窗口及 DPI 清单 | 三份 EXE PASS，实测 96 DPI / PerMonitorV2、正常退出 0 | `work/p05-window/` |
| 用户真实引擎和视频 | 7 项 GUI 流程通过，画面检查通过，正常停止退出 0 | `work/p05/real-engine-check.json`、`real-engine-window.png`、`real-gui-log.png` |

原生进程测试涵盖退出码 0/23/259、真实缺 DLL 的 `0xC0000135`、非法/缺失 EXE、缺失工作目录、正常停止与强制终止、独立检查超时。十轮重复运行检查句柄无累计增长。双路输出各约 12 MiB，结束标记均捕获，长行不阻塞，轮转每文件不超过 4 MiB，显示不超过 65,536 个 UTF-16 字符。

GUI 14 项包括三个关闭选项、停止超时前禁用强杀、超时后显示强杀、确认后退出码 `0xC000013A`，并验证关闭 GUI 后助手继续收完延迟大量输出。

## 真实引擎记录

用户提供并实际选择：

```text
引擎：D:\Tools\Media\video-compare-20260828-win10-x86_64\video-compare.exe
版本：video-compare 20260828-reykjavik
左侧：D:\Download\MiniMax_H3_00024-audio.mp4
右侧：D:\Download\MiniMax_H3_00001_.mp4
```

版本检查正常返回 0。比较窗口实际显示两侧视频，截图中有两侧文件名及已推进的播放时间。日志报告左侧 864×1120、24 fps，右侧 1024×1024、24 fps；GUI 保持响应，正常停止返回 0。日志没有把媒体信息写入 stderr 误判为错误。

版本任务 `{7B6EB194-7703-431C-8A10-66395B4A41D3}`，比较任务 `{9AA406FB-778A-4D6C-8DB2-B04676EC96B3}`，记录在 `work/runs/`。真实验证的 GUI SHA256 在 `real-engine-check.json` 中，供对照构建产物。

自动化文件选择最初受系统对话框恢复上次文件名的时序影响，已在测试脚本等待初始化完成后填入路径；修正后真实选择及完整流程通过，没有改变用户引擎或媒体。

## 日志与边界

- 原始输出每路 4 MiB 当前文件＋4 MiB 历史文件，两路最多 16 MiB；更早内容被轮转覆盖。`tail.txt` 是最近显示片段，`session.txt` 保存请求时间、引擎、cwd、argv 和命令预览；`events.log` 保存状态转换。
- UTF-8 跨段增量解码，坏字节显示替代字符，NUL 显示可见符号，原字节在限额日志内保留。不同流的显示顺序为接收顺序，不能复原精确交错时间。
- 同一 GUI 当前同时运行一项任务；版本检查不混入比较参数。完整查询独立管理、会话保存、配置内容解析属于 P06。
- 引擎退出后若后代进程仍持有管道，最多继续读取 1.5 秒并提示，随后释放管道；停止只针对直接创建的引擎，不批量终止后代或其他同名进程。
- 运行助手自身发生不可恢复的记录错误时终止其持有的引擎，避免无人排空的管道。会话历史不自动删除。
- 真实测试为基本 1+1 比较。多个右侧的完整播放切换、全部选项、设备条件和与同配置 CLI 的逐项对照仍在 P07；P05 不将这些项目标成已验收。

## 复验入口

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\gui-launcher\tools\verify-p05.ps1
```

该入口前置失败即停止，依次检查发现脚本、两份覆盖表、双配置构建与十组测试、输入/设置回归、进程 GUI 流程和窗口。真实引擎是另行执行的参数化检查，命令见 [README](README.md)，不会默认使用某台机器的固定路径。

## 持续改进建议

- P06 保存引擎位置、输入顺序与全部设置，并根据所选引擎的帮助/版本建立兼容性提示；P07 以相同媒体与参数逐项对照 CLI 与 GUI。
