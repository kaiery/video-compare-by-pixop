# video-compare 独立 GUI 启动器

当前阶段：P00–P06 已验收，P07 按受限验收收尾，P08 独立发布已验收。0.1.1 便携包在 `releases/video-compare-gui-0.1.1-win-x64.zip`，对应源码包也在 `releases/`。解压后双击 `video-compare-gui.exe`，选择已有引擎及视频即可开始比较。P07 未完整验证的项目随包在 `KNOWN-ISSUES.md` 中说明。

目标：Windows x64 独立桌面启动器，支持一个左侧参考输入、多个右侧输入、全部现有启动选项、逐视频覆盖、开始比较、错误与运行日志。执行现有 `video-compare.exe`，不修改、不链接原项目源码，不改变原有播放器行为。

## 文档与任务入口

- [实施任务计划](PLAN.md)：任务顺序、依赖、边界、完成定义。
- [完整覆盖与验收表](COVERAGE.md)：61 个 CLI 选项、11 个逐视频覆盖字段、输入语法及运行操作边界。
- [环境核验记录](ENVIRONMENT.md)：本机工具链、源码基线及待验证条件。
- [P01 验收记录](P01-ACCEPTANCE.md)：构建、窗口运行、资源与依赖检查。
- [P02 验收记录](P02-ACCEPTANCE.md)：完整选项映射、103 项参数检查和 Windows 转义验证。
- [P02 接口说明](P02-API.md)：会话结构、作用范围、继承、诊断和命令预览 API。
- [P03 验收记录](P03-ACCEPTANCE.md)：输入/常用控件对应表、真实 EXE 操作检查、原生文件框和拖入事件验证。
- [P04 逐项控件对应表](P04-CONTROLS.md)：61 CLI＋11 RV 的实际控件 ID、作用范围和验收用例。
- [P04 验收记录](P04-ACCEPTANCE.md)：全部分类设置、逐视频编辑器、查询隔离和回归结果。
- [P05 验收记录](P05-ACCEPTANCE.md)：引擎路径选择、进程生命周期、日志与真实视频验证。
- [P06 验收记录](P06-ACCEPTANCE.md)：会话、参数文件、来源与合并结果、独立查询及真实引擎证据。
- [P08 发布验收](P08-ACCEPTANCE.md)：运行包、源码包、依赖清单、校验值和干净目录运行验证。
- [覆盖表核验脚本](tools/verify-coverage.ps1)：只读扫描原源码，交叉核对覆盖表、代码目录、参数个数和独立测试样例。

从项目根目录执行文档覆盖核验：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\gui-launcher\tools\verify-coverage.ps1
```

静态核验不替代行为测试。61+11 参数模型和生成测试已完成，P05 已验证真实媒体基本比较；全部模式的逐项实机对照仍由 P07 完成。

## 构建与打开窗口

从项目根目录运行，无需手工打开 VS 开发者终端：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\gui-launcher\tools\build.ps1 -Configuration All -Install
```

脚本用 `vswhere` 检测 MSVC 安装，依据 CMake 支持列表选择对应 Visual Studio 生成器，并构建 x64 Debug/Release。安装步骤把 Release 启动器及运行助手复制到子目录 `dist/`，不是系统安装，也不是 P08 完整发布。

同时执行 P02 参数测试：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\gui-launcher\tools\build.ps1 -Configuration All -Test -Install
```

`-Test` 强制启用独立测试目标，执行 Debug/Release 的四组核心测试、一组原生输入事件测试、四组分类设置测试、一组进程生命周期测试和一组配置存储测试，共十一组，无测试时视为失败。逐项结果与 JUnit 报告分别保存到 `work/p02/Debug-cases.log`、`Release-cases.log` 和 `*-tests.xml`（沿用原报告位置）。测试替身结果不表示已验证真实解码和播放。

推荐使用一次完成 P02 验证的入口（前置失败不会继续检查旧 EXE）：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\gui-launcher\tools\verify-p02.ps1
```

VS 发现输出由独立 UTF-8 管道读取，兼容 Windows PowerShell 5.1 的中文 CP936 和 UTF-8 终端，无需 `chcp` 或修改终端编码。

双击 `gui-launcher/dist/video-compare-gui.exe` 即可打开窗口。

1. 选择比较程序和工作目录；默认工作目录为子目录 `work/`。
2. 为左侧选择一个文件，或切换类型后输入图片序列、协议地址、脚本或引用占位符。
3. 右侧用“添加文件…”多选，或将文件拖入列表；“添加路径 / 地址…”支持手动输入。
4. 勾选条目决定是否参与比较；双击编辑滤镜，使用复制、移除、上移/下移或拖动行管理顺序。复制条目拥有独立配置。
5. 设置布局、循环、窗口策略和时间偏移等常用参数，查看校验提示及参数数组。缩小窗口后可滚动访问下方控件。
6. 点击“全部设置…”进入播放与同步、画面与窗口、色彩/HDR、滤镜、解码与硬件、分析窗口、字体、配置与诊断、能力查询九个分类。
7. 右侧条目“编辑…”→“全部逐视频设置（11 项）…”可编辑全部受支持的独立字段；其中只有滤镜提供“追加”。
8. 点击“检查引擎”可在没有视频时运行版本检查；选好视频后点击“开始比较”。引擎 EXE 与原配套 DLL 保留在原目录，无需先编译原项目。
9. 在运行日志窗口查看 stdout/stderr、PID、退出码，点击“正常停止”关闭本次比较；超过 5 秒未结束后，可自行选择“强制结束”。关闭启动器时可保留任务运行、正常结束后退出或取消关闭。

`video-compare-gui.exe` 与 `video-compare-runner.exe` 必须放在同一目录。独立运行助手负责持续读取日志，因此选择保留任务后 GUI 可退出，引擎和日志记录继续运行，直到引擎结束。不会按进程名批量停止其他比较窗口。

日志在 `work/runs/{会话标识}/`：`session.txt` 保存参数预览及工作目录，`events.log` 记录状态；`stdout.raw`、`stderr.raw` 保存原始字节，每路 4 MiB 并保留一份 `.1` 轮转文件，合计最多 16 MiB 原始输出。界面保留最近 65,536 个 UTF-16 字符；取消“跟随刷新”只冻结显示，后台继续记录。历史会话目录保留，可在任务结束后自行清理。无效 UTF-8 用替代字符显示，原始字节仍在限额日志中。跨流显示顺序仅代表读取顺序。

同一 GUI 同时运行一个比较任务；设置页查询使用另一个独立任务槽，可在比较期间执行或取消。主窗口快速版本检查仍与比较共用任务槽。查询超时为 10 秒，比较无固定时限。状态“进程已启动”不等于视频加载成功，应结合引擎窗口、媒体日志和退出码判断。主窗口检查引擎仅验证版本命令能执行；设置页执行“引擎帮助”会列出相对源码基线缺失或额外的选项。帮助声明不代表设备和全部模式已实测，后者属于 P07。

分类窗口使用草稿：确定后写回主会话，取消保留原设置，“恢复本页默认”仅影响当前分类。“指定此值”关闭表示不传入该层设置；打开后，空值仍是显式值，是否允许由参数规则决定。颜色公共值、左侧覆盖、右侧默认独立保存。复杂滤镜与解码表达式按文本保留，由同一参数生成器处理。

配置页支持有序 `.opt` 文件列表和自动加载开关。文件菜单可导入为设置、导出、查看来源和合并预览；未能无损表示的文件可经确认保留为兼容引用，不重写原文件。外部文件可能启用布尔选项，不能仅凭 GUI 未勾选判断其最终关闭。能力查询页可预览、执行和取消独立查询，不向比较会话加入查询选项。

启动前检查参数结构、EXE 格式和工作目录；DLL 加载失败及非零退出保留诊断，视频能否解码由引擎执行结果确定。“复制进程参数”输出 Windows 进程命令行，不是可直接粘贴进任意 shell 的脚本。

完整重跑 P03 验收（任一步失败即停止，避免检查旧 EXE）：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\gui-launcher\tools\verify-p03.ps1
```

该命令短暂打开系统文件框和测试窗口后自动关闭，只操作测试创建的窗口。控件结果和 1+3 截图保存在 `work/p03/`，窗口截图在 `work/p03-window/`。

完整重跑 P04（包含 P03 回归）：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\gui-launcher\tools\verify-p04.ps1
```

P04 结果在 `work/p04/settings-check.json`，九个分类和逐视频编辑器截图在同目录；窗口检查在 `work/p04-window/`。该入口先验证源码/控件对应关系，再构建并测试，前置失败会停止后续步骤。

构建输出：

- Debug：`build/msvc-x64/bin/Debug/video-compare-gui.exe`
- Release：`build/msvc-x64/bin/Release/video-compare-gui.exe`
- 可直接打开的副本：`dist/video-compare-gui.exe`
- 独立运行助手：各配置同目录及 `dist/video-compare-runner.exe`

完整重跑 P05（失败即停止，含 P03/P04 回归和测试引擎）：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\gui-launcher\tools\verify-p05.ps1
```

另用真实引擎验证，路径替换为本机文件（会自动打开并正常停止本次创建的比较）：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\gui-launcher\tools\verify-process-gui.ps1 -Engine 'D:\path\video-compare.exe' -Left 'D:\videos\left.mp4' -Right 'D:\videos\right.mp4'
```

执行窗口检查（短暂显示测试窗口并自动关闭）：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\gui-launcher\tools\verify-window.ps1
```

截图与结构化结果默认保存在 `work/p01/`，可用 `-EvidenceName p03-window` 指定证据目录。脚本只操作它创建的 GUI 进程，验证 x64/GUI 子系统、主窗口、控件存在、DPI 清单、窗口缩放/滚动、“关于”对话框及正常退出；P03 的具体控件和行为由 `verify-inputs.ps1` 检查。

## 会话与引擎参数文件

文件菜单的“保存会话…”生成 `.vcgui`，再用“打开会话…”恢复。它保存引擎路径、工作目录、输入类型/顺序/启用状态、全部设置、继承/替换/追加/清空及显式配置来源；不打包视频和引擎。相对本地输入按工作目录固化为绝对路径，网络地址和引用占位符保持原样。采用版本与完整性校验、原子替换；损坏或不支持的旧版本文件不会覆盖当前会话。当前为手动保存/打开，不会退出时自动保存。

“导入引擎参数文件 (.opt)…”按引擎语法读取 UTF-8 文件，将已识别选项合并到 GUI，同名值以导入文件最后一次为准；文件含完整输入时替换当前输入列表。未知选项、查询选项、嵌套引用或不可表示的语法，可保留原文件为兼容引用。兼容引用的 GUI 值仍后发、可能覆盖文件内容；用“查看配置来源与合并结果”检查来源，未知内容会明确标成无法完整解析。该预览不是引擎运行保证。

“导出引擎参数文件 (.opt)…”保存当前有效比较参数及启用输入，不保留 GUI 停用条目、原始继承状态或输入类型元数据；需要这些内容请保存 `.vcgui`。存在外部引用时需先导入为 GUI 设置再导出，避免生成引擎不会递归展开的文件。独立空参数、以 `-` 开头且必须用 `--` 保护的位置输入等无法无损导出时会拒绝操作，不截断内容。

引擎的 `.opt` 中，引号外 `#` 会截断后续**全文**，独立 `""` 会消失；不要将其当作通用 shell 配置。导出文件使用正确转义且不写文件级 `--`，避免将引擎后续启动参数误当视频。直接从 PowerShell 使用导出文件时：

```powershell
& 'D:\engine\video-compare.exe' --no-auto-options-file --options-file 'D:\configs\comparison.opt'
```

GUI 启动时，中文显式 `.opt` 路径会在 `work/config-cache/` 创建字节相同的副本，以兼容原引擎的窄字符文件接口；工作目录不变。超长命令行可无损表达时也在该目录暂存参数；空参数等不能无损转存的情况会阻止启动并说明原因。暂存目录需为 ASCII 路径；若启动器位于中文路径且遇到此兼容需求，会提示移动启动器，绝不修改引擎。

## 独立查询与复验

“全部设置 → 能力查询”支持帮助、版本、播放器操作说明、滤镜、协议、解封装器、解码器和硬件加速八类查询。搜索留空可列出全部；查询不要求有效视频，10 秒超时，取消/关闭设置页只结束该查询。结果区先显示状态及输出，下方附参数；日志保存在 `work/queries/`。帮助查询额外保存 `compatibility.txt`，列出源码基线中未发现及基线外选项，不自动删除参数。

完整 P06 自动验收：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\gui-launcher\tools\verify-p06.ps1
```

指定真实引擎复验查询，可加视频验证中文参数文件及导出文件的实际播放：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\gui-launcher\tools\verify-config-gui.ps1 -Engine 'D:\engine\video-compare.exe' -Left 'D:\videos\left.mp4' -Right 'D:\videos\right.mp4'
```

结果在 `work/p06/`。`work/queries/`、`work/runs/`、`work/config-cache/` 不自动清理；所有相关任务结束后可自行清理。循环选项已标注“缓冲区、默认 50 帧”，整段顺序播放仍应选择“连续播放（不循环）”。

## P07 实机联调

已增加生产 GUI 启动路径与直接 CLI 的逐项对照程序，覆盖布局、滤镜、色彩、逐右侧覆盖、CUDA、AV1、VMAF、配置及特殊输入。记录见 [P07 验收](P07-ACCEPTANCE.md) 和 [111 项结果登记](P07-RESULTS.md)。本轮为**部分验收**：完整交互及物理显示条件仍有待验项，不宣称所有模式已完成完整实机验收。

复验入口为 `tools/verify-p07.ps1`，需要 `-Engine` 和素材生成工具 `-FFmpeg`；FFmpeg 仅为测试素材生成依赖，GUI 启动器不依赖它。测试会创建并操作原比较窗口，结果留在 `work/p07/`。P08 已完成便携发布，其通过不代表 P07 未验证项自动通过。

安全续验已补齐帮助/HUD/隐藏左右、差异状态及正常退出；全局键鼠注入保持禁用，P07 入口会检查这一边界。当前剩余组合键、鼠标及显示设备项目按 [手动验收清单](P07-MANUAL.md) 记录，历史失败不能用“跳过”或“受阻”覆盖成通过。

## 目录边界

所有新源代码、资源、构建脚本、测试、文档和产物均放在 `gui-launcher/`。原项目的 `src/`、`tests/`、`makefile`、根 README、依赖与 CI 保持不变。

Git 提交包含 `src/`、`resources/`、`tests/`、`tools/`，以及 CMake 配置、阶段计划、验收记录、历史结果和维护文档。`release/` 是发布模板，按本项目约定仅保留在本机，不提交 Git；`releases/` 是生成的压缩包目录，同样不提交。

局部 `.gitignore` 排除 `build/`、`dist/`、`work/`、`release/`、`releases/`、`.vs/` 和个人 CMake 配置，并显式保留被根目录通配规则忽略的 CMake 配置及独立源码包中的 `LICENSE.txt`。从 Git 全新检出可构建和测试启动器；运行 `tools/package.ps1` 前需另行准备本机 `release/` 模板。

`build/`、`dist/` 可通过上述构建命令重新生成，清理前应退出其中运行的程序。`work/` 包含日志、测试证据及会话，清理前应确认需要保留的记录。不要使用 `git add -f` 强行加入生成目录；提交前可在项目根目录执行 `git status --short --untracked-files=all -- gui-launcher` 核对实际文件清单。

本次仓库整理保留阶段验收结论与历史结果，删除可重新生成的 `work/` 原始日志、素材、截图和验收证据。历史文档中的证据路径是原运行记录，在重新执行对应测试前可能不存在；历史通过状态不代表当前检出已重新完成实机验收。

已采用 C++17、Win32 Unicode、Windows Common Controls、独立 CMake 工程和 MSVC x64，静态链接 C++ 运行库。窗口使用系统默认图标；定制图标尚未制作。后续使用现有比较程序及其配套 DLL，不假设 MSVC 可以直接构建原有 MinGW 比较项目。

## 持续改进建议

- 保存一份常用 `.vcgui` 会话，P07 以该会话对照全部模式和多个右侧的实际切换行为。
