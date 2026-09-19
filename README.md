# video-compare：新增独立 GUI 启动器

本项目在视频比较工具的基础上，新增了面向 **Windows x64** 的独立图形启动器，代码位于 [`gui-launcher/`](gui-launcher/)。可以在界面中选择引擎、加载视频、配置比较参数并启动比较，无需手工拼写命令行。

## GUI 功能

- **选择比较引擎**：通过文件选择框指定已有的 `video-compare.exe`，使用其原目录中的配套 DLL；可检查引擎版本。
- **一个参考视频＋多个待比较视频**：左侧选择参考输入，右侧支持批量添加、拖入文件、勾选启用、复制、编辑和调整顺序。开始比较后，在本体窗口中切换右侧视频；多个右侧视频不会同时平铺。
- **常用与完整参数设置**：提供分割、水平和垂直布局，窗口尺寸、循环方式、时间偏移、差异视图、高 DPI 等常用控件；完整设置覆盖当前源码的 61 个 CLI 选项及 11 个逐右侧覆盖字段，并提供参数校验和命令预览。
- **多种输入与逐视频配置**：可配置本地文件、图片序列、网络地址／协议和脚本输入，以及各右侧条目的滤镜等覆盖设置；实际支持能力取决于所选引擎。
- **会话与参数文件**：支持保存／打开 `.vcgui` 会话，导入／导出引擎 `.opt` 参数文件，查看配置来源及合并结果。
- **启动、停止与日志**：启动外部比较进程，查看标准输出、错误输出、进程状态和退出码；支持正常停止及停止超时后的确认强制结束。
- **引擎能力查询**：查询帮助、版本、播放器操作、滤镜、协议、解封装器、解码器和硬件加速能力。

GUI 负责配置和启动，视频解码、画面显示及播放快捷键仍由所选 `video-compare.exe` 提供。它不会改变本体的帧缓存或逐帧导航行为，也不包含引擎及其 DLL。参数控件覆盖不代表全部硬件、输入和交互组合均已通过实机验证。

## 构建与使用 GUI

已安装 Visual Studio C++ 桌面开发工具、Windows SDK 和 CMake 后，在项目根目录执行：

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File .\gui-launcher\tools\build.ps1 -Configuration Release -Test -Install
```

然后运行 `gui-launcher/dist/video-compare-gui.exe`：

1. 点击“选择程序…”，选择已有的 `video-compare.exe`，保留其配套 DLL。
2. 选择左侧参考视频，向右侧列表添加并启用一个或多个待比较视频。
3. 设置比较选项，点击“校验与预览”，确认后点击“开始比较”。

一般顺序播放请选择“连续播放（不循环）”；缓冲区循环默认只循环缓存中的 50 帧，不是循环整段视频。

更多说明见 [GUI 使用与构建文档](gui-launcher/README.md)、[源码选项与验收覆盖表](gui-launcher/COVERAGE.md) 和 [GUI 控件映射](gui-launcher/P04-CONTROLS.md)。发布模板 `gui-launcher/release/` 按本项目约定仅在本机保留，不随 Git 提交；从 Git 检出后可直接构建和测试 GUI，生成发布压缩包前需另行准备模板。

## 原项目来源

原项目为 **[pixop/video-compare](https://github.com/pixop/video-compare)**。

[![原项目 GitHub release](https://img.shields.io/github/release/pixop/video-compare)](https://github.com/pixop/video-compare/releases)

上面的徽章对应原项目发布版本，不代表本项目 GUI 的版本。以下保留本仓库原有 README 全文，其安装、命令行用法及播放器操作说明供继续查阅；GUI 的使用方式以上文及独立启动器文档为准。

---

# <img src="https://github.com/user-attachments/assets/da615466-683e-4cc5-8380-32a98d743ba0" alt="Logo" width="32"/>&nbsp; video-compare

[![GitHub release](https://img.shields.io/github/release/pixop/video-compare)](https://github.com/pixop/video-compare/releases)

Split-screen video comparison tool written in C++14, utilizing FFmpeg libraries and SDL2. It provides
interactive navigation and playback controls, along with various analysis tools and customizable display options.

`video-compare` can be used to visually compare the impact of codecs, resizing algorithms, and other modifications
on two video files played in sync. The tool is versatile, allowing videos of differing resolutions, frame rates,
scanning methods, color formats, dynamic ranges, input protocols, container formats, codecs, or durations.

Thanks to FFmpeg's flexibility, `video-compare` is also capable of comparing images or image sequences.

## Installation

### Arch Linux

Install [via AUR](https://aur.archlinux.org/packages/video-compare):

```sh
git clone https://aur.archlinux.org/video-compare.git
cd video-compare
makepkg -sic
```

### Homebrew

Install [via Homebrew](https://formulae.brew.sh/formula/video-compare):

```sh
brew install video-compare
```

For more advanced use cases, it is also recommended to install [ffmpeg-full](https://formulae.brew.sh/formula/ffmpeg-full)
instead of the default FFmpeg:

```sh
brew install ffmpeg-full
```

The standard formula is somewhat minimal, while `ffmpeg-full` adds broader codec and format support (e.g., JPEG XL).

### Pre-compiled Windows 10 binaries

Pre-built Windows 10 x86 64-bit releases are available from [this page](https://github.com/pixop/video-compare/releases).
Download and extract the .zip-archive on your system, then run `video-compare.exe` from a command prompt.

### Compile from source

[Build it yourself](#build).

## Screenshots

Visual compare mode:
![Visual compare mode](docs/images/screenshot_1.jpg?raw=true)

Subtraction mode (plus time-shift, 200% zoom, and magnification):
![Subtraction mode"](docs/images/screenshot_2.jpg?raw=true)

Vertically stacked mode:
![Stacked mode"](docs/images/screenshot_3.jpg?raw=true)


## Usage

Launch using the operating system's DPI setting. Video pixels are doubled on devices like a Retina 5K display;
therefore, it is the preferred option for displaying HD 1080p videos on such screens:

    video-compare video1.mp4 video2.mp4

Allow high DPI mode on systems which supports that. Video pixels are displayed "1-to-1". Useful
for e.g. displaying UHD 4K video on a Retina 5K display:

    video-compare -d video1.mp4 video2.mp4

Increase bit depth to 10 bits per color component (8 bits is the default). Fidelity is increased while
performance takes a hit. Significantly reduces visible banding on systems with a higher grade display
and driver support for 30-bit color:

    video-compare -b video1.mp4 video2.mp4

Use a specific window size instead of deriving the window size from the video dimensions. The video
frame will be scaled to fit. If either width or height is left out, the missing value will be calculated
from the other specified dimension so that aspect ratio is maintained. Useful for downscaling high resolution
video onto a low resolution display:

    video-compare -w 1280x720 video1.mp4 video2.mp4

Size the window to fit the usable display bounds while maintaining the video’s aspect ratio. This option adjusts
for elements like taskbars or OS menus. Ideal for maximizing the viewing area while keeping the video dimensions
proportional to the screen:

    video-compare -W video1.mp4 video2.mp4

Automatic in-buffer loop playback, triggered when the buffer fills or end-of-file is reached, streamlines
video analysis by eliminating the need for manual replay initiation (bidirectional "ping-pong" mode, `pp`, is
also available):

    video-compare -a on video1.mp4 video2.mp4

Shift the presentation time stamps of the right video instead of assuming the videos are aligned. A
positive amount has the effect of delaying the left video while negative values conversely delays the
right video. Useful when videos are slightly out of sync:

    video-compare -t 0.080 video1.mp4 video2.mp4

Display videos stacked vertically at full size without a slider (`hstack` for horizontal stacking is
also supported):

    video-compare -m vstack video1.mp4 video2.mp4

Perform simpler comparison of a video with itself using double underscore (`__`) as a placeholder. This
enables tasks such as comparing the video with a time-shifted version of itself or testing various sets
of filters, without the need to enter the same, potentially long path twice:

    video-compare some/very/long/and/complicated/video/path.mp4 __

Preprocess one or both inputs via a list of FFmpeg video filters specified on the command line
(see [FFmpeg's video filters documentation](https://ffmpeg.org/ffmpeg-filters.html#Video-Filters)).
The Swiss Army knife for cropping/padding (comparing videos with different aspect ratios),
adjusting colors, deinterlacing, denoising, speeding up/slowing down, etc.:

    video-compare -l crop=iw:ih-240 -r format=gray,pad=iw+320:ih:160:0 video1.mp4 video2.mp4

Select a demuxer that cannot be auto-detected (such as VapourSynth):

    video-compare --left-demuxer vapoursynth script.vpy video.mp4

Explicit decoder selection for the right video:

    video-compare --right-decoder h264_cuvid video1.mp4 video2.mp4

Compare an AV1 video against itself with and without film grain synthesis applied (requires `libdav1d`; `export_side_data=film_grain` disables grain synthesis during decoding):

    video-compare --right-decoder libdav1d:export_side_data=film_grain input_av1.mkv __

Set the same hardware acceleration type for both videos:

    video-compare --hwaccel cuda video1.mp4 video2.mp4

Set the hardware acceleration type for the left video only:

    video-compare --left-hwaccel videotoolbox video1.mp4 video2.mp4

By default, HDR videos are automatically color space converted to sRGB with an initial 500-nit peak light
level. This default can be overridden with a custom peak light level, such as 850 nits. The specified peak
light level is then dynamically adjusted during decoding based on any MaxCLL metadata:

    video-compare -R 850 sdr_video.mp4 hdr_video.mp4

Map a 500-nit peak light level HDR video for an sRGB SDR display, and adjust the tone of the SDR video
to simulate the relative light level difference between the two videos on an actual HDR display:

    video-compare -T rel -L 500 hdr_video.mp4 sdr_video.mp4

Apply common filters to both videos and extend them with additional side-specific filters using the
placeholder resolution functionality. This structure also works for demuxer, decoder, and hardware
acceleration settings:

    video-compare -i yadif,hqdn3d -l setfield=bff,__ -r __,scale=iw/2:ih/2 video1.mp4 video2.mp4

Compare a reference (left) video against multiple renditions (right) by specifying more than two input paths.
Useful for comparing a reference encode to multiple renditions (e.g. different bitrates or encoder settings),
or for comparing ground truth, input, and model output in one session. Command-line settings are shared for all
right videos, and the active right video can be switched within the UI:

    video-compare reference.mp4 rendition1.mp4 rendition2.mp4

Override any command-line option for individual right videos using the `::` separator. Global settings
apply to all right videos by default, but can be overridden per-video:

    video-compare -r yadif input.mp4 output1.mp4 \
        output2.mp4::filters=__,scale=1920:-1 \
        output3.mp4::filters=::hwaccel=videotoolbox

The above features can be combined in any order, of course. Launch `video-compare` without any arguments to
see all supported options.

## Controls

### Basic

- `H`: Toggle on-screen help text for controls
- `V`: Toggle video info overlay
- `Space`: Toggle play/pause
- `,`: Toggle bidirectional in-buffer loop/pause
- `.`: Toggle forward-only in-buffer loop/pause
- `Escape`: Quit
- `Left arrow`: Seek 1 second backward
- `Right arrow`: Seek 1 second forward
- `Up arrow`: Seek 15 seconds forward
- `Down arrow`: Seek 15 seconds backward
- `Page up`: Seek 600 seconds forward
- `Page down`: Seek 600 seconds backward
- `Z`: Magnify area around cursor (shown in lower-left corner)
- `C`: Magnify area around cursor (shown in lower-right corner)
- `J`: Reduce playback speed
- `L`: Increase playback speed
- `A`: Move to the previous frame in the buffer
- `D`: Move to the next frame in the buffer
- `E`: Re-center view around mouse position
- `R`: Global re-center and reset zoom to 100% (x1)
- `S`: Swap left and right video
- `Tab`: Cycle through right videos
- `1`: Toggle hide/show left video
- `2`: Toggle hide/show right video
- `3`: Toggle hide/show HUD
- `4`: Zoom to 1:1 pixels
- `5`: Zoom 50% (x0.5)
- `6`: Zoom 100% (x1)
- `7`: Zoom 200% (x2)
- `8`: Zoom 400% (x4)
- `9`: Zoom 800% (x8)
- `0`: Toggle video/subtraction mode
- `+`: Time-shift right video 1 frame forward
- `-`: Time-shift right video 1 frame backward

### Advanced

- `P`: Print mouse position and pixel value under cursor to console
- `M`: Print image similarity metrics to console
- `F`: Save both frames and the on-screen content as PNG images
- `I`: Toggle fast/high-quality resizing for input alignment
- `T`: Toggle nearest-neighbor/bilinear video texture filtering
- `Y`: Cycle through subtraction modes
- `U`: Toggle luminance-only subtraction mode
- `X`: Show the current video frame and UI update rates (FPS)
- `F1`: Toggle Histogram window
- `F2`: Toggle Vectorscope window
- `F3`: Toggle Waveform window
- `Alt+Enter`: Toggle fullscreen
- `Shift+L`: Crop left video interactively
- `Ctrl+L`: Clear crop on left video
- `Ctrl+Shift+L`: Copy left crop to all right videos
- `Shift+R`: Crop right video interactively
- `Ctrl+R`: Clear crop on right video
- `Ctrl+Shift+R`: Copy right crop to left
- `Shift+B`: Crop both videos to the same area
- `Backspace`: Undo last crop operation
- `Shift+D`: Decode and advance one frame
- `Shift+A`: Seek to the previous frame (best with intra-frame formats)
- `Shift+M`: Cycle display mode
- `Shift+S`: Cycle aspect view mode
- `Shift+F`: Select a region and save cutouts as PNGs
- `Shift+X`: Print display state to console
- `Shift+W`: Restore saved window size
- `Ctrl+W`: Restore startup window size
- `Ctrl+Shift+W`: Save current window size
- `Ctrl+Shift+1..0`: Switch directly to right video 1–10
- `Ctrl+C` / `Cmd+C`: Copy the current timestamp of the left video to the clipboard
- `Ctrl+V` / `Cmd+V`: Paste a timestamp from the clipboard and seek to that position
- `Ctrl` + `+/-`: Time-shift right video by 10 frames
- `Alt` + `+/-`: Time-shift right video by 100 frames

### Mouse Controls

Move the mouse horizontally to adjust the movable slider position.

Use the mouse wheel to zoom in/out on the pixel under the cursor. Pan the view by moving the mouse while holding down the right button.

Left-click the mouse to perform a time seek based on the horizontal position of the mouse cursor relative to the window width (the target position is shown in the lower right corner).

### Other

Hold `Ctrl` or `Shift` for smaller relative seek, playback-speed, and zoom adjustments where available.
Availability may depend on conflicts with application shortcuts or operating system bindings.

## Build

### Requirements

Requires FFmpeg headers and development libraries to be installed, along with SDL2 and
its TrueType font rendering add on (libsdl2_ttf). SDL2 version 2.0.10 or later is now
specifically required for subpixel accuracy rendering capabilities. Users may need to
upgrade their existing SDL2 installation before compiling.

On Debian GNU/Linux the required development packages can be installed via `apt`:

```sh
apt install build-essential libavformat-dev libavcodec-dev libavfilter-dev libavutil-dev libswscale-dev libswresample-dev libsdl2-dev libsdl2-ttf-dev
```

On Fedora Linux the required development packages can be installed via `dnf`:

```sh
dnf install make gcc-c++ ffmpeg-devel SDL2-devel SDL2_ttf-devel
```

### Instructions

Compile the source code via GNU Make:

```sh
make
```

The linked `video-compare` executable will be created in the repository root. To perform a system wide installation:

```sh
make install
```

Note that root privileges are required to perform this operation in most environments (hint: use e.g. `sudo`).

## Notes

1. Audio playback is not supported.
2. Keep time-shifts below a few seconds for the best experience.
3. Seeks require re-synchronization on the closest keyframe (i.e., I-frame).

## Practical tips

### Send To Integration in Windows File Explorer

You can launch `video-compare` directly from Windows File Explorer when you only need to specify input files. Simply use:

**Right click → Send to → video-compare**

#### How it works

https://user-images.githubusercontent.com/8549626/166630445-c8c511b7-005f-48aa-83bc-0eb9676cfa2a.mp4

For quick access, select two files, right-click either one, then press:

- **N** to focus _Send to_
- **V** to select _video-compare_

#### Setup

To make _video-compare_ appear in the **Send to** menu:

1. Open the Run dialog (**Windows + R**)
2. Type `shell:sendto` and press Enter
3. Create a shortcut to `video-compare.exe` in this folder

Thanks to [couleurm](https://github.com/couleurm) for sharing this tip and providing the screen recording.

### More frontend options for Windows users

For Windows users, the community has shared several frontend options to complement the command-line functionality:

1. **Beyond Compare** integration: Launch `video-compare` directly from the interface.
2. **Total Commander** integration: Add a toolbar button to open selected videos.
3. **[VideoCompareGUI](https://github.com/TetzkatLipHoka/VideoCompareGUI)**: A standalone graphical utility that simplifies launching `video-compare`.

For details, check out the [open GitHub issue thread](https://github.com/pixop/video-compare/issues/81).

## Contributing

We're always looking for ways to improve and expand the tool. Your feedback and contributions are appreciated.

## Credits

`video-compare` was created by Jon Frydensbjerg (email: jon@pixop.com). The code is mainly based on
the excellent video player GitHub project: https://github.com/pockethook/player

Many thanks to the [FFmpeg](https://github.com/FFmpeg/FFmpeg), [SDL2](https://github.com/libsdl-org/SDL) and
[stb](https://github.com/nothings/stb) authors.

## License

`video-compare` is licensed under the [GNU General Public License version 2](LICENSE.md).
Third-party software and the corresponding license texts are listed in
[licenses/THIRD-PARTY-NOTICES.txt](licenses/THIRD-PARTY-NOTICES.txt).

Binary redistributions of `video-compare`, including installers, application bundles, and GUI
frontends that package the executable, must include `LICENSE.md` and the applicable third-party
notices and license texts. The simplest approach is to preserve the complete `licenses/` directory
unchanged.

Source code for each official release is available from the corresponding GitHub release and
repository tag. Redistributors of binary builds must also provide access to the complete
corresponding source code as required by GPLv2.
