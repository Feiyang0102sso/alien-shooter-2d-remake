# Alien Shooter — C++17 / Visual Studio 重建

基于 NET Lizard 的 Java ME 版 Alien Shooter，使用前期提取的原关卡、部件图集、位图字库和 MIDI 重建 Windows 游戏。当前已实现八关运行时、五类敌人、五种武器、原版机关与菜单、暂停、失败重试和关卡起点存档。

**运行：** 构建后打开 `bin/Release/AlienShooter.exe`。同目录包含素材、MIDI、音色和音频依赖 DLL；默认用户已安装 x64 MSVC 运行库，运行不依赖 Python、Java 或原游戏包。

**Visual Studio：** 直接打开根目录 `AlienShooter.sln`，选择 x64 Debug / Release。solution 只有 `AlienShooter` 一个主程序项目；同级提供 `.vcxproj`、`.vcxproj.filters` 和 `.vcxproj.user`，不使用 CMake 或生成步骤。

## 构建

安装 Visual Studio 2022/2026 的“使用 C++ 的桌面开发”和 Windows SDK。固定版本的依赖已放入 `src/3rdparty/`，构建无需联网下载。

```bat
build.bat
build.bat Debug
build.bat Release run
```

项目只保留一个 `build.bat`，默认构建 Release；`run` 构建后启动游戏。IDE 和批处理均使用同一个主程序项目，自动将运行依赖复制到 `bin/<Configuration>/`。游戏编译不要求 Python 或 JDK；可选回归验证独立运行，见 [验证记录](docs/verification.md)。

版本号只需修改 `src/as_java_re/version.h` 中的 `ALIEN_VERSION_STRING` 字符串，格式为四段数字，如 `1.0.0.0`。资源版本、启动日志和构建时生成的 manifest 自动同步；产品名称、文件描述和原始文件名也在该头文件中维护。

## 操作

| 输入 | 动作 |
| --- | --- |
| A / D | 左右移动 |
| W | 跳跃；蹲下时先站起 |
| S | 停稳后蹲下、向下看、进入已启用终端 |
| 鼠标左键 | 朝角色面向开火，按住连发 |
| 1 / 2 / 3 / 4 | 机枪 / 喷火器 / 火箭筒 / 爆能枪，须先拾取 |
| 5 | 默认手枪 |
| Q / E | 前后切换已取得武器 |
| Esc | 暂停、返回；主菜单退出 |

菜单使用 W/S、Enter 或鼠标点击。按原版保留左右射击、空中不能开火和攻击期间锁定横向移动；站姿射击可按 W 取消并起跳。完整说明见 [游玩指南](docs/player-guide.md)。

## CHGOD 秘籍

在游戏中连续输入 `chgod`（不区分大小写）开启无敌，并获得全部武器和各武器满弹药。弹药仍按原版规则消耗。再次输入关闭，恢复开启前的武器、弹药和选枪；关闭时中断当前攻击。菜单、暂停和失焦时不接收秘籍。

重开、离开游戏或确认过关时自动关闭，临时装备不会写入关卡起点存档。实现位于 `src/as_java_re/world/cheat.cpp` 和 `cheat.h`，秘籍字符串在 `core/constants.h`。

## 实现与验证

- C++17、raylib 5.5、nlohmann/json 3.12.0、原生 Visual Studio / MSBuild。
- 原 240×320 画布，最近邻整数缩放，80ms 离散世界更新；保留原对象/动画/碰撞数值和地图变化。渲染时在两次 tick 之间对摄像机、角色、物件和弹体位置做线性插值（仅影响显示，逻辑不变；瞬移超过 64px 直接跳转）。
- 直接解析全部 17 首原 MIDI，用 FluidSynth 2.6.1 / GeneralUser GS 2.0.3 按采样计数调度，保留打断、冻结和续播；运行时不使用 OGG 尾音计时。
- 运行时音色库已按这 17 个 MIDI 裁剪至 5,167,308 字节（约 5.17 MB），比原版 32.3 MB 减少约 84%；保留所用采样与音色参数，裁剪前后合成输出逐样本一致。复现方法见 [开发说明](docs/development.md)。
- Debug / Release 当前均通过 13 组原生回归测试，包含原字节码动作/战斗/机关/难度/死亡样本、前两关完整路线与输入边界逐步对照、MIDI 合成、存档、八关流程及 CHGOD 秘籍。
- 前两关全部 5307 个逻辑帧，加上 55 张界面及动画参考图，共 5362 张在既定 Java2D 参考条件下零像素差。
- 两关独立正常输入通关已通过真实 Game 流程：第一关 3020 步、生命 25、分数 135；第二关 2287 步、生命 55、分数 210。每步状态均与原字节码比较，没有修改位置、生命、装备或通关标志。
- 八关另有 32 组正常输入、共 5890 步扩展对照全部匹配；其中六组 1114 步边界场景纳入固定测试。

其余六关尚未完整实玩，原机音色和设备显示也未作全面比对。验证范围与有意桌面适配分别见 [验证记录](docs/verification.md)、[已知差异](docs/known-differences.md)。

## 目录

```text
AlienShooter.sln       仅包含主程序的 Visual Studio solution
AlienShooter.vcxproj   全部源码、C++17 编译链接与资源部署配置
AlienShooter.vcxproj.filters  VS 源码分组
AlienShooter.vcxproj.user     VS 调试启动与工作目录
src/as_java_re/        C++ 游戏代码，统一 .cpp / .h，按包分目录：
  core/                常量总表 constants.h、素材加载 assets
  world/               80ms 离散世界：关卡、地形、角色、玩家、敌人、物件、弹体、特效、秘籍
  render/              像素图元、精灵/文字、世界与 HUD、菜单/提示框/富文本页
  audio/               MIDI 解析与 FluidSynth 播放
  game/                桌面输入、界面流程、存档
  app/                 入口 main、config.cfg 读取
  platform/            Windows 路径、日志与入口
src/3rdparty/          固定版本依赖、音色、DLL 和许可证
resources/            Windows 图标、版本与 UTF-8 清单
assets/               正式素材及原 MIDI
bin/Debug|Release/     游戏 exe 及运行文件
build/                编译中间产物、参考数据和验证日志
tests/                C++ 回归与已提交原版基线
tools/                Python 素材及原字节码验证工具
docs/                 开发、验证、玩法和阶段总结
```

窗口配置读取 EXE 旁的 `config.cfg`，由 `src/as_java_re/app/config.cpp/.h` 管理，字段名和默认值统一定义在 `core/constants.h` 的 `config` 命名空间中。`DefaultScreenX/Y` 为窗口基准尺寸，乘以 `Scalar` 得到初始窗口尺寸；默认 `240×320 × 3 = 720×960`，小屏自动降低倍率，游戏逻辑画布保持 `240×320`。`MaxFps` 为帧率上限（默认 120）；显示器刷新率高于该值时关闭 VSync，交给限帧器控制。支持 `;` 行尾注释，修改后重启生效。启动时缺少 cfg 会按常量生成默认文件，重新构建不会覆盖已有设置。

存档在 EXE 旁的 `save/progress.json`，日志在 `save/logs/latest.log`，与启动时的工作目录无关。调试参数 `--save-dir` 仍可指定其他目录。旧的 `%LOCALAPPDATA%\AlienShooterRemake` 存档不会自动迁移；需要继续旧进度时，可将旧存档复制到新的 `save/`。完成关卡后保存下一关起点；第一关未过关时 CONTINUE 不可用。损坏主存档可从有效备份恢复。

## 常量与素材数据

- 所有具名常量集中在 `src/as_java_re/core/constants.h`：画布与节拍、原版物件/瓦片/音效编号、教程位、玩法规则、布局坐标、颜色、音频参数、config.cfg 键名和默认值。改数值前先确认对应原版行为确实要变，回归测试按原字节码对照。
- 素材里已有的数据一律从 `assets/` 读取，不在代码里重复：显示尺寸校验 `config/display.json`；关卡列表、贴图清单、帮助/关于页路径来自 `catalog.json`；动画区间、开火帧来自 `character_animations.json`；蹲姿/护甲/换枪部件来自 `player_poses.json`；阴影、发电机灯光与电弧颜色、震屏衰减来自 `visual_effects.json`；物件拼接、终端门、弹体与 HUD 精灵来自 `object_visuals.json`；界面文字按名字取自 `localization/en/interface.json`。
- 桌面版文字（键盘键位说明、终端与换枪提示）由素材流水线直接写入运行时素材，游戏不再在运行时替换文本。
- 程序手绘的特效（发电机电弧、开关激光、倒计时、终端箭头、死亡幕布）仍在渲染代码中实现，几何参数在 `constants.h` 的 `effect_shape` / `layout` 中。

## 素材流水线

只处理本游戏，入口只接受整个游戏包的一个路径：

```bat
tools\run_pipeline.bat "游戏目录或 JAR、ZIP 路径"
```

输出固定为 tools/output/，运行素材在 runtime/，浏览页为 index.html。首次自动准备依赖，环境和下载也在 output 内；需要已安装 Python 3.10 或更新版本。再次运行覆盖旧产物，保留依赖缓存。无需指定其他路径，不计算 SHA256，不运行测试。tools/tests、tools/reference、tools/metadata 已删除，历史验证记录保留在 docs。

- [开发构建与源码对应表](docs/development.md)
- [阶段总结](docs/progress.md)
- [素材工具和编码细节](tools/README.md)
- [正式素材目录](docs/runtime-assets.md)
- [源码调研](docs/source-research.md)
- [MIDI 调研](docs/midi-research.md)
- [第三方与素材来源](docs/third-party-notices.md)

