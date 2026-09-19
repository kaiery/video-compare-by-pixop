# P03 输入与常用界面验收

日期：2026-09-19。状态：**已验收（限 P03 输入编辑、常用设置与参数预览）**。

所有变更位于 `gui-launcher/`。原项目源码、构建文件和根 README 未修改。当前界面使用独立的 Win32/C++17 工程和 P02 参数模型，不链接原播放器源码。

## 已交付的对应表

控件 ID 定义在 [resource.h](resources/resource.h)，绑定在 [main.cpp](src/main.cpp)，条目对话框在 [app.rc](resources/app.rc)。本表补充 [完整覆盖表](COVERAGE.md)，不把局部 GUI 完成等同于所有引擎模式完成。

| 源码选项 / 覆盖 ID | GUI 控件 | 参数作用范围 | 本次验收用例及结果 |
|---|---|---|---|
| 引擎与工作目录 / IN-010 | `IDC_ENGINE`、`IDC_WORKDIR`、系统选择按钮 | 会话 | 默认子目录 work，绝对程序路径进入预览；PASS |
| 左侧输入 / IN-001、003、004、005 | `IDC_LEFT`、`IDC_LEFT_KIND`、文件按钮 | L | Unicode 文件拖入、特殊字符路径、输入类型；PASS |
| 多个右侧 / IN-001、002、003、004 | `IDC_RIGHT_LIST`、添加文件/路径按钮 | 有序 Ri 集合 | 1+1、1+3 文件/地址/序列混合；系统多选；停用剔除/恢复；PASS |
| 复制、移除、排序 / IN-011 | `IDC_DUPLICATE`、`IDC_REMOVE`、`IDC_MOVE_UP/DOWN`、行拖动 | Ri 的稳定 ID、输入与配置 | 复制独立编辑、多选删除、上移下移及拖动保留滤镜；PASS |
| `display-mode` / CLI-015 | `IDC_LAYOUT` | G | split/hstack/vstack 的预览；PASS |
| `auto-loop-mode` / CLI-020 | `IDC_LOOP` | G | off/on/pp；控件说明明确为缓冲区循环；PASS |
| `window-size`、`window-fit-display` / CLI-016、017 | `IDC_WINDOW_MODE`、宽/高 | G | 默认/适应/自定义互斥、1280x/x720、非法尺寸拒绝、切换复位；PASS |
| `fullscreen`、`high-dpi`、`10-bpc`、`subtraction-mode` / CLI-006、007、010、013 | 四个独立复选框 | G | 勾选发出标志、取消后省略；PASS |
| `time-shift` / CLI-022 | `IDC_TIMESHIFT` | G→全部 R | 完整表达式 x25/24+0.1 和留空省略；PASS |
| `filters` / RV-001 | 条目对话框：`IDC_FILTER_MODE/TEXT` | Ri | 继承/替换/追加/清空；参考复用；复制后修改不影响原条目；PASS。P03 无公共滤镜设置时，追加由参数层解析为追加内容 |
| 校验与命令预览 / IN-012 | `IDC_VALIDATE`、`IDC_PREVIEW`、`IDC_COPY`、`IDC_FEEDBACK` | 会话 | 无有效右侧、错误窗口尺寸禁用复制；有效输入显示 argv；PASS。复制使用 P02 的 Windows 进程命令行编码 |
| 开始比较 / P05 | `IDC_START` | 会话 | 当前明确禁用；三个 EXE 均确认未开放 |

本机布局下，窗口最小拖动尺寸为 956×600（96 DPI），内容可纵向滚动；右侧列表及预览随可用宽度扩展。文件选择支持多选；输入类型包括本地文件、图片序列、网络协议、脚本、引用另一侧。对无法按原引擎语法表达的右侧 `::` 提供诊断。

## 执行与证据

从项目根目录运行：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\gui-launcher\tools\verify-p03.ps1
```

此入口依次核验 VS 发现编码、静态覆盖、构建及测试、真实 EXE 控件、窗口；任何步骤失败即停止。测试数据均写入子目录，原生事件测试创建的是路径占位文件，不是已解码的视频。

| 验证层 | 实际结果 | 证据 |
|---|---|---|
| VS 发现与静态映射 | CP936/CP65001 PASS；61 CLI + 11 RV PASS | `work/p03/verification.log`（本轮日志） |
| x64 Debug / Release 构建 | 均 PASS；Release 复制到 dist | [构建脚本](tools/build.ps1) |
| 参数核心 | 每配置 103 项检查，四组 CTest 均 PASS | `work/p02/Debug-cases.log`、`Release-cases.log` |
| 原生 GUI 事件 | 每配置 7 项，`ui.native-events` PASS | [测试](tests/ui_events.cpp)；同上 CTest 日志与 XML |
| 实际 Debug / Release / Installed GUI | 每份 29 项控件检查 PASS，共 87 项 | [测试脚本](tools/verify-inputs.ps1)；`work/p03/input-check.json`（含 EXE SHA256） |
| 窗口与资源 | 三份均 x64 GUI、PerMonitorV2、96 DPI；缩小/滚动/关于/正常退出 PASS | `work/p03-window/window-check.json` 及 PNG |
| 1+3 界面 | 已检查截图中的输入、条目滤镜、参数提示与按钮状态 | `work/p03/one-plus-three.png` |
| 原项目边界 | `git diff --exit-code`、`git diff --cached --exit-code` 均无原有文件改动 | `git status --short` 仅新增 `gui-launcher/` |

原生事件测试在独立测试宿主中编译生产窗口过程，使用真实 HDROP、ListView 通知和命中坐标验证拖入/行排序；系统文件框实际打开、取消及选择两个文件。它验证原生事件路径，不冒充已人工完成资源管理器鼠标拖放。另有三份真实 EXE 的控件交互检查，避免仅测试宿主。

## 边界与后续状态

- P04 **未完成**：其余 CLI 控件、公共/左右设置、其余 10 个逐右侧字段。
- P05 **未完成**：引擎探测、实际进程启动、stdout/stderr、运行日志；当前只提供参数校验反馈。
- P06 **未完成**：会话保存恢复、配置导入导出、查询。
- P07/P08 **未完成**：真实媒体和 CLI 对照、物理鼠标/完整键盘走查、150%/200% DPI、多显示器/10 位设备、最终发布。
- 本轮没有验证真实视频加载、解码、循环播放或比较窗口效果；参数通过不代表引擎/DLL/媒体可用。GUI 内明确说明此边界。

## 持续改进建议

- P04 为新增控件逐项补充覆盖 ID、作用范围和真实 EXE 测试，确保公共、左右与条目设置互不混淆。
- P05 先用引擎替身验证 Unicode argv 与日志排空，再接入真实媒体，避免把参数预览当作执行结果。
