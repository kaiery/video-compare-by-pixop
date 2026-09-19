# P02 数据与参数层接口

位置：`src/core/`。C++17 静态库目标 `launcher-core`，独立于原项目源码、FFmpeg/SDL 和 Win32 窗口。Windows 路径处理使用 C++ 标准库；最终参数转义针对 Windows `CommandLineToArgvW`。

此阶段只构造启动计划，不启动进程、不检查媒体是否可解码、不读写会话/参数文件。调用方必须在 P05/P06 完成实际引擎、文件与配置预检，不能把 `BuildResult::ok()` 当成“视频已加载”。

## 选项定义与作用范围

`option_catalog()` 包含 61 行，逐行记录覆盖表 ID、源码键、长/短别名、值类型、作用范围、模型字段、校验规则、枚举值和默认值提示。`override_catalog()` 包含 11 个 Ri 字段及其空值/追加能力。

- 默认值提示用于解释界面，未设置字段不会自动输出默认值，避免覆盖用户明确指定的配置来源。
- `find_option()` 支持稳定 ID、源码键和大小写敏感的 CLI 别名，例如 `CLI-015`、`display-mode`、`--mode`、`-m`。
- 比较选项和查询选项分开处理。8 个查询命令只能通过 `build_query()` 构造。
- 每个实际发出的选项都有 `Emission`，包含覆盖 ID、作用字段和 argv 索引。右侧字段使用稳定 ID，例如 `right[17].filters`。

## 会话结构

| 成员 | 含义 | 字段示例 |
|---|---|---|
| `schema_version` | 当前为 1；未知版本拒绝生成 | 1 |
| `engine_path` / `working_directory` | 引擎位置和绝对工作目录 | 不隐式依赖当前进程目录 |
| `global` | 全局选项，值为 `bool` 或 `wstring` | `fullscreen`、`window-size` |
| `common` | 两侧公共输入配置 | `filters`、`decoder`、`color-space` |
| `left` / `left_options` | 左侧来源及覆盖 | 输入源、`filters`、`peak-nits` |
| `right_defaults` | 全部右侧的默认输入配置 | `filters`、`tone-map-mode` |
| `right_inputs` | 保持顺序的右侧列表 | ID、启用状态、来源和逐项覆盖 |
| `configuration` | 自动配置开关及有序显式文件 | GUI 默认禁用自动 `.opt` |
| `next_input_id` | 下一可用稳定 ID | 不随删除/排序重置 |

输入类型包括 File、ImageSequence、Address、Script、Reference。普通文件、序列、脚本和自定义字体基于会话工作目录转换成绝对路径；Address 原样保留；Reference 解析为实际参考来源后传入引擎，避免左侧 `__` 引用混入首个右侧的 `::` 覆盖参数。

`add_right`、`duplicate_right`、`move_right`、`remove_right` 按稳定 ID 操作。复制条目产生新 ID，设置跟随条目移动；停用条目不生成输入/覆盖参数。无左侧或无启用右侧均拒绝生成。

## 参数修改与继承

`set_option(session, name, value, error)` 负责把选项定位到正确模型层；类型或作用范围错误时不修改会话。值是否合法在生成阶段统一验证，以支持界面保留尚未填写完整的编辑内容。

`unset_option` 恢复未设置状态。公共的 `color-space` 等成对参数可以用 `左:右` 赋值；直接编辑 `left_options` / `right_defaults` 时，每个字段只包含单侧值。

Ri 的四种模式：

| 模式 | 输出行为 |
|---|---|
| Inherit | 不输出该键；编辑框残留值不生效 |
| Replace | 输出 `::key=value`，允许支持的专家表达式 |
| Append | 仅支持滤镜；模板非空时输出 `::filters=__,value`；已知模板为空时直接输出 value |
| Clear | 输出 `::key=`；与删除覆盖键不同 |

Clear 的底层含义由引擎字段决定：滤镜为空、色彩空值、色调映射恢复 auto、peak-nits 空值解析为 0、boost-tone 空值为 1；解码器/解复用器/硬件配置中的已继承字典项可能保留。不可将 Clear 统一标成“彻底清除该模块所有配置”。

内部校验复核公共→左右→Ri 的 `__` 解析，生成时保留引擎原表达式，不重新排序或拆解 FFmpeg 参数字典。只有滤镜 Append 是高层操作，需要生成等价表达式。

## 生成结果与错误

```cpp
#include "core/model.h"

auto session = launcher::make_session(LR"(D:\ProjectAI\video-compare-by-pixop\gui-launcher)");
session.engine_path = LR"(D:\VideoCompare\video-compare.exe)";
session.left = {launcher::InputKind::File, LR"(D:\视频\参考.mp4)"};
launcher::add_right(session, {launcher::InputKind::File, LR"(D:\视频\输出 A.mp4)"});
launcher::add_right(session, {launcher::InputKind::File, LR"(D:\视频\输出 B.mp4)"});
session.right_defaults["filters"] = L"yadif";
session.right_inputs[1].overrides["filters"] = {
    launcher::OverrideMode::Append, L"scale=1920:-1"};

std::wstring error;
launcher::set_option(session, "CLI-015", std::wstring(L"hstack"), error);
auto result = launcher::build_comparison(session);
// result.plan 存在才有可用的参数计划；界面仍需展示 result.diagnostics 中的警告。
// result.plan->preview 可展示；command_line 只供直接进程创建，不交给 CMD/PowerShell。
```

默认双视频 argv 示意：

```text
argv[0] = D:\tools\video-compare.exe
argv[1] = --no-auto-options-file
argv[2] = --
argv[3] = D:\fixture\left.mp4
argv[4] = D:\fixture\right.mp4
```

`arguments` 包含 argv[0]，`command_line` 为 Windows 进程命令行，`preview` 为逐参数可读文本。控制字符在预览中显示为 `\n` / `\r` / `\t`，执行参数保留原值。

存在错误时 `plan` 为空，同时返回结构化诊断：代码、字段、严重程度和中文说明。错误包括未知字段、放错作用范围、类型错误、冲突尺寸、非法数值、未解析引用、无输入、重复 ID、无效 Unicode、`::` 不可表达值、超过 32767 UTF-16 单元（含结尾 NUL）的命令等。

## 明确保留的边界

- `.opt` 内容、优先级的实际解析和会话文件存储留给 P06。当前传入文件路径并提示 `W_EXTERNAL_CONFIG`；外部文件参与时不假装已知最终布尔状态或所有模板，不对未知模板进行提前替换。
- 文件是否存在、引擎兼容性、媒体/滤镜/设备能力留给 P05/P07。地址不能按本地文件规则强制检查存在性。
- 时间表达式原样传递。负时间分量使用原引擎的逐项相加语义，例如 `-1:30.5` 为 -29.5 秒，不是 -90.5 秒；建议使用负秒数。当前不重现 FFmpeg 对倍率的 AVRational 舍入与设备相关限制。
- 数值输入先限制在 256 字符内再校验，防止极长数字导致正则资源消耗；窗口和画布尺寸采用正整数要求，不接受引擎偶然容忍的畸形输入。
- 超长命令当前明确拒绝，不截断；参数文件的无损落盘及长命令回退留给 P06/P05。
- 视频比较模式和参数控件尚未接入窗口；参数预览以可调用 API 提供，界面展示在 P03/P04 接入。

## 持续改进建议

- P03 从本目录的定义和诊断字段绑定控件，避免在窗口代码里再次实现参数拼接或校验规则。
- P06 读取配置文件后补齐来源解析，再利用现有模型生成参数；保留文件中存在型布尔选项无法用省略撤销的语义。
