# P02 数据与参数层验收记录

日期：2026-09-19。任务当前状态：**已验收**。原项目现有代码、构建配置和文档均未修改。

## 交付范围

| 交付 | 位置 | 实际结果 |
|---|---|---|
| 完整选项定义 | `src/core/catalog.h/.cpp` | 61 CLI、11 Ri；稳定 ID、别名、值类型、范围、规则、枚举和默认值提示 |
| 分层会话模型 | `src/core/model.h/.cpp` | 公共、左侧、右侧公共、逐视频覆盖；列表稳定 ID、启用、复制、排序和删除 |
| 参数与校验 | `src/core/build_plan.cpp` | 独立查询/比较计划、作用范围、输入和引用、冲突/数值校验、结构化诊断 |
| Windows 命令行与预览 | `src/core/command_line.cpp`、`build_plan.cpp` | 精确保留空值、反斜杠、引号、中文；可读 argv 预览和参数来源索引 |
| 独立测试 | `tests/core_tests.cpp`、`*_cases.inc` | 独立期望值，不从生产目录生成期望 argv；Debug/Release 各 103 项检查 |
| 源码交叉核验 | `tools/verify-coverage.ps1` | 源码→覆盖表→代码目录→独立样例的 ID/字段/别名/参数个数匹配 |
| 构建集成 | `CMakeLists.txt`、`tools/build.ps1 -Test` | 独立 `launcher-core` 静态库；CTest；所有产物和证据在子目录 |

完整接口与边界详见 [P02-API.md](P02-API.md)。本轮没有增加窗口中的视频选择、参数控件或比较按钮行为；命令预览以 API 交付，界面展示按 P03/P04 接入。

## 实测结果

环境沿用 P01：MSVC 19.51.36243.0、Visual Studio 18 2026 生成器、Windows SDK 10.0.26100.0、CMake 4.2.3-msvc3，x64。

| 验收组 | 每种构建的检查数量 | Debug | Release |
|---|---:|---|---|
| `core.catalog` | 62（61 CLI + 目录唯一性/覆盖） | PASS | PASS |
| `core.overrides` | 12（11 Ri + 覆盖完整性） | PASS | PASS |
| `core.semantics` | 26 | PASS | PASS |
| `core.quoting` | 3，其中一项含 1000 组确定性随机往返 | PASS | PASS |
| 合计 | 103；CTest 展示为 4 组 | 全通过 | 全通过 |

关键验证：

- 61 个选项逐项使用独立预期的长选项和值；布尔关闭/恢复默认、短别名、作用范围和参数跟踪；查询不混入媒体输入。
- 11 个 Ri 字段逐项验证 Replace、Clear、Inherit，确认不污染其他右侧输入。
- 公共→左/右→Ri 配置、滤镜追加、空模板、未解决引用、解码器字典表达式保留、左右成对值及单侧清空。
- 1+11 输入、排序/复制后的稳定 ID、禁用输入、左右自引用、中文/空格/特殊字符路径、地址/图片序列/脚本。
- 非法数值、空值恢复默认、尺寸互斥、未知字段/错误作用范围、无效 Unicode、保留 `::`、命令长度限制。
- 独立 Windows `CommandLineToArgvW` 对照生成命令，校验空参数、引号前反斜杠、末尾反斜杠、中文及补充平面字符；1000 组固定随机种子测试。
- 外部配置来源按顺序传递，明确提示最终值尚未解析；不会把 GUI 中 false 的省略误报为已经撤销配置文件布尔开关。
- Debug、Release、dist 副本的主窗口、关于对话框、调整大小、PerMonitorV2 和正常退出再次通过，退出码均为 0。

## 证据

- [Debug 逐项结果](work/p02/Debug-cases.log) / [JUnit](work/p02/Debug-tests.xml)。
- [Release 逐项结果](work/p02/Release-cases.log) / [JUnit](work/p02/Release-tests.xml)。
- [窗口回归结果](work/p02-window/window-check.json)，截图位于同目录。
- 所有日志属于生成产物，按局部 `.gitignore` 忽略；本文记录可长期跟踪的验收摘要。

复现（项目根目录；任一步失败立即停止，避免后续窗口检查误用旧 EXE）：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\gui-launcher\tools\verify-p02.ps1
```

## 中文终端兼容修复

用户复现发现：原 `build.ps1` 通过 Windows PowerShell 原生命令管道接收 `vswhere -utf8`，在 CP936 环境中误解码了中文字段，甚至吞掉 JSON 字符串结尾引号，导致 `ConvertFrom-Json` 失败。初次验收的终端代码页为 65001，未覆盖此环境差异。后续窗口 PASS 只能说明已有 EXE 可运行，不能证明失败的构建完成。

已新增 `tools/vs-discovery.ps1`，使用 `ProcessStartInfo` 重定向输出并明确指定 UTF-8，异步排空 stdout/stderr，检查退出码并限制发现过程时长；不更改用户的控制台编码。`tools/verify-vs-discovery.ps1` 在 CP936/65001 下比较安装路径、版本及中文显示名称/描述，验证无损解析。

`tools/verify-p02.ps1` 将编码回归、覆盖核验、构建/测试/安装和窗口检查串联，检查每一步退出码。任何前置失败均停止后续步骤。旧的分行命令仍可单独用于诊断，但不能用窗口 PASS 覆盖构建失败。

修复验证结果：CP936 下原读取方式复现 JSON 失败；新方式在 CP936/65001 下保留完整 Unicode 字段并通过。随后在 CP936 下执行完整验证入口，Debug/Release 各 103 项检查、安装和三份窗口检查全部通过。另在 `work/encoding-regression/` 使用独立替身模拟覆盖检查退出码 23，确认验证入口返回失败且不执行构建和窗口步骤，日志见该目录 `fail-fast.log`。

## 尚未验收的后续能力

- P03/P04：界面控件绑定、命令预览展示、可访问性和完整 DPI 布局。
- P05：实际引擎兼容性与文件预检、进程启动、stdout/stderr 日志管理；本轮没有运行比较引擎。
- P06：外部 `.opt` 内容解析、来源合并、会话存储及长命令参数文件回退。当前超长命令明确拒绝，外部配置返回未解析警告。
- P07：视频结果、设备能力、FFmpeg 滤镜/解码和 AVRational 时间倍率舍入的真实行为；参数测试不能证明这些能力。

## 持续改进建议

- 执行 P03 时把“开始比较”准备状态建立在结构化诊断上，先展示准确的参数预览，待 P05 接入进程启动。
- P06 完成外部配置解析后，为文件布尔开关、空值、`__`、复杂转义追加真实引擎对照，保留现有独立样例作为回归基准。
