# 环境核验记录

核验日期：2026-09-19。初次计划阶段仅执行读取与文件存在性检查；本轮 P01 已完成构建和窗口运行，未安装额外工具。

| 项目 | 本机证据与结论 |
|---|---|
| 工作目录 | `D:\ProjectAI\video-compare-by-pixop` |
| 原项目源码基线 | Git HEAD `dcdbefcdf7900659cb1a1f2631edbccfdcb1398c` |
| 原工作区 | 创建计划前 `git status --short` 无输出 |
| VS 安装 | `vswhere` 检测到 Visual Studio Build Tools 18.6.11806.211，安装完整 |
| 安装位置 | `C:\Program Files (x86)\Microsoft Visual Studio\18\BuildTools` |
| C++ 组件 | `vswhere -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64` 返回该安装 |
| MSVC 工具目录 | `VC\Tools\MSVC\14.51.36231` |
| CMake | VS 内置 `Common7\IDE\CommonExtensions\Microsoft\CMake\CMake\bin\cmake.exe` 存在 |
| Windows SDK Include | `10.0.22621.0`、`10.0.26100.0` 目录存在 |
| 比较程序 | 本次仓库文件枚举没有发现 `.exe`；未检查用户其他目录，不能推断整台机器未安装 |
| MFC/ATL | 本方案不依赖，未核验其安装状态 |

截图作为用户提供的环境线索；实际构建使用上述本机检测结果，不按截图写死工具链或 SDK 版本。

## 原定构建核验步骤及进展

1. 使用 `vswhere` 定位包含 x64/x86 C++ 工具链的 VS，调用其开发环境或独立构建脚本。
2. 在 `gui-launcher/build/` 生成独立构建；生成器按检测结果选择，不写死旧版 Visual Studio 名称。
3. 编译最小 Unicode Win32 窗口，确认编译器、SDK 库、资源编译器和链接器可用。
4. 再编译启动器；构建产物仅进入 `gui-launcher/`。
5. 选择已有 `video-compare.exe`，核对版本和 `--help`，验证配套 DLL、至少两份测试媒体及运行日志。

P01 已完成步骤 1–4：使用自动探测的 Visual Studio 18 2026 生成器、MSVC 19.51.36243.0、SDK 10.0.26100.0，完成 x64 Debug/Release 编译与链接，并检查安装副本的窗口运行。步骤 5 的引擎与视频联调尚未执行；硬件加速、10 位输出没有实测。详见 [P01 验收记录](P01-ACCEPTANCE.md)。

## 持续改进建议

- 首次视频联调时记录引擎版本、配套 DLL 和测试媒体信息，保持窗口工程验收与引擎能力验收分开。
