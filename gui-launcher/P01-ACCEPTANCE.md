# P01 独立工程验收记录

日期：2026-09-19。当前状态：**已验收**。验收范围仅为独立构建、原生窗口、资源和生命周期；不包含视频选择、参数生成或引擎比较。

## 实现交付

| 内容 | 文件/产物 |
|---|---|
| 独立工程 | `CMakeLists.txt`、`CMakePresets.json` |
| Unicode Win32 入口与窗口 | `src/main.cpp` |
| 中文菜单、版本资源 | `resources/app.rc`、`resources/resource.h` |
| 普通用户权限、Common Controls v6、PerMonitorV2 清单 | `resources/app.manifest` |
| 自动探测 VS 并构建 Debug/Release | `tools/build.ps1` |
| 窗口运行检查 | `tools/verify-window.ps1` |
| 局部忽略与构建配置保留 | `.gitignore` |
| 可直接打开的基础版 EXE | `dist/video-compare-gui.exe` |

当前使用系统默认应用图标；没有自制图标。`dist/` 只是 P01 可运行副本，完整打包发布仍属于 P08。

## 实际环境

- Visual Studio Build Tools：18.6.11806.211。
- CMake：4.2.3-msvc3；自动选择生成器 `Visual Studio 18 2026`。
- MSBuild：18.6.3+84d3e95b4。
- C++ 编译器：MSVC 19.51.36243.0，x64。
- Windows SDK：10.0.26100.0。
- 实际窗口 DPI：96（100%）；清单上下文确认为 PerMonitorV2。
- C++17、静态 MSVC 运行库；引擎开发依赖未参与构建。

## 验证结果

| 检查项 | 实际结果 | 状态 |
|---|---|---|
| x64 Debug 构建 | CMake/MSBuild 成功生成 `build/msvc-x64/bin/Debug/video-compare-gui.exe` | PASS |
| x64 Release 构建 | 成功生成 `build/msvc-x64/bin/Release/video-compare-gui.exe` | PASS |
| 子目录安装 | `dist/video-compare-gui.exe` 与 Release SHA-256 相同 | PASS |
| EXE 格式 | 三份 EXE 均为 x64、Windows GUI 子系统 | PASS |
| 直接启动 | 检查脚本通过系统进程启动分别打开三份 EXE；无引擎或媒体参数 | PASS |
| 窗口与控件 | 正确的 Unicode 标题、三个文本控件及一个状态栏 | PASS |
| 调整窗口大小 | 初始尺寸与 640×420 截图已检查；中文文本及状态栏可见 | PASS |
| 关于对话框 | 菜单命令打开并可正常关闭 | PASS |
| 正常退出 | Debug 用退出菜单命令；Release/安装副本用关闭窗口消息；三者退出码均为 0 | PASS |
| DPI 清单 | 运行 API 确认 PerMonitorV2；提取 EXE 清单含 asInvoker、Common Controls v6 | PASS |
| 动态依赖 | dumpbin 显示仅 COMCTL32、USER32、GDI32、KERNEL32；无 FFmpeg/SDL/MFC 或动态 MSVC CRT DLL | PASS |
| 源码覆盖基准 | 61 CLI 选项全部别名、11 Ri 覆盖字段均通过静态核验 | PASS（仅文档覆盖） |
| 原项目隔离 | `git diff --exit-code` 为 0，新增源码及文档全部位于 `gui-launcher/` | PASS |

当前检查采用系统进程启动与窗口消息验证，没有模拟资源管理器鼠标双击；EXE 无参数即可打开，可供用户直接双击。150%/200% 缩放、跨显示器 DPI 切换、完整键盘可访问性和视频联调未执行，保留在后续专项验收中。

## 证据与复现

- [结构化结果](work/p01/window-check.json)：三种构建的路径、标题、DPI、退出码和 SHA-256。
- [默认窗口截图](work/p01/Installed-window.png)。
- [缩小窗口截图](work/p01/Installed-resized.png)。
- [从最终 EXE 提取的清单](work/p01/embedded.manifest)。

以上生成证据在 `work/` 中，按局部忽略规则不加入源码版本控制；重新运行窗口检查可生成 JSON 和截图。此文档保存验收摘要。

最终安装副本 SHA-256：

```text
457301E445D2D56DA33D9602C87B59DF3BC8730BD6678C863B58183A7B670945
```

复现命令（项目根目录）：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\gui-launcher\tools\build.ps1 -Configuration All -Install
powershell -NoProfile -ExecutionPolicy Bypass -File .\gui-launcher\tools\verify-window.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\gui-launcher\tools\verify-coverage.ps1
```

## 持续改进建议

- 下一步执行 P02，将覆盖表 ID 写入结构化选项定义与参数测试，在接入视频选择界面前先验证参数生成。
- 后续窗口加入可交互控件后扩展 DPI、Tab 导航和屏幕阅读器检查，当前文本窗口截图不替代这些验收。
