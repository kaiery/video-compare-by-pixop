# 源码选项 → GUI 控件 → 参数作用范围 → 验收用例

基线：2026-09-19，Git `dcdbefcdf7900659cb1a1f2631edbccfdcb1398c`。
权威来源：`../src/main.cpp` 的参数定义、配置装配、`apply_right_video_spec`；运行操作来源：`../src/controls.cpp`。

状态：P02 已完成全部 61 CLI / 11 RV 的参数层，P03/P04 已完成相应输入、常用和分类控件。完整实际控件 ID、作用范围及用例见 [P04-CONTROLS.md](P04-CONTROLS.md)，实测记录见 [P04-ACCEPTANCE.md](P04-ACCEPTANCE.md)。P05 已完成进程与日志，并用真实引擎验证基本 1+1 加载/停止，见 [P05-ACCEPTANCE.md](P05-ACCEPTANCE.md)。P06 已完成会话、参数文件和八类独立查询，见 [P06-ACCEPTANCE.md](P06-ACCEPTANCE.md)。全部比较选项逐项实机及设备验收仍为**待执行**。

P05 运行控件与验收索引（不替代下方 61+11 逐项实机要求）：

| 覆盖 ID | GUI 控件 | 参数作用范围 | 验收用例与证据 | 当前状态 |
|---|---|---|---|---|
| GUI-001 | 引擎路径 1001、浏览 1002、检查 1046 | 引擎位置；检查仅 Q，发出 `--version` | `process.lifecycle` 缺失/非法 EXE、真实缺 DLL；真实 GUI 文件选择及版本检查 | P05 已验收 |
| GUI-003 | 开始比较 1043，参数预览 1040 | 当前会话 G/B/L/R/Ri 按 P02 规则形成启动快照 | 替身原样记录 Unicode/引号/空值 argv；真实 1+1 启动；任意复杂滤镜实际语义仍待 P07 | 进程转义已验收；全模式实机未完成 |
| GUI-004 | 运行日志 1047，日志 1301，跟随刷新 1305 | 本次任务 stdout/stderr；不增加引擎选项 | 双路各约 12 MiB、超长行、UTF-8 分段/坏字节、4 MiB 轮转、65,536 字符上限 | P05 已验收 |
| GUI-005 | 状态 1300、检查引擎 1046 | 本次任务 PID/退出码；版本检查 10 秒超时 | 退出 0/23/259、DLL 状态码、检查超时、失败重试；真实退出 0 | P05 已验收 |
| GUI-006 | 正常停止 1302、强制结束 1303、主窗口关闭三选项 | 仅本实例创建的引擎进程；保留时由运行助手读管道 | 三份 GUI 各 14 项检查，含取消关闭、保留运行、停止后关闭、超时后确认强杀 | P05 已验收 |
| GUI-009 | 开始/停止及独立会话日志目录 | 每次请求独立标识和生命周期 | 原生测试十轮启动/结束与句柄计数、关闭控制器后持续排空 | P05 已验收 |

实现：[运行核心](src/process/runtime.cpp)、[运行助手](src/process/runner.cpp)、[主窗口绑定](src/main.cpp)；测试：[进程用例](tests/process_tests.cpp)、[GUI 操作](tools/verify-process-gui.ps1)。测试替身与真实媒体的证据分别保存，不混记全部引擎能力为通过。

P06 配置与查询验收索引：

| 覆盖 ID / 源码选项 | GUI 控件 | 参数作用范围 | 验收用例与结果 |
|---|---|---|---|
| GUI-007；全部会话字段 | 文件菜单保存 40003、打开 40004 | Session 全部层级、顺序、启用状态和 schema | `config.storage` 138 项/配置；三份 GUI 文件框与损坏文件不替换检查 PASS |
| IN-008/009；CLI-005/060 | 导入 40005、导出 40006、来源与合并结果 40007；配置页列表 | 自动文件→显式文件→GUI，布尔存在语义/有值覆盖 | 分词/引号/全文注释/空值边界；Unicode 暂存；真实引擎直接读取导出文件 PASS |
| CLI-001/002/003/035/046/050/054/058 | 查询基址 +2 执行、+3 取消；搜索基址 +1 | 独立 Q，无有效视频也可查询；不修改比较 Session | 三份 GUI 与真实引擎均 PASS；五类空搜索、关键词、无匹配词；帮助声明 61 项匹配 |
| GUI-002/005/006；查询生命周期 | 查询结果区、取消按钮、关闭设置页 | 独立查询进程，10 秒超时，比较任务不受影响 | 比较中查询版本、取消及超时后比较仍存活，随后正常停止 PASS |
| IN-012 / Windows 命令长度 | 开始比较、session.txt 实际命令记录 | 可无损时 `.opt` 转存；否则拒绝并说明原因 | 超长参数值不截断；空 argv 与危险位置参数不静默改变；参数文件级 `--` 避免污染启动 CLI |

实现为 [storage.cpp](src/core/storage.cpp)、[main.cpp](src/main.cpp)、[settings.cpp](src/ui/settings.cpp)。细项表和证据见 [P06 验收](P06-ACCEPTANCE.md)。P07 仍须逐项验证全部比较模式及设备条件。

P02 实现索引：

| 覆盖 ID | 实现引用 | 独立期望值与测试 | 当前状态 |
|---|---|---|---|
| CLI-001–CLI-061 | [选项目录](src/core/catalog.cpp)、[模型赋值](src/core/model.cpp)、[参数生成](src/core/build_plan.cpp) | [61 项独立样例](tests/cli_cases.inc)，`core.catalog` 中每个 ID 单独输出 PASS | 参数层 PASS；控件见 P03/P04；引擎实测 TODO |
| RV-001–RV-011 | [覆盖定义](src/core/catalog.cpp)、[逐视频构造](src/core/build_plan.cpp) | [11 项独立样例](tests/override_cases.inc)，`core.overrides` 验证替换/清空/继承及输入隔离 | 参数层 PASS；11 项控件见 P04；引擎实测 TODO |
| IN / GUI 的参数相关部分 | [模型](src/core/model.h)、[Windows 转义](src/core/command_line.cpp) | [语义与转义测试](tests/core_tests.cpp)，`core.semantics` / `core.quoting` | 详见 [P02 验收](P02-ACCEPTANCE.md)，不整体标记 IN/GUI 通过 |

`Emission` 会把每个发出参数的稳定 ID、作用字段和 argv 索引关联起来；源码/文档/代码目录/样例 ID 由 `tools/verify-coverage.ps1` 交叉核验。

P03 控件实现索引（P04 已补齐其余设置，实际运行继续按 P05–P07 执行）：

| 覆盖 ID | GUI 控件 / 参数作用范围 | 实现与验收依据 | 当前状态 |
|---|---|---|---|
| CLI-006、007、010、013 | `IDC_FULLSCREEN` / `IDC_HIGH_DPI` / `IDC_TEN_BIT` / `IDC_DIFFERENCE`；G | [主界面](src/main.cpp) `refresh_preview`；[控件测试](tools/verify-inputs.ps1) 开关及复位 | GUI 参数 PASS；设备/引擎 TODO |
| CLI-015、020 | `IDC_LAYOUT` / `IDC_LOOP`；G | 三种布局、三种循环进入参数预览 | GUI 参数 PASS；引擎 TODO |
| CLI-016、017 | `IDC_WINDOW_MODE` / `IDC_WIDTH` / `IDC_HEIGHT`；G | 窗口策略互斥、单边尺寸、非法尺寸 | GUI 参数 PASS；引擎 TODO |
| CLI-022 | `IDC_TIMESHIFT`；G→R，所有右侧共享 | 完整表达式及留空复位 | GUI 参数 PASS；引擎 TODO |
| RV-001 | `IDC_FILTER_MODE` / `IDC_FILTER_TEXT`；Ri | [条目对话框](resources/app.rc)，继承/替换/追加/清空，复制与排序保持配置 | GUI 参数 PASS；公共滤镜控件在 P04，实际滤镜运行 TODO |
| IN-001–005、010–011 的编辑部分 | 引擎/工作目录/左侧输入/多选右侧列表；会话、L、Ri | [原生事件测试](tests/ui_events.cpp) 与真实 EXE 控件测试；[P03 对应表](P03-ACCEPTANCE.md) | 1+1、1+3 编辑 PASS；媒体能力/1+11 运行/引擎快捷切换 TODO |
| IN-007 的保留分隔符、IN-012 的预览部分 | 条目编辑器、`IDC_PREVIEW`；Ri/会话 | 沿用 P02 参数校验和转义，不通过 shell 执行预览 | 参数层 PASS；真实子进程在 P05/P07 |
| GUI-008、011 的 P03 部分 | Unicode Win32 控件、滚动主窗口、独立构建 | 96 DPI 下窗口截图、三份 EXE 控件及关闭检查 | 本机 PASS；150%/200% DPI 与完整发布 TODO |

`CLI-060` 默认禁用自动 `.opt` 加载沿用 P02 策略，P04 已在配置页提供反向映射的“允许加载自动 .opt”控件，参数预览显示外部配置警告；P06 已提供文件菜单导入/导出、来源与合并结果检查，无法解析的内容明确标识。

P04 分类页由 [设置目录](src/ui/settings_catalog.inc) 和 [控件绑定](src/ui/settings.cpp) 实现；[逐项控件测试](tests/settings_events.cpp) 从独立样例验证提交结果，[真实 EXE 检查](tools/verify-settings.ps1) 验证主窗口与嵌套条目编辑器联通。`tools/verify-settings-coverage.ps1` 核对 61＋11 表项，不能替代行为测试。

范围缩写：G=会话全局；B=公共输入设置；L=左侧；R=所有右侧默认；Ri=某个右侧；Q=独立查询，不启动比较。CLI 表中有些左右共用参数由 GUI 分栏编辑后编码为 `左:右`；Ri 仅通过后续 RV 表支持。

通用验收：每行先检查参数数组、值、作用范围，再用实际引擎比较与等价 CLI 的一致性。有值选项测试默认/有效值/无效值；无值选项测试选中/未选中；未指定值时保留引擎默认行为。查询测试不提供视频输入。

## A. 全部命令行选项（61 项）

下表“源码键”是 argagg 内部名称，不一定等于长选项名。别名列必须全部保留以核验完整性；GUI 通常输出长选项。

| ID | 源码键 | CLI 别名 | GUI 控件 | 作用范围 | 验收用例与预期 |
|---|---|---|---|---|---|
| CLI-001 | `help` | `-h`, `--help` | 帮助：引擎选项 | Q | 无输入显示当前引擎帮助，不打开比较窗口 |
| CLI-002 | `version` | `-V`, `--version` | 引擎版本/重新检测 | Q | 显示所选 exe 的版本；更换引擎后更新 |
| CLI-003 | `show-controls` | `-c`, `--show-controls` | 比较窗口操作说明 | Q | 完整显示引擎快捷键及鼠标说明，不启动播放 |
| CLI-004 | `verbose` | `-v`, `--verbose` | 详细日志复选框 | G | 启用后捕获参数、库与渲染诊断，关闭时不附加该标志 |
| CLI-005 | `options-file` | `-o`, `--options-file` | 有序参数文件列表、添加/移除 | G/配置来源 | 多文件按顺序加载；CLI 有值覆盖文件值；不存在、引号不闭合、中文路径有明确结果 |
| CLI-006 | `fullscreen` | `-u`, `--fullscreen` | 启动全屏复选框 | G | 默认窗口/勾选桌面全屏；退出全屏沿用播放器操作 |
| CLI-007 | `high-dpi` | `-d`, `--high-dpi` | 高 DPI 复选框 | G | 高缩放屏幕下与 CLI 的像素/窗口行为一致；不改变 GUI 自身缩放设置 |
| CLI-008 | `font` | `-g`, `--font` | 字体：auto/scp/sarasa/文件 | G | 三种内置选择及中文路径字体；缺失文件错误可见；作用于比较窗口 |
| CLI-009 | `ui-scale` | `-U`, `--ui-scale` | 比较窗口文字缩放数值 | G | 1、1.25 生效；0、负数、非数值阻止启动 |
| CLI-010 | `10-bpc` | `-b`, `--10-bpc` | 10 位色深复选框 | G | 参数开关正确；支持设备实际对照；不支持设备不宣称 10 位成功 |
| CLI-011 | `fast-alignment` | `-F`, `--fast-alignment` | 输入对齐：高质量/快速 | G | 不同分辨率输入下与 CLI 插值一致；不得与 PNG 快捷键 F 混淆 |
| CLI-012 | `bilinear-texture` | `-I`, `--bilinear-texture` | 纹理插值：最近邻/双线性 | G | 放大素材比较边缘；参数与运行快捷键独立说明 |
| CLI-013 | `subtraction-mode` | `-S`, `--subtraction-mode` | 启动差异视图 | G | 相同视频差异结果与 CLI 一致；三种布局分别验证 |
| CLI-014 | `display-number` | `-n`, `--display-number` | 显示器列表/编号 | G | 0、第二屏与 CLI 一致；负值拒绝，失效显示器报告错误 |
| CLI-015 | `display-mode` | `-m`, `--mode` | split/hstack/vstack 下拉框 | G | 三种布局逐项启动；多右侧时仍是参考与活动右侧比较 |
| CLI-016 | `window-size` | `-w`, `--window-size` | 自定义宽高、允许单边留空 | G | 1280x720、1280x、x720；不可与适应屏幕并用；空尺寸拒绝 |
| CLI-017 | `window-fit-display` | `-W`, `--window-fit-display` | 窗口策略：适应可用屏幕 | G | 保持比例适应任务栏外区域；选择后禁用自定义尺寸 |
| CLI-018 | `aspect-lock` | `-k`, `--aspect-lock` | off/window/content 下拉框 | G | 三种模式拖动窗口；锁定行为与 CLI 相同 |
| CLI-019 | `aspect-view-mode` | `-x`, `--aspect-view-mode` | stretch/original/dynamic/16:9/4:3/1:1 | G | 六种值对照；导入 16x9/4x3/1x1 别名；dynamic 交换画面后参考语义一致 |
| CLI-020 | `auto-loop-mode` | `-a`, `--auto-loop-mode` | 连续/缓冲区正向/缓冲区往返 | G | off/on/pp 全验；长视频填满缓冲即循环，不标成整片循环 |
| CLI-021 | `frame-buffer-size` | `-f`, `--frame-buffer-size` | 缓冲帧数 | G | 1、50、150；0、负数、非整数拒绝；大值不使 GUI 卡死 |
| CLI-022 | `time-shift` | `-t`, `--time-shift` | 秒数/完整时间表达式 | G→R | 0.150、-0.1、x1.04+0.1、x25.025/24-1:30.5 与 CLI 一致；不提供 Ri 偏移 |
| CLI-023 | `wheel-sensitivity` | `-s`, `--wheel-sensitivity` | 滚轮灵敏度有符号数值 | G | 0.5、-1、1.7；负值反向，0 按原程序语义保留 |
| CLI-024 | `color-space` | `-C`, `--color-space` | 左右色彩矩阵可编辑选择 | L/R（Ri 见 RV） | bt709、bt2020nc:、不同左右值编码正确；单侧空值不污染另一侧 |
| CLI-025 | `color-range` | `-A`, `--color-range` | 左右范围 auto/tv/pc | L/R（Ri 见 RV） | tv、:pc、pc:tv 对照；auto 以省略/空侧正确表示 |
| CLI-026 | `color-primaries` | `-P`, `--color-primaries` | 左右原色可编辑选择 | L/R（Ri 见 RV） | bt709、bt2020:bt709 对照；手动值原样传递 |
| CLI-027 | `color-trc` | `-N`, `--color-trc` | 左右传递函数可编辑选择 | L/R（Ri 见 RV） | bt709、smpte2084: 对照；无效值错误可见 |
| CLI-028 | `tone-map-mode` | `-T`, `--tone-map-mode` | 左右 auto/off/on/rel | L/R（Ri 见 RV） | 四种模式、auto:off、:rel；SDR/HDR 对照 CLI |
| CLI-029 | `left-peak-nits` | `-L`, `--left-peak-nits` | 左侧峰值亮度/默认 | L | 默认不传，850、1000；0、10001 拒绝；不得标为右侧独立值 |
| CLI-030 | `right-peak-nits` | `-R`, `--right-peak-nits` | 右侧公共峰值亮度/默认 | R（Ri 见 RV） | 1、10000 边界及默认；切换多个右侧保持公共语义 |
| CLI-031 | `boost-tone` | `-B`, `--boost-tone` | 左右映射强度 | L/R（Ri 见 RV） | 0.6、:3、2:1.5；0 按源码允许值处理，不误限为严格正数 |
| CLI-032 | `filters` | `-i`, `--filters` | 公共滤镜编辑器 | B | scale=1920:-2 等完整表达式传递；引号、逗号、反斜杠测试 |
| CLI-033 | `left-filters` | `-l`, `--left-filters` | 左滤镜：覆盖/引用公共 | L | format=gray 及 __,scale=iw/2:ih/2；确认公共引用只按引擎语义替换 |
| CLI-034 | `right-filters` | `-r`, `--right-filters` | 右侧公共滤镜 | R | 所有右侧继承；Ri 追加、替换、清空另见 RV-001 |
| CLI-035 | `find-filters` | `--find-filters` | 滤镜搜索/列出全部 | Q | scale 关键词和空字符串；空 argv 保留，不转成缺少参数 |
| CLI-036 | `conversion-size` | `--conversion-size` | 共享画布 max/指定宽高 | G | max、1920x1440；不完整或零尺寸拒绝；与窗口尺寸分开 |
| CLI-037 | `conversion-fit` | `--conversion-fit` | stretch/native 下拉框 | G | 不同尺寸输入验证填满与 1:1 居中黑边；与 aspect-view 分开 |
| CLI-038 | `histogram-window` | `--histogram-window` | 启动直方图窗口 | G | 单独及与其他 scopes 组合打开，仍可用 F1 切换 |
| CLI-039 | `vectorscope-window` | `--vectorscope-window` | 启动矢量示波器 | G | 单独/组合打开；F2 正常 |
| CLI-040 | `waveform-window` | `--waveform-window` | 启动波形窗口 | G | 单独/组合打开；F3 正常 |
| CLI-041 | `histogram-options` | `--histogram-options` | 直方图参数文本 | G/直方图 | display_mode=parade 等表达式完整传递；无效参数可诊断 |
| CLI-042 | `vectorscope-options` | `--vectorscope-options` | 矢量示波器参数文本 | G/矢量图 | mode=color4:graticule=green；加号、冒号不损坏 |
| CLI-043 | `waveform-options` | `--waveform-options` | 波形参数文本 | G/波形图 | graticule=orange:display=stack:scale=ire；与 CLI 一致 |
| CLI-044 | `scope-size` | `--scope-size` | 分析窗口宽高 | G/scopes | 1024x256、替代有效尺寸；注明为双面板总尺寸；不完整尺寸拒绝 |
| CLI-045 | `scope-notop` | `--scope-notop` | 分析窗口置顶复选框 | G/scopes | 默认勾选不传；取消勾选传 --scope-notop，验证反向映射 |
| CLI-046 | `find-protocols` | `--find-protocols` | 输入协议搜索 | Q | 关键词、空搜索、不匹配结果；不要求本地文件 |
| CLI-047 | `demuxer` | `--demuxer` | 公共解复用器及参数 | B | rawvideo:pixel_format=rgb24,video_size=320x240,framerate=10；适配原始媒体测试 |
| CLI-048 | `left-demuxer` | `--left-demuxer` | 左侧解复用器/继承表达式 | L | 左侧覆盖不影响右侧；__ 引用对照；脚本需对应引擎能力 |
| CLI-049 | `right-demuxer` | `--right-demuxer` | 右侧公共解复用器 | R | 多右侧继承与 Ri 覆盖；复合参数保留 |
| CLI-050 | `find-demuxers` | `--find-demuxers` | 解复用器搜索 | Q | matroska 与空搜索；查询输出按所选引擎更新 |
| CLI-051 | `decoder` | `--decoder` | 公共解码器及选项 | B | h264 或 :strict=experimental；名称为空但有参数不丢失 |
| CLI-052 | `left-decoder` | `--left-decoder` | 左侧解码器及选项 | L | 左侧指定不影响右侧；错误名称反馈完整 |
| CLI-053 | `right-decoder` | `--right-decoder` | 右侧公共解码器及选项 | R | 多右侧公共配置与 Ri 覆盖独立；不同引擎能力正确报告 |
| CLI-054 | `find-decoders` | `--find-decoders` | 解码器搜索 | Q | h264、hevc、空搜索；查询结果不是设备可用承诺 |
| CLI-055 | `hwaccel` | `--hwaccel` | 公共硬件类型/设备/选项 | B | 完整 type:device:options 编码；支持环境实际解码，失败日志可见 |
| CLI-056 | `left-hwaccel` | `--left-hwaccel` | 左侧硬件配置 | L | 只改变左侧；设备编号完整；不按操作系统名称硬编码能力 |
| CLI-057 | `right-hwaccel` | `--right-hwaccel` | 右侧公共硬件配置 | R | 多右侧与 Ri 混合配置测试；无设备环境记录条件受限 |
| CLI-058 | `find-hwaccels` | `--find-hwaccels` | 硬件加速搜索 | Q | cuda 与空搜索；显示为编译支持类型，不宣称硬件已就绪 |
| CLI-059 | `libvmaf-options` | `--libvmaf-options` | VMAF 参数文本 | G/指标 | model=version=vmaf_4k_v0.6.1；转义/多模型表达式完整；在比较窗口按 M 执行，缺能力明确提示 |
| CLI-060 | `disable-auto-options-file` | `--no-auto-options-file` | 加载工作目录自动配置 | G/配置来源 | 默认 GUI 管理模式禁用自动加载；启用兼容模式正确反向映射；不同工作目录验证 |
| CLI-061 | `disable-auto-filters` | `--no-auto-filters` | 启用引擎自动滤镜 | G/所有输入 | 默认启用；取消才传参数；与清空用户滤镜区别清楚 |

## B. 每个右侧视频的全部覆盖字段（11 项）

格式为单个位置参数 `文件路径::key=value::key=value`。整个条目作为一个 argv 元素。源码仅处理以下键；不要依据 README 的概括允许任意 CLI 键出现在 `::` 后。

| ID | 源码覆盖键 | GUI 控件 | 作用范围 | 验收用例与预期 |
|---|---|---|---|---|
| RV-001 | `filters` | 条目滤镜：继承/替换/追加/清空 | Ri | A 继承，B `::filters=__,scale=1920:-1`，C `::filters=`；切换核验，清空不关闭引擎自动滤镜 |
| RV-002 | `color-space` | 条目色彩矩阵覆盖 | Ri | `::color-space=bt709` 只影响该条目；空值按引擎语义测试 |
| RV-003 | `color-range` | 条目范围覆盖 | Ri | `::color-range=pc`；其他条目继承 R |
| RV-004 | `color-primaries` | 条目原色覆盖 | Ri | `::color-primaries=bt2020`；保存会话后不丢失 |
| RV-005 | `color-trc` | 条目传递函数覆盖 | Ri | `::color-trc=smpte2084`；与同参数 CLI 对照 |
| RV-006 | `decoder` | 条目解码器及选项覆盖 | Ri | `::decoder=libdav1d:export_side_data=film_grain`，需支持媒体；验证继承字典合并，不虚构全部重置 |
| RV-007 | `demuxer` | 条目解复用器及选项覆盖 | Ri | 有效类型/选项与 CLI 对照；名称覆盖和选项继承分别核验 |
| RV-008 | `hwaccel` | 条目硬件类型、设备及选项 | Ri | 支持环境 `::hwaccel=cuda`；清空/设备表达式按引擎对照，不误当 G |
| RV-009 | `tone-map-mode` | 条目 auto/off/on/rel | Ri | 四种值各测；混合 SDR/HDR 条目切换与 CLI 一致 |
| RV-010 | `peak-nits` | 条目峰值亮度 | Ri | `::peak-nits=850`；1/10000 边界；恢复继承删除覆盖键 |
| RV-011 | `boost-tone` | 条目映射强度 | Ri | `::boost-tone=1.5` 及 0；负值拒绝；其他条目不变 |

## C. 输入语法与配置来源

| ID | 源码行为/输入 | GUI 控件或规则 | 作用范围 | 验收用例与预期 |
|---|---|---|---|---|
| IN-001 | `LEFT RIGHT [RIGHT2 ...]` | 左侧选择＋右侧多选列表 | 输入会话 | 1+1、1+3、1+11；argv 顺序正确，Tab 可遍历，前十项快捷切换可用 |
| IN-002 | 右侧省略不合法 | 启用勾选、删除、开始按钮状态 | 输入会话 | 无左侧或无启用右侧阻止启动；停用条目不进入 argv |
| IN-003 | 文件路径 | 系统对话框、拖入、手动输入 | L/Ri | 中文、空格、&、括号、#、反斜杠；替身校验精确 argv；不经 shell |
| IN-004 | 图片/序列/协议/脚本 | 输入类型＋可编辑地址＋所有文件过滤 | L/Ri | 图片对比、编号序列、支持协议、脚本；不一律用文件存在检查拒绝 |
| IN-005 | `__` 文件占位符 | 使用参考视频/手工兼容输入 | L/Ri | 左/右单个占位符按引擎解析；两侧均 __ 拒绝；多个右侧复用参考 |
| IN-006 | 参数 `__` 引用 | 继承/追加及专家表达式 | B/L/R/Ri | 公共→侧边→条目；左右峰值亮度互相引用；双方未解决占位符报错；不当作通用文本宏无限替换 |
| IN-007 | `::` 分隔 | 条目覆盖编辑器 | Ri | 文件路径加多键为一个 argv；未知键拒绝；原始地址含 :: 报原引擎表达限制 |
| IN-008 | 参数文件优先级 | 配置来源列表/最终摘要 | G | 自动文件→多个显式文件→CLI；有值参数最后生效，布尔不能用省略撤销 |
| IN-009 | `.opt` 分词 | 兼容导入导出 | G | 单/双引号、转义、未闭合、引号外 # 截断后续全文、独立空值丢失；不能套通用 shell 分词 |
| IN-010 | 工作目录影响读取/输出 | 工作目录选择、路径规范化 | 会话 | 默认子目录 work；相对媒体、配置、字体和截图路径可预测；原目录无新增文件 |
| IN-011 | 多条目重排/复制 | 上移下移/拖动排序/复制设置 | Ri | 排序后覆盖随条目移动；同名不同目录明确区分；重复路径不同滤镜可保留 |
| IN-012 | 命令长度/复杂转义 | 预检、等价命令查看 | 会话 | 大量右侧/长滤镜触及长度阈值；无损参数文件或明确拒绝；不截断，不把预览当 shell 执行源 |

## D. 原比较窗口保留的运行能力

本节验收的是“GUI 启动后原有操作仍可用、说明完整、日志可接收”，不是给启动器增加控制协议。快捷键完整文本由 CLI-003 从所选引擎读取；下表按功能分组，组合修饰键以引擎帮助为准。

| ID | 运行能力 | 原窗口操作 | 验收用例与预期 |
|---|---|---|---|
| RUN-001 | 帮助、视频信息、HUD、隐藏左右 | H/V/1/2/3 | 切换正常；启动器帮助展示完整控制说明 |
| RUN-002 | 暂停、缓冲循环、变速 | Space、逗号、句点、J/L | 正常操作，Ctrl/Shift 细调说明保留 |
| RUN-003 | 寻址、逐帧、时间剪贴板 | 方向键、PageUp/Down、A/D、Shift+A/D、Ctrl+C/V | 正常寻址与帧导航；不被启动器抢占快捷键 |
| RUN-004 | 放大镜、缩放、平移、复位 | Z/C/E/R、4..9、滚轮、右键拖动 | 交互与直接 CLI 启动一致 |
| RUN-005 | 左右交换、切换右侧 | S、Tab、Ctrl+Shift+1..0 | 1+3 及 1+11 条目序列；切换后独立设置正确 |
| RUN-006 | 差异视图、子模式、亮度差异 | 0/Y/U | 每种可用子模式正常；仅启动差异开关在 GUI 有 CLI 映射 |
| RUN-007 | 时间微调 | +/-、Ctrl 或 Alt 组合 | 原窗口支持的微调有效；不声称逐条初始偏移可配置 |
| RUN-008 | 像素/指标/FPS/显示状态 | P/M/X、Shift+X | 文本进入日志；可用指标正确，缺 VMAF 能力诊断可见 |
| RUN-009 | 截图及选区导出 | F、Shift+F | 文件进入选定工作目录；路径可找到，失败日志保留 |
| RUN-010 | 插值、布局、比例、全屏 | I/T、Shift+M/S、Alt+Enter | 运行切换保留，不修改启动器已保存的初始配置 |
| RUN-011 | 裁剪、复制裁剪、清除、撤销 | Shift/Ctrl+L/R/B、Backspace 等 | 完整组合参照引擎帮助；不在 GUI 承诺裁剪状态实时同步 |
| RUN-012 | 分析窗口 | F1/F2/F3 | 三窗口打开关闭可用；已配置 options 与启动行为一致 |
| RUN-013 | 窗口尺寸保存与恢复 | Shift+W、Ctrl+W、Ctrl+Shift+W | 同一进程内保存恢复有效；当前源码仅保存内存状态，不承诺跨启动恢复 |
| RUN-014 | 鼠标分割和定位 | 水平移动、左键寻址 | 分割、寻址、修饰键行为与 CLI 一致 |
| RUN-015 | 正常退出 | Escape/关闭窗口 | GUI 获取退出码并恢复可启动状态，配置与日志保留 |

## E. 启动器专项验收

| ID | 场景 | 预期 |
|---|---|---|
| GUI-001 | 引擎未配置/不存在/DLL 缺失/不是可运行 exe | 可读错误；不显示已加载视频；允许更正路径重试 |
| GUI-002 | 引擎版本与基线不一致 | 查询并显示缺失/未知能力，不偷偷丢参数 |
| GUI-003 | 中文路径、引号、空参数、复杂 FFmpeg 值 | 替身接收 argv 逐元素相等；真实引擎行为与 CLI 对照 |
| GUI-004 | stdout/stderr 大量输出、长行、UTF-8 分段 | UI 响应、管道持续排空、日志限额有效；stderr 信息不全标错误 |
| GUI-005 | 启动后立即失败、正常退出、查询超时 | 状态与退出码正确；重试可用；查询超时不误杀比较会话 |
| GUI-006 | 停止、关闭 GUI、保留比较运行 | 只操作所属进程；保留时日志管道不堵塞；正常退出优先于强杀 |
| GUI-007 | 保存/加载会话、旧版本/损坏配置 | 输入顺序、启用状态、继承、空值、工作目录无损；损坏不覆盖当前会话 |
| GUI-008 | GUI 100%/150%/200% DPI、窗口缩小、键盘导航 | 常用操作可见、分类页可滚动、焦点顺序明确；不依赖 MFC |
| GUI-009 | 多次开始/关闭、十轮重复运行 | 无累计线程/句柄泄漏、日志串会话或僵尸进程 |
| GUI-010 | 默认值与配置文件中的布尔开关冲突 | 显示来源与实际语义；不存在“界面关了，实际仍开着”而无说明的情况 |
| GUI-011 | 独立构建与发布 | x64 Debug/Release；解压运行；只增加子目录内容，原 makefile/src/tests 等零修改 |
| GUI-012 | 原功能与 GUI 对照 | 使用同一引擎、媒体、工作目录及参数；模式、滤镜、截图、日志结果匹配 |

## F. 结果记录模板

P07 当前实测结果见 [111 项登记表](P07-RESULTS.md)；状态与证据范围见 [P07-ACCEPTANCE.md](P07-ACCEPTANCE.md)。未完成完整交互观察的运行组不因按键冒烟通过而自动验收。

每行验收至少一条记录；复杂选项按取值拆分子用例，如 CLI-015/split。

| 用例 ID/子用例 | 控件/代码引用 | 引擎版本/环境 | 输入/会话 | argv 证据 | 实际观察 | 状态 | 证据文件 |
|---|---|---|---|---|---|---|---|
| 示例：CLI-015/split | 待实现 | 待联调 | 待准备 | 待执行 | 未执行 | TODO | 无 |

状态：TODO / PASS / FAIL / BLOCKED / N/A。BLOCKED/N/A 必须有条件说明；硬件功能仅参数测试 PASS 不足以标实机 PASS。

## 持续改进建议

- 原项目更新后运行覆盖核验，人工复核默认值、合并语义、运行快捷键和新增边界；静态键覆盖不能发现所有行为变化。
- 将 GUI-003、IN-008/009 和 RV-001 优先实现为可重复自动测试，它们最容易造成“界面配置与实际启动不同”。
