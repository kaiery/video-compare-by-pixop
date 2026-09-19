# P06 配置与查询验收

日期：2026-09-19。状态：**已验收（P06 范围）**。全部实现、测试、构建产物和记录在 `gui-launcher/`；未修改或链接原项目代码。

## 交付与作用范围

| 功能 / 源码选项 | GUI 控件 | 参数作用范围 | 验收用例 / 实现 |
|---|---|---|---|
| 会话保存、打开；GUI-007 | 文件菜单 40003 / 40004 | 整个 Session，含停用条目、稳定 ID、next ID、schema、配置来源和空值 | `storage::save_session/load_session`；所有非查询/非配置 CLI 分别往返；多输入/四种覆盖模式、损坏与非法结构 |
| `.opt` 导入、导出；IN-009 | 文件菜单 40005 / 40006 | 导入已识别设置；含位置输入则替换输入列表；导出启用输入及有效参数 | 原引擎语法的引号/转义、全文注释、空参数、逐右侧覆盖；真实引擎直接读取导出文件 |
| options-file、no-auto-options-file；IN-008 | 配置页 3100–3106、2590；文件菜单 40007 | 自动文件→显式有序文件→GUI CLI；布尔存在语义与有值覆盖 | `inspect_sources`，继承 true 不被 GUI false 撤销，后发左右成对值覆盖前值；未知内容不宣称完整解析 |
| help / version / show-controls | 查询页执行 2002 / 2012 / 2022，取消 2003 / 2013 / 2023 | 独立 Q，不影响比较 Session | 无有效视频执行；帮助核对 61 项基线声明，显示缺失与额外选项 |
| find-filters | 搜索 2341，执行 2342，取消 2343 | 独立 Q，显式空 argv 保留 | 空搜索、`scale`、无匹配词；真实引擎 PASS |
| find-protocols | 搜索 2451，执行 2452，取消 2453 | 独立 Q | 空搜索、`http`、无匹配词；真实引擎 PASS |
| find-demuxers | 搜索 2491，执行 2492，取消 2493 | 独立 Q | 空搜索、`matroska`、无匹配词；真实引擎 PASS |
| find-decoders | 搜索 2531，执行 2532，取消 2533 | 独立 Q | 空搜索、`h264`、无匹配词；真实引擎 PASS |
| find-hwaccels | 搜索 2571，执行 2572，取消 2573 | 独立 Q；声明不等于设备可用 | 空搜索、`cuda`、无匹配词；真实引擎 PASS |
| GUI-005/006 查询隔离 | 查询页执行/取消及关闭设置页 | 单独 `process::Run`，10 秒超时，不复用比较任务句柄 | 比较运行期间版本查询成功；取消/超时后比较仍存活，随后正常停止 |
| 长命令行及中文配置路径 | 开始比较 1043；实际命令见 session.txt | `prepare_launch` 仅改变参数传递方式，保留工作目录 | 中文文件字节相同暂存；超过 Windows 限长时无损参数文件；空参数和危险位置参数拒绝转存 |

全部原始 61 CLI / 11 RV 对应表仍见 [COVERAGE.md](COVERAGE.md) 和 [P04-CONTROLS.md](P04-CONTROLS.md)，后者已补齐八类查询执行/取消控件 ID。

## 实现说明

- `.vcgui` 为独立版本化二进制会话格式，字符串 UTF-8、长度有界，最大文件 16 MiB。完整性校验用于识别损坏，不是安全签名。读入临时对象，校验通过后才替换；写入同目录临时文件并原子替换。未知格式/版本、损坏长度/校验、重复 ID、错误枚举、未知作用域及字段拒绝读入。
- 本地相对输入在保存时相对于工作目录固化；协议地址、占位符、显式空值及未启用条目保持独立状态。会话不复制媒体或引擎，不在退出时自动保存。
- `.opt` 使用独立的同语义分词器，不使用 shell 分词。未知/嵌套/查询选项等不能完整转换时不部分提交，可明确选择兼容文件引用。读取的 UTF-8 原文和来源可查看，原文件不重写。
- 导出的是当前启用比较参数，不是 GUI 编辑模型；停用输入和继承结构应使用 `.vcgui` 保存。外部引用不静默变成嵌套配置，独立空参数不静默丢失。
- 原引擎将文件内容插在启动 CLI 前再次解析。文件级 `--` 会将后续 `--options-file` 等启动参数误当视频，因此导出/超长转存不写该分隔符，并拒绝必须靠它保护的位置输入。该规则已用真实引擎验证。
- 中文显式配置路径暂存到 ASCII `work/config-cache/`，内容字节不变、cwd 不变。启动器所在路径无法提供 ASCII 暂存位置时给出限制，不改引擎、不写任意外部目录。
- 查询复用独立运行助手，但与比较拥有不同进程、日志目录和控制事件。输出与退出状态显示在设置页，参数在其后；帮助查询记录 `compatibility.txt`。输出仍采用 P05 限额，不能把截断的帮助声明当作能力证明。

## 验证结果

| 验证层 | 实际结果 | 证据 |
|---|---|---|
| 61 CLI 全别名＋11 RV 静态覆盖 | PASS；9 个主窗口＋52 个分类控件映射保持一致 | `verify-coverage.ps1`、`verify-settings-coverage.ps1` |
| 中文 VS 发现 | CP936 / CP65001 PASS | `verify-vs-discovery.ps1` |
| Debug / Release 构建及原生测试 | 每配置 11/11 CTest 通过；新增配置 138 项 | `work/p02/Debug-tests.xml`、`Release-tests.xml` 及 `*-cases.log` |
| GUI 会话、导入导出、查询 | Debug/Release/Installed 各 31 项通过 | `work/p06/config-query.json` |
| 原 P03 / P04 回归 | 三份 GUI 各 29 项输入、10 项设置通过 | `work/p03/input-check.json`、`work/p04/settings-check.json` |
| 原 P05 进程 GUI 回归 | 三份 GUI 各 14 项通过 | `work/p05/process-gui-check.json` |
| 窗口检查 | 三份 GUI 96 DPI / PerMonitorV2 / 正常退出通过 | `work/p06-window/` |
| 真实引擎查询与参数文件 | 30 项通过 | `work/p06/real-config-query.json`，每次测试子目录保存完整查询文本 |

原生新增测试：[storage_tests.cpp](tests/storage_tests.cpp)。真实 EXE 菜单/原生文件框/查询及并行比较检查：[verify-config-gui.ps1](tools/verify-config-gui.ps1)。输出显示顺序调整后，最终安装版再次通过该 GUI 验收，结果 JSON 保存实际 EXE SHA256。

## 真实引擎结果

引擎为用户提供的 `D:\Tools\Media\video-compare-20260828-win10-x86_64\video-compare.exe`，版本 `20260828-reykjavik`。媒体为用户提供的 `MiniMax_H3_00024-audio.mp4` 和 `MiniMax_H3_00001_.mp4`。

- 八类查询正常退出 0；五类搜索的空值、关键词、无匹配词均返回 0。空搜索以独立空 argv 传入，不转成缺少参数。
- 帮助文本中的 61 项与源码基线匹配，未发现缺失或额外选项。只确认帮助声明，不等同所有模式/设备已验收。
- 中文路径配置 `--frame-buffer-size 60` 经字节不变暂存后，真实比较窗口显示 `/60`，证明引擎读取该配置；两侧媒体加载，正常停止退出 0。
- 导出的 `.opt` 经 ASCII 文件名副本直接交给原引擎执行，两侧媒体加载、右侧 `hflip` 生效，正常停止退出 0，没有将启动参数当作额外视频。
- 截图：`work/p06/real-query.png`、`real-config-engine.png`、`real-config-log.png`、`real-export-engine.png`。垂直布局的原始尺寸超出屏幕时，引擎自身显示尺寸提示；此测试未开启适应屏幕。

## 复验与边界

完整本地入口（失败即停止）：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\gui-launcher\tools\verify-p06.ps1
```

真实引擎入口及使用方法见 [README](README.md)。测试只关闭其创建的进程，历史日志及参数缓存保留用于复核。

未知格式会话不自动迁移；未知 `.opt` 语义不猜测重写；含位置输入的兼容文件与 GUI 输入的拼接会明确标为不能给出完整有效配置。非 UTF-8 文件需自行转换为明确 UTF-8 或手动通过配置页引用，导入不会擅自改编码。完整原引擎模式、1+多右侧播放切换、条件设备与同配置 CLI 对照仍为 P07，P08 发布未完成。

## 持续改进建议

- P07 使用保存的会话固定输入顺序与参数，逐项核验多右侧切换、滤镜/同步和设备条件；将条件不满足的项目明确记录，不以帮助声明替代实测。
