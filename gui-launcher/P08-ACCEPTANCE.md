# P08 独立发布验收

日期：2026-09-19。状态：**已验收（P08 发布范围）**。P07 仍是受限验收，其未验证交互、显示环境和历史裁剪对照问题已随发布包说明，不因完成打包改为通过。

## 交付

| 文件 | 内容 |
|---|---|
| `releases/video-compare-gui-0.1.0-win-x64.zip` | 0.1.0 Release x64 主程序、运行助手、便携标记、使用说明、依赖、已知限制、许可证、源码说明、校验清单与可选 engine 目录说明 |
| `releases/video-compare-gui-0.1.0-source.zip` | 对应独立 GUI 源码、资源、CMake、测试、工具和文档；不包含原引擎、用户媒体、个人会话、工作日志或编译缓存 |
| 两个 `.zip.sha256` | 对应 ZIP 的 SHA256；内部 `manifest.json` 包含运行包文件列表、大小、哈希及 EXE 直接导入项 |

包不附带引擎，用户可以在 GUI 中选择已有 `video-compare.exe`，原配 DLL 保留在其原目录；也可自行将完整引擎放在便携包的 `engine/` 下。

## 发布相关变更

- 发布包含 `portable.mode`。主程序发现该标记时，使用自身目录作为便携根目录，不因解压在原项目之内就继续向上查找 `gui-launcher`。日志、默认工作目录和缓存由此固定在本包 `work/` 内。
- 没有标记的开发构建保留原有开发目录行为。所有变更均在 `gui-launcher/`。
- `tools/package.ps1` 先构建/测试，再从 `build/msvc-x64/bin/Release` 的明确二进制列表打包；不从可能过期或被占用的 `dist` 取文件。不打包测试 EXE、PDB、测试媒体、会话或已有 work 目录。
- 本机旧 `dist/video-compare-gui.exe` 当时正在运行，安装覆盖步骤因占用失败。没有关闭用户窗口，也没有把失败安装当作成功；Debug/Release 构建及各 11 组 CTest 已通过，发布包采用新编译的 Release 文件。
- `verify-process-gui.ps1` 新增指定 EXE 和证据目录参数，支持直接检查新解压副本；只操作测试创建的进程和控件，不进行全局键鼠输入。

## 实际验证

| 检查 | 结果 | 证据 |
|---|---|---|
| Debug / Release 编译和 CTest | 两种配置各 11 组全部通过；后续旧 dist 安装受占用失败已单列 | `work/p08-build.log` |
| x64 与运行依赖 | dumpbin 确认两份 EXE 为 x64；GUI 导入 COMCTL32/GDI32/KERNEL32/ole32/SHELL32/USER32，runner 导入 KERNEL32/USER32；无独立 MSVCP/VCRUNTIME 或引擎 DLL 导入 | 包内 `manifest.json`，构建暂存目录的 imports 文本 |
| ZIP 校验与白名单 | 外层 SHA256、内部文件哈希/大小、文件清单一致；运行包共 10 个文件，无源码/缓存/媒体/个人配置 | `work/p08/package-verification.json` |
| 新目录启动 | 全新解压副本不含机器专属引擎路径；默认 work 和实际日志均位于新包目录，即使其祖先目录叫 gui-launcher | 实际 GUI 报告中的 portable 检查 |
| 实际选择与播放 | 原生引擎选择框选中用户已有引擎；无输入查询版本；加载用户提供的两份视频；比较窗口出现，GUI 保持响应 | `package-verification.json` 指向的 `real-engine-check.json` 与截图；10 项全部 PASS |
| 停止及退出 | GUI 正常停止引擎，退出码 0；关闭测试 GUI，助手完成 | 同上 |
| 配套源码独立构建 | 将源码 ZIP 解压到新目录，只用其 CMake/源码和本机 MSVC 完成 Release 构建，不依赖原引擎源码 | `work/p08/source-build.log`、`source-location.txt` |
| 原项目边界 | `git diff --name-only` 无原项目已跟踪文件变更；状态仅 gui-launcher 子目录 | 最终工作区检查 |

此验证在本机环境完成，不代表已覆盖所有 Windows 版本和显示硬件。发布包未签名；P07 全部限制在包内 `KNOWN-ISSUES.md`。

## 复现

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\gui-launcher\tools\package.ps1
powershell -NoProfile -ExecutionPolicy Bypass -File .\gui-launcher\tools\verify-package.ps1 -Archive '.\gui-launcher\releases\video-compare-gui-0.1.0-win-x64.zip' -Engine 'D:\engine\video-compare.exe' -Left 'D:\videos\left.mp4' -Right 'D:\videos\right.mp4'
```

已有同名发布文件时默认拒绝覆盖；有意重新生成这些产物可加 `-ReplaceExisting`。`-SkipBuild` 仅供已确认当前编译/测试结果的打包使用，不能用于跳过失败构建。新解压验收每次生成不同 GUID 目录，保留原证据。

## 持续改进建议

- 后续引擎或启动器升级时，用新的解压目录重复发布验证，保留旧版压缩包和会话；专项回归逐步关闭 P07 限制，避免将发布验证当成完整功能认证。
