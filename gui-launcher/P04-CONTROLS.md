# P04 源码选项 → GUI 控件 → 参数作用范围 → 验收用例

本表逐项定位全部 61 CLI 与 11 RV。原源码选项和全部别名仍以 [COVERAGE.md](COVERAGE.md) 为准。本表是控件和自动验收索引；实际结果与限制见 [P04-ACCEPTANCE.md](P04-ACCEPTANCE.md)。控件数不代表引擎运行通过。

分类定义：[settings_catalog.inc](src/ui/settings_catalog.inc)；绑定与提交：[settings.cpp](src/ui/settings.cpp)；已有常用项：[main.cpp](src/main.cpp)。公共、左侧、右侧默认分别存储；未指定和显式空值不混淆。CLI 控件基址为 `2000 + (编号−1)×10`，RV 基址为 `4000 + (编号−1)×10`；配置文件列表为专用控件。

## CLI 逐项对应

除开关外，普通值用“指定此值”控制是否发出参数。下拉框允许编辑兼容值，数值和枚举由参数层校验。`ui.settings-catalog-a/b` 从独立样例验证实际控件提交后的参数；主窗口交互由 P03 回归覆盖。P06 已接入独立查询执行：基址 +2 为执行、+3 为取消，结果在预览区；支持空搜索与 10 秒超时，详见 P06 验收。

| ID | 源码键 / 发出选项 | GUI 分类与控件 | 参数作用范围 | 验收用例 |
|---|---|---|---|---|
| CLI-001 | `help` / `--help` | 能力查询 · 引擎帮助：预览按钮 `2000`，执行 `2002`，取消 `2003` | Q 独立查询 | `ui.settings-catalog-a / CLI-001`；不进入比较参数 |
| CLI-002 | `version` / `--version` | 能力查询 · 引擎版本：预览按钮 `2010`，执行 `2012`，取消 `2013` | Q 独立查询 | `ui.settings-catalog-a / CLI-002`；不进入比较参数 |
| CLI-003 | `show-controls` / `--show-controls` | 能力查询 · 比较窗口操作说明：预览按钮 `2020`，执行 `2022`，取消 `2023` | Q 独立查询 | `ui.settings-catalog-a / CLI-003`；不进入比较参数 |
| CLI-004 | `verbose` / `--verbose` | 配置与诊断 · 详细日志：开关 `2030` | G 全局 | `ui.settings-catalog-a / CLI-004` |
| CLI-005 | `options-file` / `--options-file` | 配置与诊断 · 显式参数文件：列表 `3100`、路径 `3101`、添加/浏览/移除/上移/下移 `3102–3106` | G 配置来源 | `ui.settings-catalog-a / CLI-005`；有序多文件、单页复位 |
| CLI-006 | `fullscreen` / `--fullscreen` | 主窗口 · 全屏启动：`IDC_FULLSCREEN`（1038） | G 全局 | P03 实际 EXE 常用控件回归 |
| CLI-007 | `high-dpi` / `--high-dpi` | 主窗口 · 高 DPI：`IDC_HIGH_DPI`（1036） | G 全局 | P03 实际 EXE 常用控件回归 |
| CLI-008 | `font` / `--font` | 字体 · 比较窗口字体：指定 `2070`、值 `2071`、字体文件选择 `3107` | G 全局 | `ui.settings-catalog-a / CLI-008` |
| CLI-009 | `ui-scale` / `--ui-scale` | 字体 · 比较窗口文字缩放：指定 `2080`、值 `2081` | G 全局 | `ui.settings-catalog-a / CLI-009` |
| CLI-010 | `10-bpc` / `--10-bpc` | 主窗口 · 10 位色深：`IDC_TEN_BIT`（1037） | G 全局 | P03 实际 EXE 常用控件回归 |
| CLI-011 | `fast-alignment` / `--fast-alignment` | 画面与窗口 · 快速输入对齐：开关 `2100` | G 全局 | `ui.settings-catalog-a / CLI-011` |
| CLI-012 | `bilinear-texture` / `--bilinear-texture` | 画面与窗口 · 双线性纹理插值：开关 `2110` | G 全局 | `ui.settings-catalog-a / CLI-012` |
| CLI-013 | `subtraction-mode` / `--subtraction-mode` | 主窗口 · 差异视图：`IDC_DIFFERENCE`（1035） | G 全局 | P03 实际 EXE 常用控件回归 |
| CLI-014 | `display-number` / `--display-number` | 画面与窗口 · 显示器编号：指定 `2130`、值 `2131` | G 全局 | `ui.settings-catalog-a / CLI-014` |
| CLI-015 | `display-mode` / `--mode` | 主窗口 · 显示布局：`IDC_LAYOUT`（1030） | G 全局 | P03 实际 EXE 常用控件回归 |
| CLI-016 | `window-size` / `--window-size` | 主窗口 · 窗口大小：`IDC_WIDTH`（1032），高 `IDC_HEIGHT`（1033） | G 全局 | P03 实际 EXE 常用控件回归 |
| CLI-017 | `window-fit-display` / `--window-fit-display` | 主窗口 · 适应可用屏幕：`IDC_WINDOW_MODE`（1031） | G 全局 | P03 实际 EXE 常用控件回归 |
| CLI-018 | `aspect-lock` / `--aspect-lock` | 画面与窗口 · 宽高比锁定：指定 `2170`、值 `2171` | G 全局 | `ui.settings-catalog-a / CLI-018` |
| CLI-019 | `aspect-view-mode` / `--aspect-view-mode` | 画面与窗口 · 画面宽高比：指定 `2180`、值 `2181` | G 全局 | `ui.settings-catalog-a / CLI-019` |
| CLI-020 | `auto-loop-mode` / `--auto-loop-mode` | 主窗口 · 缓冲区循环：`IDC_LOOP`（1034） | G 全局 | P03 实际 EXE 常用控件回归 |
| CLI-021 | `frame-buffer-size` / `--frame-buffer-size` | 播放与同步 · 帧缓冲数量：指定 `2200`、值 `2201` | G 全局 | `ui.settings-catalog-a / CLI-021` |
| CLI-022 | `time-shift` / `--time-shift` | 主窗口 · 右侧时间偏移：`IDC_TIMESHIFT`（1039） | G 全局 | P03 实际 EXE 常用控件回归 |
| CLI-023 | `wheel-sensitivity` / `--wheel-sensitivity` | 播放与同步 · 滚轮灵敏度：指定 `2220`、值 `2221` | G 全局 | `ui.settings-catalog-a / CLI-023` |
| CLI-024 | `color-space` / `--color-space` | 色彩 / HDR · 色彩矩阵：公共 `2230/2231`；左 `2232/2233`；右 `2234/2235`（指定/值） | B / L / R 独立保存 | `ui.settings-catalog-a / CLI-024`；左右隔离、空值和继承 |
| CLI-025 | `color-range` / `--color-range` | 色彩 / HDR · 色彩范围：公共 `2240/2241`；左 `2242/2243`；右 `2244/2245`（指定/值） | B / L / R 独立保存 | `ui.settings-catalog-a / CLI-025`；左右隔离、空值和继承 |
| CLI-026 | `color-primaries` / `--color-primaries` | 色彩 / HDR · 原色：公共 `2250/2251`；左 `2252/2253`；右 `2254/2255`（指定/值） | B / L / R 独立保存 | `ui.settings-catalog-a / CLI-026`；左右隔离、空值和继承 |
| CLI-027 | `color-trc` / `--color-trc` | 色彩 / HDR · 传递函数：公共 `2260/2261`；左 `2262/2263`；右 `2264/2265`（指定/值） | B / L / R 独立保存 | `ui.settings-catalog-a / CLI-027`；左右隔离、空值和继承 |
| CLI-028 | `tone-map-mode` / `--tone-map-mode` | 色彩 / HDR · 色调映射模式：公共 `2270/2271`；左 `2272/2273`；右 `2274/2275`（指定/值） | B / L / R 独立保存 | `ui.settings-catalog-a / CLI-028`；左右隔离、空值和继承 |
| CLI-029 | `left-peak-nits` / `--left-peak-nits` | 色彩 / HDR · 左侧峰值亮度：指定 `2280`、值 `2281` | L 左侧 | `ui.settings-catalog-a / CLI-029` |
| CLI-030 | `right-peak-nits` / `--right-peak-nits` | 色彩 / HDR · 右侧默认峰值亮度：指定 `2290`、值 `2291` | R 右侧默认 | `ui.settings-catalog-a / CLI-030` |
| CLI-031 | `boost-tone` / `--boost-tone` | 色彩 / HDR · 色调增益：公共 `2300/2301`；左 `2302/2303`；右 `2304/2305`（指定/值） | B / L / R 独立保存 | `ui.settings-catalog-a / CLI-031`；左右隔离、空值和继承 |
| CLI-032 | `filters` / `--filters` | 滤镜 · 公共用户滤镜：指定 `2310`、值 `2311` | B 公共输入 | `ui.settings-catalog-b / CLI-032` |
| CLI-033 | `left-filters` / `--left-filters` | 滤镜 · 左侧用户滤镜：指定 `2320`、值 `2321` | L 左侧 | `ui.settings-catalog-b / CLI-033` |
| CLI-034 | `right-filters` / `--right-filters` | 滤镜 · 右侧默认用户滤镜：指定 `2330`、值 `2331` | R 右侧默认 | `ui.settings-catalog-b / CLI-034` |
| CLI-035 | `find-filters` / `--find-filters` | 能力查询 · 查询滤镜：预览按钮 `2340`，执行 `2342`，取消 `2343`，搜索文字 `2341`（可为空） | Q 独立查询 | `ui.settings-catalog-b / CLI-035`；不进入比较参数 |
| CLI-036 | `conversion-size` / `--conversion-size` | 画面与窗口 · 转换画布大小：指定 `2350`、值 `2351` | G 全局 | `ui.settings-catalog-b / CLI-036` |
| CLI-037 | `conversion-fit` / `--conversion-fit` | 画面与窗口 · 转换填充方式：指定 `2360`、值 `2361` | G 全局 | `ui.settings-catalog-b / CLI-037` |
| CLI-038 | `histogram-window` / `--histogram-window` | 分析窗口 · 直方图窗口：开关 `2370` | G 全局 | `ui.settings-catalog-b / CLI-038` |
| CLI-039 | `vectorscope-window` / `--vectorscope-window` | 分析窗口 · 矢量示波器窗口：开关 `2380` | G 全局 | `ui.settings-catalog-b / CLI-039` |
| CLI-040 | `waveform-window` / `--waveform-window` | 分析窗口 · 波形窗口：开关 `2390` | G 全局 | `ui.settings-catalog-b / CLI-040` |
| CLI-041 | `histogram-options` / `--histogram-options` | 分析窗口 · 直方图选项：指定 `2400`、值 `2401` | G 全局 | `ui.settings-catalog-b / CLI-041` |
| CLI-042 | `vectorscope-options` / `--vectorscope-options` | 分析窗口 · 矢量示波器选项：指定 `2410`、值 `2411` | G 全局 | `ui.settings-catalog-b / CLI-042` |
| CLI-043 | `waveform-options` / `--waveform-options` | 分析窗口 · 波形选项：指定 `2420`、值 `2421` | G 全局 | `ui.settings-catalog-b / CLI-043` |
| CLI-044 | `scope-size` / `--scope-size` | 分析窗口 · 分析窗口尺寸：指定 `2430`、值 `2431` | G 全局 | `ui.settings-catalog-b / CLI-044` |
| CLI-045 | `scope-notop` / `--scope-notop` | 分析窗口 · 分析窗口不置顶：开关 `2440` | G 全局 | `ui.settings-catalog-b / CLI-045` |
| CLI-046 | `find-protocols` / `--find-protocols` | 能力查询 · 查询协议：预览按钮 `2450`，执行 `2452`，取消 `2453`，搜索文字 `2451`（可为空） | Q 独立查询 | `ui.settings-catalog-b / CLI-046`；不进入比较参数 |
| CLI-047 | `demuxer` / `--demuxer` | 解码与硬件 · 公共解封装器：指定 `2460`、值 `2461` | B 公共输入 | `ui.settings-catalog-b / CLI-047` |
| CLI-048 | `left-demuxer` / `--left-demuxer` | 解码与硬件 · 左侧解封装器：指定 `2470`、值 `2471` | L 左侧 | `ui.settings-catalog-b / CLI-048` |
| CLI-049 | `right-demuxer` / `--right-demuxer` | 解码与硬件 · 右侧默认解封装器：指定 `2480`、值 `2481` | R 右侧默认 | `ui.settings-catalog-b / CLI-049` |
| CLI-050 | `find-demuxers` / `--find-demuxers` | 能力查询 · 查询解封装器：预览按钮 `2490`，执行 `2492`，取消 `2493`，搜索文字 `2491`（可为空） | Q 独立查询 | `ui.settings-catalog-b / CLI-050`；不进入比较参数 |
| CLI-051 | `decoder` / `--decoder` | 解码与硬件 · 公共解码器：指定 `2500`、值 `2501` | B 公共输入 | `ui.settings-catalog-b / CLI-051` |
| CLI-052 | `left-decoder` / `--left-decoder` | 解码与硬件 · 左侧解码器：指定 `2510`、值 `2511` | L 左侧 | `ui.settings-catalog-b / CLI-052` |
| CLI-053 | `right-decoder` / `--right-decoder` | 解码与硬件 · 右侧默认解码器：指定 `2520`、值 `2521` | R 右侧默认 | `ui.settings-catalog-b / CLI-053` |
| CLI-054 | `find-decoders` / `--find-decoders` | 能力查询 · 查询解码器：预览按钮 `2530`，执行 `2532`，取消 `2533`，搜索文字 `2531`（可为空） | Q 独立查询 | `ui.settings-catalog-b / CLI-054`；不进入比较参数 |
| CLI-055 | `hwaccel` / `--hwaccel` | 解码与硬件 · 公共硬件加速：指定 `2540`、值 `2541` | B 公共输入 | `ui.settings-catalog-b / CLI-055` |
| CLI-056 | `left-hwaccel` / `--left-hwaccel` | 解码与硬件 · 左侧硬件加速：指定 `2550`、值 `2551` | L 左侧 | `ui.settings-catalog-b / CLI-056` |
| CLI-057 | `right-hwaccel` / `--right-hwaccel` | 解码与硬件 · 右侧默认硬件加速：指定 `2560`、值 `2561` | R 右侧默认 | `ui.settings-catalog-b / CLI-057` |
| CLI-058 | `find-hwaccels` / `--find-hwaccels` | 能力查询 · 查询硬件加速：预览按钮 `2570`，执行 `2572`，取消 `2573`，搜索文字 `2571`（可为空） | Q 独立查询 | `ui.settings-catalog-b / CLI-058`；不进入比较参数 |
| CLI-059 | `libvmaf-options` / `--libvmaf-options` | 分析窗口 · libvmaf 选项：指定 `2580`、值 `2581` | G 全局 | `ui.settings-catalog-b / CLI-059` |
| CLI-060 | `disable-auto-options-file` / `--no-auto-options-file` | 配置与诊断 · 加载工作目录自动配置：开关 `2590`（允许自动加载，反向映射） | G 配置来源 | `ui.settings-catalog-b / CLI-060`；勾选省略禁用标志 |
| CLI-061 | `disable-auto-filters` / `--no-auto-filters` | 滤镜 · 禁用引擎自动滤镜：开关 `2600` | G 全局 | `ui.settings-catalog-b / CLI-061` |

## 逐右侧字段（仅支持下列 11 项）

入口：主窗口选中右侧条目 → 编辑 → 全部逐视频设置。模式和值分别占两个控件，继承不发出覆盖参数，清空发出显式空值；只有滤镜允许追加。编辑使用草稿，取消不修改条目，复制和排序保留完整覆盖映射。

| ID | 源码键 | GUI 控件 | 参数作用范围 | 验收用例 |
|---|---|---|---|---|
| RV-001 | `filters` | 模式 `4000`、值 `4001`；继承 / 替换 / 追加 / 清空 | Ri 仅当前条目 | `ui.settings-overrides / RV-001`：替换、清空、继承、禁止不支持的追加 |
| RV-002 | `color-space` | 模式 `4010`、值 `4011`；继承 / 替换 / 清空 | Ri 仅当前条目 | `ui.settings-overrides / RV-002`：替换、清空、继承、禁止不支持的追加 |
| RV-003 | `color-range` | 模式 `4020`、值 `4021`；继承 / 替换 / 清空 | Ri 仅当前条目 | `ui.settings-overrides / RV-003`：替换、清空、继承、禁止不支持的追加 |
| RV-004 | `color-primaries` | 模式 `4030`、值 `4031`；继承 / 替换 / 清空 | Ri 仅当前条目 | `ui.settings-overrides / RV-004`：替换、清空、继承、禁止不支持的追加 |
| RV-005 | `color-trc` | 模式 `4040`、值 `4041`；继承 / 替换 / 清空 | Ri 仅当前条目 | `ui.settings-overrides / RV-005`：替换、清空、继承、禁止不支持的追加 |
| RV-006 | `decoder` | 模式 `4050`、值 `4051`；继承 / 替换 / 清空 | Ri 仅当前条目 | `ui.settings-overrides / RV-006`：替换、清空、继承、禁止不支持的追加 |
| RV-007 | `demuxer` | 模式 `4060`、值 `4061`；继承 / 替换 / 清空 | Ri 仅当前条目 | `ui.settings-overrides / RV-007`：替换、清空、继承、禁止不支持的追加 |
| RV-008 | `hwaccel` | 模式 `4070`、值 `4071`；继承 / 替换 / 清空 | Ri 仅当前条目 | `ui.settings-overrides / RV-008`：替换、清空、继承、禁止不支持的追加 |
| RV-009 | `tone-map-mode` | 模式 `4080`、值 `4081`；继承 / 替换 / 清空 | Ri 仅当前条目 | `ui.settings-overrides / RV-009`：替换、清空、继承、禁止不支持的追加 |
| RV-010 | `peak-nits` | 模式 `4090`、值 `4091`；继承 / 替换 / 清空 | Ri 仅当前条目 | `ui.settings-overrides / RV-010`：替换、清空、继承、禁止不支持的追加 |
| RV-011 | `boost-tone` | 模式 `4100`、值 `4101`；继承 / 替换 / 清空 | Ri 仅当前条目 | `ui.settings-overrides / RV-011`：替换、清空、继承、禁止不支持的追加 |

## 跨字段验收

- `ui.settings-semantics`：左右显式空值与继承、取消、重新打开不注入默认值、无效数值、配置顺序与单页复位、无效逐视频值、公共滤镜追加。
- `verify-settings.ps1`：三份真实 EXE 的分类入口、52 个分类控件、嵌套条目编辑、11 项模式限制、回传主会话、复制/排序保留设置，及各分类截图。
- `verify-inputs.ps1` / `ui.native-events`：P03 输入、多选、拖入、排序、常用控件回归。
- P05/P06 已实现真实启动、配置内容解析及独立查询；全部模式、媒体和设备对照仍由 P07 逐项执行。

## 持续改进建议

- 新增或变更字段时，同时更新源码基线、控件映射、独立样例和实际 EXE 用例，再运行两份覆盖核验脚本。
