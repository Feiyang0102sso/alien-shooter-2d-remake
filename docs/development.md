# 运行时代码与开发

## 原生 Visual Studio 工程

要求 Windows x64、Visual Studio 2022/2026 的“使用 C++ 的桌面开发”和 Windows SDK。直接打开根目录 `AlienShooter.sln`；不需要 CMake、工程生成器、Python 或 JDK。

```bat
build.bat Release
build.bat Debug
build.bat Release run
```

根目录只有一个 `AlienShooter.vcxproj`，直接编译全部游戏 CPP 和 raylib 的八个 C 文件。C++17、工具集、警告、链接库、输出路径和资源复制均在此文件配置；没有静态库项目、测试项目或额外属性/目标文件。`.vcxproj.filters` 管理源码分组，`.vcxproj.user` 设置可执行文件及调试工作目录，均与 `.sln` 同级。`build.bat` 只定位 Visual Studio 并调用 MSBuild，不下载依赖或生成项目。

`src/as_java_re/` 保存游戏包，所有自有头文件使用 `.h`。每个 C++ 文件含英文 `@file` / `@brief`，已有行为注释保留含义并译为英文。第三方代码保持上游文件名及版权声明，例如 `nlohmann/json.hpp`。

`src/3rdparty/` 包含固定版本源码、头文件、导入库、DLL、SoundFont 与许可证，来源及上游哈希见 [第三方说明](third-party-notices.md)。Raylib 从该目录源码编译；FluidSynth 使用原官方二进制。VS 2026 使用 v145，VS 2022 使用 v143，当前验证环境为 VS 2026。

游戏输出到 `bin/Debug/`、`bin/Release/`，中间文件位于 `build/obj/<Configuration>/AlienShooter/`。Debug 保留控制台入口，Release 使用 Windows 桌面入口。运行文件可整体复制；日志仍在存档目录 `logs/latest.log`。

按用户要求已删除 tools 下的测试与原字节码验证工具，后续测试由用户执行。已有验证记录仅作为历史资料。

## 模块与原版对应

| 文件 | 职责 | 原版依据 |
| --- | --- | --- |
| `src/as_java_re/assets.*` | 正式 PNG/JSON/MIDI 索引与强类型参数 | 素材流水线，e.h() 的资源及参数加载 |
| `src/as_java_re/world.*` | 80ms 离散世界，整数坐标、敌人、机关、弹体、地形变化 | e.f() 更新；e.l() 对象/敌人；e.m() 弹体；e.s() 输入 |
| `src/as_java_re/renderer.*` | 原画布、组合部件、动态瓦片、HUD、菜单、文字页和过渡 | e.paint()；d 的位图字库、菜单及富文本绘制 |
| `src/as_java_re/midi.*` | Format 0 MIDI 解析、tempo 转采样位置 | 原始 17 个 SMF 文件，960 PPQN |
| `src/as_java_re/audio.*` | 单活动曲目，独立合成器保存中断状态 | c.a()/b() 的 MMAPI 播放控制语义 |
| `src/as_java_re/game.*` | 桌面输入快照、界面状态、提示等待、关卡衔接 | e 的启动、主菜单、暂停、关卡和失败流程 |
| `src/as_java_re/cheat.*` | CHGOD 连续输入、无敌及临时装备恢复 | 用户指定的桌面秘籍扩展 |
| `src/as_java_re/save.*` | JSON 存档、有效备份、临时文件提交 | 原版选项与最高关卡起点存档，使用 Windows 文件格式替代 RMS |
| `src/as_java_re/platform.*`、`windows_entry.cpp` | Unicode 路径、日志和 Windows 入口 | 桌面适配，不改变玩法 |
| `src/as_java_re/main.cpp` | 参数、窗口、240×320 渲染目标与整数倍显示 | 原 240×320 画布 |
| `src/as_java_re/config.*` | EXE 旁的 cfg、字段名与默认值常量、窗口标题和初始尺寸 | 桌面配置 |

没有通用 ECS 或额外玩法框架。World 只依赖 Assets，不依赖窗口和音频；Renderer 读取世界，Game 消费世界产生的声音编号与消息。敌人数组、物件和弹体保留可直接对应原数组的字段，便于对照。

## 更新顺序

一个正常世界逻辑步依次执行灯光、玩家动作/重力、镜头、物件与敌人、清关判断、弹体、输入，以及原逻辑绘制时更新的动画字段。不能把输入挪到物理前，或让 60Hz 绘制重复消耗随机数。

传送镜头每 50ms 平移 40px，过渡不推进正常物理。提示先按原规则落地，再展开和逐字显示；确认后继续。死亡单独执行粒子和红色幕布阶段。主循环累计帧时间，每 80ms 推进一次；短按跳跃和切枪保留到下一逻辑步。

震屏位移在逻辑绘制阶段保存到 `drawOffsetY`，屏幕刷新只读取，不再次衰减震幅。原版震屏会留下上一帧边缘像素，因此世界绘制保留固定渲染目标，不能在每次绘制前整屏清空；界面页面自行覆盖背景。路线截图工具也必须绘制中间每一步和提示准备完成后的底图，不能只跳到抽样帧绘制。

生成器的参数槽与场景物件分开保存。每次遍历重新按仍为 36/37 型的物件编号；37 型销毁后变为 33 型，后续物件会使用前一参数槽，而已出生敌人保留出生时的槽编号。原版清场计数在生成器更新前累加，爆破当步仍计入；不要改成更新后的全场重数。

坐标沿用原版左下原点，地图和碰撞单位 16px。原版矩形命中、整数截断、箱堆随机高度、弹体链表倒序以及原有难度加载顺序均应保留。生产随机种子来自启动时间，跨关继续同一随机流；测试明确使用 12345。

## MIDI 时序

事件 delta 先累计为整数时间分子，再换算为 44100Hz 样本位置，避免逐事件舍入误差积累。音频回调按事件边界分段合成；游戏帧率、OGG 释放尾音不会成为 MIDI 音序时钟。17 个合成器启动时加载同一音色，各自保留采样位置、事件游标和声音包络；只渲染当前活动曲目。

新声音打断旧声音，旧曲目冻结；再次请求未结束的曲目从冻结位置继续，结束后再次请求才重置。这是当前原版调用路径的行为，不是现代游戏常见的多音效混音。参数为 44100Hz、立体声、gain 0.2、polyphony 256、GM bank mode、混响/合唱关闭。

## 音色库裁剪

运行时 `src/3rdparty/generaluser/GeneralUser-GS.sf2` 是 GeneralUser GS 2.0.3 的游戏专用子集，大小从 32,319,396 减为 5,167,308 字节（减少 84.01%）。17 个 MIDI 实际发声使用 22 种旋律音色和标准鼓组的 25 个键位；为保持通道初始化及无音符的 Program Change，最终保留 29 个预设入口、60 个乐器和 102 个采样。

工具只移除当前 MIDI 的音高/力度不会触发的区域与未引用内容；保留全局区域、调制器、完整原采样、循环点、采样率与包络参数，仅重写索引。没有发声事件的默认/选中预设保留中央 C、力度 100 对应区域。修改 MIDI 后必须重新裁剪并比较音频，不能把此子集当作完整 GM 音色库。SoundFont 表结构依据 [SoundFont 2.01 规范](https://www.synthfont.com/SFSPEC21.PDF)。

整包处理统一运行 `tools/run_pipeline.bat "游戏包路径"`，自动按输入 MIDI 裁剪，写入 tools/output/audio/synth/，并放入 tools/output/runtime/audio/synth/。完整母版及下载依赖位于 output/.cache/。不计算哈希、不自动运行音频对比测试，测试由用户自行执行。

## 调试入口

```powershell
.\bin\Debug\AlienShooter.exe --level 6 --seed 12345
.\bin\Debug\AlienShooter.exe --silent --hidden --level 1 --frames 60 --screenshot build\shot.png
.\bin\Debug\AlienShooter.exe --save-dir build\test-save
```

`--assets` 可指定资源根目录；`--screen` 支持 menu/help/about/difficulty/settings，pause 需同时指定 `--level`。`--frames` 达到指定渲染帧数后退出；`--screenshot` 保存 240×320 逻辑画布。F3 显示碰撞网格。调试直接指定关卡不等同于正常解锁进度。

需要改素材时修改 `tools/asset_pipeline/` 后生成到 output/runtime，使用 `--assets "输出目录/runtime"` 验证；确认后再将素材接入项目发布目录。运行时直接读取声音记录的 `midi_source`，不读取历史 OGG `file`。便携包省去只供离线试听的 OGG 文件。
