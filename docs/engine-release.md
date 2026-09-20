# 本体 Windows x64 正式版

版本：`20260920-frame-step-cache`；发布日期：2026-09-20。

## 使用

完整解压 `video-compare-20260920-frame-step-cache-windows-x64.zip`，保留同目录的 EXE 和 DLL。无需安装 Visual Studio、FFmpeg 命令行程序或开发工具。目标环境为 Windows 10/11 x64；实际运行验收以本机 Windows 为准，未覆盖所有系统版本与显卡。

推荐在独立 GUI 的“比较程序”中选择解压后的 `video-compare.exe`，加载一个左侧参考和一个或多个右侧输入。此包为本体，不包含独立 GUI。

或在解压目录执行：

```powershell
.\video-compare.exe --version
.\video-compare.exe -- "D:\Videos\left.mp4" "D:\Videos\right.mp4"
```

双击无参数本体不会弹出文件选择框。不要将新版 EXE 单独放到旧版 DLL 目录，以免混用依赖版本。

`A` / `Shift+A`：暂停并后退一参考帧；`D` / `Shift+D`：暂停并前进一参考帧；空格：从显示位置恢复播放。默认缓存 50；缓存外退帧会批量补齐历史，但仍可能有重新解码停顿。可变帧率原始时间戳可配置 `--left-decoder-options trust_dec_pts=1`。完整边界见 `frame-stepping.md`（源码仓库中为 `docs/frame-stepping.md`）。

## 产物

- `video-compare-20260920-frame-step-cache-windows-x64/`：可直接使用的程序目录。
- 同名 `.zip`：便携发布包。
- `*-source.zip`：本次构建对应的工作区源码快照，包括尚未提交的修改，不包含构建缓存、测试素材或日志。
- `*.sha256`：发布 ZIP 校验值；程序包内 `SHA256SUMS.txt` 为文件级校验值。
- 包内 `BUILD-INFO.txt`、`licenses/`：编译及依赖版本、许可证和 FFmpeg 构建来源。

这些产物生成于本地 `dist/`，不纳入 Git；本地生成包不等于已上传 GitHub Release。

## 重建

`tools/build-engine-release.ps1` 使用 LLVM-MinGW UCRT x64，以 C++14、`-O2` 编译独立 Release 对象目录，同时构建测试程序；打包时去除 EXE 调试符号。需要预先准备以下依赖目录（默认放在 `.local-build/`，也可通过 `-DependencyRoot` 指定）：

```text
llvm-mingw-20260908-ucrt-x86_64/
ffmpeg-9.0.1-full_build-shared/
SDL2-2.32.10/x86_64-w64-mingw32/
SDL2_ttf-2.24.0/x86_64-w64-mingw32/
```

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\build-engine-release.ps1
# 执行测试后再打包；测试方式见 docs/frame-stepping.md
powershell -NoProfile -ExecutionPolicy Bypass -File .\tools\package-engine-release.ps1
```

打包脚本递归收集 PE 导入的 DLL，检测未解决依赖，在仅包含 Windows 系统目录的 PATH 下验证版本及帮助命令，生成源码快照、发布 ZIP 与校验值。同版本产物已存在时停止，不覆盖旧包。

## 验收记录

| 项目 | 当前状态 | 判断依据 |
| --- | --- | --- |
| Release 优化构建 | 已验收 | 独立对象目录，重新编译本体及测试程序 |
| 逐帧正确性 | 已验收 | 对 Release 构建重新运行 CFR、VFR、多右侧、缓存 1/3/50 的 PNG 核对 |
| 普通播放回归 | 已验收 | Release 构建十组播放／寻址／循环／多右侧回归，以及四组单元测试 |
| 发布目录依赖完整性 | 已验收 | 10 个 DLL 递归导入检查、仅系统 PATH 下启动、ZIP 解压逐文件 SHA-256 复验；解压后的本体用 SDL dummy/software 成功加载左右视频 |
| 本机以外系统／显卡组合 | 未完成 | 不将本机结果扩大为全部设备兼容性保证 |


