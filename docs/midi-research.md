# MIDI 播放与还原调研

调研日期：2026-10-03。范围：重新生成的 `c.java`、`e.java` 播放入口、游戏包内全部 17 个 `.mid`，以及 Oracle MMAPI 和 MIDI 官方资料。本轮只读解析 MIDI，没有转码、安装软件或试听原机。

后续状态（2026-10-04）：已按用户要求实现原 MIDI 实时播放，使用前期确认的 FluidSynth 2.6.1 / GeneralUser GS 2.0.3 参数。17 首离线合成、采样计数、中断/续播及 EOT 重播均已自动验证，并完成真实 WASAPI 输出。运行时不使用 OGG 决定时间轴；实现见 [开发说明](development.md)，验证边界见 [验证记录](verification.md)。以下保留最初调研证据，不将桌面音色等同于原手机音色。

## 结论

**游戏没有自带 MIDI 解析器或合成器。它把标准 MIDI 文件交给手机的 Java ME MMAPI，由设备提供实际音序器、合成器和音色库。** 仅凭当前游戏包，无法认定底层是某个具体厂商引擎，更不能直接认定为 Java SE 的 Java Sound、Windows 合成器或某个 SoundFont。

全部 17 个文件可按标准 SMF 结构解析到文件末尾：Format 0、单轨、960 ticks/quarter note，无 SysEx、无 Bank Select、无 Pitch Bend。它们不是需要专用解密的音乐包。音色编号和通道使用方式与 GM 习惯相符，但文件未发送 GM System On，不能据此确定原机具体 GM 实现或 GS/XG 默认模式。

**用户补充：此前多个转换器输出不正确且有杂音，最后才找到转换正确的结果。** 这一现象值得认真对待，不能简单解释为“换个 SoundFont 就好”，也不能在不知道转换器与参数的情况下归因于某个具体缺陷。本游戏大量用合成音色、特殊效果音色与鼓声叠加制造音效。文件事件解析、合成器渲染和输出增益/削波是不同环节，都应与那份已认可的结果对照。

## 1. 实际播放路径

| 环节 | 源码事实 | 位置 |
| --- | --- | --- |
| 文件选择 | `0.mid` 至 `15.mid`；编号 16 对应 `mm.mid` | [e.java:468](E:/coding_projects/c_projects/alien_shooter_java_re/Alien_Shooter/e.java:468) |
| 载入 | 从资源流读取；默认类型 `audio/midi`，只有 `.amr` 后缀走 `audio/amr` | [c.java:16](E:/coding_projects/c_projects/alien_shooter_java_re/Alien_Shooter/c.java:16) |
| 创建播放器 | `Manager.createPlayer(var3, var4)`，随后 `prefetch()` | [c.java:26](E:/coding_projects/c_projects/alien_shooter_java_re/Alien_Shooter/c.java:26) |
| 启动 | 对目标 Player 调用 `start()` | [c.java:59](E:/coding_projects/c_projects/alien_shooter_java_re/Alien_Shooter/c.java:59) |
| 单活动播放器 | 静态字段保存当前对象；符合优先级时先停止旧对象，再启动新对象 | [c.java:55](E:/coding_projects/c_projects/alien_shooter_java_re/Alien_Shooter/c.java:55) |
| 优先级 | 当前游戏入口只调用单参数构造函数，其优先级为 0，因此新请求可打断当前请求 | [c.java:35](E:/coding_projects/c_projects/alien_shooter_java_re/Alien_Shooter/c.java:35)、[e.java:480](E:/coding_projects/c_projects/alien_shooter_java_re/Alien_Shooter/e.java:480) |
| 播放编号 | `a(int)` 先过 `i(int)`，但本版本 `i` 原值返回 | [e.java:4663](E:/coding_projects/c_projects/alien_shooter_java_re/Alien_Shooter/e.java:4663)、[e.java:3873](E:/coding_projects/c_projects/alien_shooter_java_re/Alien_Shooter/e.java:3873) |

Oracle 将 `Manager` 定义为获取系统相关播放器资源的入口；MMAPI 暴露设备本机多媒体服务，而不是规定唯一音频后端。[Oracle Manager](https://docs.oracle.com/javame/config/cldc/opt-pkgs/api/mm/jsr135/javax/microedition/media/Manager.html)、[Oracle MMAPI](https://www.oracle.com/java/technologies/mmapi.html)

Oracle 的 MIDIControl 文档也明确区分设备提供的内部音色库与应用加载的自定义音色库。当前源码没有加载自定义音色库，也没有调用 MIDIControl、VolumeControl、TempoControl 或 PitchControl。[Oracle MIDIControl](https://docs.oracle.com/javame/config/cldc/opt-pkgs/api/mm/jsr135/javax/microedition/media/control/MIDIControl.html)

包内 [MANIFEST.MF](E:/coding_projects/c_projects/alien_shooter_java_re/Alien_Shooter/META-INF/MANIFEST.MF) 声明 CLDC-1.0、MIDP-1.0、NET Lizard，但没有原手机型号或声音引擎标识。Manifest 的 MIDP 字段本身不足以确定具体 MMAPI 后端。

## 2. 全部文件实测

方法：在 PowerShell 内存中解析 `MThd`、`MTrk`、大端 chunk 长度、VLQ delta time、running status、通道事件及 Meta/SysEx 长度，检查轨道终点与文件终点完全一致。下表时长按 tempo map 积分至 End Of Track，**不是实际可听声音长度，也不包含合成器释放尾音**。全部文件均为 Format 0、1 轨、PPQN 960。

| 文件 | 字节 | 有效 Note On 数 | Running status 事件数 | EOT tick | EOT 秒，约 |
| --- | ---: | ---: | ---: | ---: | ---: |
| 0.mid | 2799 | 364 | 250 | 7560 | 1.890 |
| 1.mid | 309 | 25 | 21 | 1920 | 0.480 |
| 2.mid | 688 | 76 | 40 | 2640 | 0.971 |
| 3.mid | 171 | 9 | 10 | 1080 | 0.450 |
| 4.mid | 179 | 10 | 10 | 1200 | 0.500 |
| 5.mid | 512 | 49 | 18 | 3840 | 0.960 |
| 6.mid | 397 | 36 | 33 | 7680 | 2.160 |
| 7.mid | 498 | 54 | 63 | 3840 | 1.600 |
| 8.mid | 376 | 31 | 20 | 3840 | 0.960 |
| 9.mid | 343 | 27 | 37 | 5568 | 1.392 |
| 10.mid | 3438 | 428 | 260 | 49920 | 14.182 |
| 11.mid | 4814 | 595 | 363 | 61920 | 19.350 |
| 12.mid | 651 | 70 | 39 | 2880 | 0.720 |
| 13.mid | 667 | 70 | 39 | 7680 | 2.160 |
| 14.mid | 811 | 85 | 7 | 3840 | 1.043 |
| 15.mid | 2328 | 300 | 192 | 6240 | 2.053 |
| mm.mid | 7287 | 926 | 507 | 99840 | 39.000 |

结构结论：

- 全部文件含 tempo `FF 51`、拍号 `FF 58`、调号 `FF 59`、End Of Track `FF 2F` 和文字类 Meta 数据；未发现专用采样数据或 Sequencer Specific `FF 7F`。
- 全部没有 `F0/F7` SysEx，因此未发现 GM/GS/XG reset、厂商参数块或设备专用音色上传。不存在 SysEx 不等于原设备不支持 GS/XG；只能说明这些文件没有显式请求相应扩展。
- 通道控制器只有 CC7（通道音量）；未发现 CC0/CC32 音色库选择、CC64 延音、CC91/CC93 混响/合唱发送或 RPN/NRPN。合成器默认音色库及效果设置仍会影响结果。
- 没有 Pitch Bend。不要先把排查重点放在弯音范围或 RPN 上。
- `6.mid` 与 `13.mid` 在 tick 3840 从 300000 改为 240000 微秒/四分音符。其他多次 tempo 事件通常重复现有速度；`14.mid` 在终点还有 tempo 事件，不能把它当作整段速度。
- 全部使用 running status，解析时不能要求每条通道消息都带状态字节。该编码属于 SMF 正常机制。[Microsoft running status 说明](https://learn.microsoft.com/en-us/windows/win32/multimedia/receiving-running-status-messages)

SMF 的用途是交换带时间信息的 MIDI 事件，音轨、速度及拍号属于文件描述内容；它不是录好的 PCM 波形。[MIDI Association：Standard MIDI Files](https://midi.org/standard-midi-files)

## 3. 音色与节奏风险

文件内 Program Change 为 **0–127 编号**。常见 GM 表格写成 1–128，核对时需加一，不能把编号偏一误诊为音色库问题。

| 文件中的编号 | GM 表格编号与名称 | 例子 |
| --- | --- | --- |
| 127 | 128 Gunshot | 2、3、4、15.mid |
| 126 | 127 Applause | 0、2、5、6、7、12、13、14、15.mid |
| 122 | 123 Seashore | 9.mid |
| 119 | 120 Reverse Cymbal | 6、12、13.mid |
| 117 | 118 Melodic Tom | 3、4、5、6、8、9、12、13.mid |

名称按 [Finale 官方 GM 表](https://usermanuals.finalemusic.com/SongWriter2010Mac/Content/General_MIDI_Table.htm) 对照。它们是音色名称，不直接等于游戏事件名称。例如某文件里使用 Applause，不代表游戏一定在播放“掌声”；多通道、不同音高和短时音符可以共同构成另一个音效。

GM 规范统一音色集合与部分行为，目的是改善跨设备一致性，并未使 MIDI 文件自带原机采样。[MIDI Association：General MIDI](https://midi.org/general-midi)

据此，本项目需要按以下顺序排查，而不是随便更换音色库后宣布还原：

1. **事件正确性**：running status、Program 编号、通道编号、Note On velocity 0 的关音语义、tempo map、同 tick 事件原序必须保留。
2. **鼓与特殊效果**：实际数据大量使用第 10 通道及上表特殊音色。比较原机与转换结果的起音、噪声成分、音高响应和释放包络；这些声音的差别容易在短音效中暴露。
3. **复音与混音**：核实原机声部上限、抢占策略、音量曲线、默认混响/合唱和采样率。更大的音色库或更多声部，不自动代表更接近原机。
4. **速度与尾音**：按 tempo map 算时间；完整保留原始 MIDI 中的等待时间，并单独记录导出尾音策略。不能自动剪静音、归一化或强行统一速度后再做还原对比。

后面三项是需要实验验证的差异来源，目前没有原机录音或用户认可的转换结果，不能断言已经发生在某一具体转换器上。大量 running status 只说明解析器必须正确支持它；本轮完整解析成功并不是“它导致转换器杂音”的证据。应先找回当时成功的转换器、版本、音色库及参数，或直接取得成功输出作为基准，再判断是否需要追到原手机后端。

## 4. 容易漏掉的播放语义

源码只调用 `stop()` 和 `start()`，没有 `setMediaTime(0)`，也没有 `setLoopCount()`。

MMAPI 规定：`stop()` 暂停当前位置；随后 `start()` 从暂停位置继续；只有已经到达媒体末尾时，`start()` 才自动从头开始。默认循环次数为 1。[Oracle Player：start、stop、setLoopCount](https://docs.oracle.com/javame/config/cldc/opt-pkgs/api/mm/jsr135/javax/microedition/media/Player.html)

因此，依据规范推导：被另一声音打断的 Player 下次可能续播；对正在播放的同一对象重新请求，也会先停再继续。将每次游戏请求简单改成“从 WAV 第 0 帧播放”，会改变这一行为。是否存在老手机实现偏差，必须通过目标设备或认可的模拟器验证。

单活动 Player 仅表示不同文件之间的抢占策略；一份 MIDI 内部仍是多通道、可同时发声。不要将其误解为单音合成器。

## 5. 1:1 还原需要的证据和路线

1. **定义参照**：优先查明用户此前转换成功的工具、版本、配置及输出；若以手机体验为准，则确定手机与固件，或模拟器与版本。相同 JAR 在不同设备上的声音可以不同；游戏包未提供足够信息来唯一确定“原声”。
2. **保留母版**：原样保留这 17 个 MIDI；记录原始 SHA-256、解析统计、tempo map、Program/通道清单与游戏编号。当前资源提取阶段不做不可逆音色替换。
3. **取得对照**：在参照环境逐个播放并录制完整 PCM，同时录制短音效连续触发、不同音效互相打断和播放至结束后重播三种情况。
4. **选择实现**：若能复用参照环境的相同声音后端及音色数据，可固定其版本和参数进行离线渲染；若只能取得录音，优先使用逐文件录制素材，并按照实验确定的中断/续播语义实现运行时播放。
5. **验收后导出**：对比节奏、起音、尾音、特殊音色、峰值及中断行为。确认后输出固定格式的 WAV/OGG，记录后端、音色库版本/哈希、采样率和效果配置。

只拥有同一份 SoundFont 仍不足以保证完全相同；合成器插值、包络、增益、效果与调度也需要一致。若目标是最终 PCM 逐采样一致，还需稳定的确定性数字输出；手机扬声器的模拟录音只能作为听感基准。上述为工程验收建议，当前尚未执行。

**本阶段决策：完整提取和保留原始 MIDI，不以普通转换器输出覆盖母版，不承诺尚无参照条件的 1:1 音频。** 图像、地图和英文文本导出可以独立继续。

## 6. 2026-10-04 音频试听进展

用户希望先将全部声音转换为 OGG，再通过试听选择最终方案；此前成功的在线转换器名称已无法确认。本次采用 FluidSynth 2.6.1、GeneralUser GS 2.0.3 和 FFmpeg/libvorbis，生成 17 个 OGG，并保留浮点 WAV 对照。原 MIDI 和正式声音映射保持不变。

这是通用音色的试听基线，尚未声称与原机一致。使用 44.1kHz 双声道、固定增益 0.2、关闭额外混响与合唱，不自动归一化或裁剪合成器输出。某些文件的低电平释放尾音和尾部静音很长，渲染报告同时记录 MIDI 事件时长与实际音频时长。峰值和解码校验只能排除部分技术错误，不能代替听感确认。

工具来源：[FluidSynth 官方发布](https://github.com/FluidSynth/fluidsynth/releases/tag/v2.6.1)、[GeneralUser GS 作者仓库](https://github.com/mrbumpy409/GeneralUser-GS)。完整复现配置现位于 `tools/asset_pipeline/audio_profile.json`，试听入口默认为 `tools/output/audio/rendered/index.html`。当时计划通过 raylib 播放 OGG；后续已按用户要求改为实时 MIDI，当前实现见本文开头说明。

同日后续：用户试听后确认听感可用，已将该配置作为重建音色方案。当前整包入口将 17 个声音的渲染结果、校验报告、原 MIDI 及裁剪音色统一放入选定 output；runtime/audio/sounds.json 保留 OGG 的 file 和游戏使用的 midi_source。保留此次确认时的增益、效果开关、音色库和完整尾音；这项听感验收不等于已证明原机逐采样一致。
