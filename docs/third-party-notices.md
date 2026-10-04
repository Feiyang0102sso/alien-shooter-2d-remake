# 第三方组件与素材来源

本项目使用用户提供的 NET Lizard Java ME 版 Alien Shooter 素材重建桌面运行时。原关卡、图片、图标、字形和 MIDI 的权利归原权利人；项目没有把这些素材重新授权为开源素材。Windows 图标直接封装原 `icons/ico.png` 的 24×24 像素，没有重绘。

| 组件 | 固定版本 | 用途 | 上游 |
| --- | --- | --- | --- |
| raylib | 5.5 | 窗口、OpenGL 绘制、输入、音频设备 | [源码](https://github.com/raysan5/raylib/tree/5.5) |
| nlohmann/json | 3.12.0 | JSON 资源和存档 | [源码](https://github.com/nlohmann/json/tree/v3.12.0) |
| FluidSynth | 2.6.1 | 原 MIDI 实时合成，未修改 DLL | [源码](https://github.com/FluidSynth/fluidsynth/tree/v2.6.1) |
| SDL | 3.2.10 | FluidSynth 官方 Windows 包附带 DLL | [源码](https://github.com/libsdl-org/SDL/tree/release-3.2.10) |
| libsndfile | 1.2.2 | FluidSynth 官方 Windows 包附带 DLL | [源码](https://github.com/libsndfile/libsndfile/tree/1.2.2) |
| GeneralUser GS | 2.0.3 游戏专用子集 | 按原 MIDI 裁剪的音色库，保留上游许可及所用采样 | [固定源码版本](https://github.com/mrbumpy409/GeneralUser-GS/tree/684543d5e5efaef08d02be50dcda8d552478fa60) |
| Microsoft Visual C++ Runtime | 与构建工具链兼容 | Windows C/C++ 运行库 | 使用用户系统已安装的 x64 运行库，不复制到构建输出 |

raylib 使用 zlib 风格许可，nlohmann/json 使用 MIT，FluidSynth 和 libsndfile 附带 LGPL 2.1 文本，SDL 使用 zlib 许可，GeneralUser GS 使用其单独许可。相应原文保留在源码目录 `src/3rdparty/` 中；构建不复制 `licenses/` 和本文。音色许可仍复制到 `assets/audio/synth/LICENSE.txt`。

raylib 内嵌的 GLFW、miniaudio、stb 等组件各自保留上游声明。GLFW 许可位于 `src/3rdparty/raylib/src/external/glfw/LICENSE.md`；其余单文件依赖及版权和许可段落保留在 `src/3rdparty/raylib/src/external/` 中，不复制到构建输出，也不参与游戏运行时加载。

固定依赖位于 `src/3rdparty/`，来源与许可集中记录于本文。取得时的上游 SHA256：

| 内容 | SHA256 |
| --- | --- |
| raylib 5.5 源码 ZIP | `00e8a7c85f7a5221e56ee8e4e1c6642b773eae73d29379a91c8ac5149e803f42` |
| nlohmann/json 3.12.0 单头文件 | `aaf127c04cb31c406e5b04a63f1ae89369fccde6d8fa7cdda1ed4f32dfc5de63` |
| FluidSynth 2.6.1 Windows ZIP | `fab7a2e4b85675b66970f97a39bbc239729c5e0f237198b5922a6a73cbc8677c` |
| GeneralUser-GS.sf2 完整母版 | `9575028c7a1f589f5770fccc8cff2734566af40cd26ed836944e9a5152688cfe` |

运行时使用 5,167,308 字节的裁剪版。裁剪按输入 MIDI 的预设、音高与力度移除未使用内容，保留原采样及参数；上游 `LICENSE.txt` 和 SoundFont 内的版权信息保持不变。整包 BAT 入口自动调用裁剪模块，输出为 `tools/output/audio/synth/`，subset.json 记录大小和保留的音色数量；当前工具不计算哈希。上表仅保留此前记录的上游来源信息。

微软 CRT 使用系统安装版本，主程序 `.vcxproj` 只复制音频依赖 DLL。音频参数与前期试听来源见 `tools/asset_pipeline/audio_profile.json`，实际使用副本在所选 output 的 audio/profile.json。本地便携包未上传或对外发布。
