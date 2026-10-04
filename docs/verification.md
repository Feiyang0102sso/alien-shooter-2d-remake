# 验证记录与复现

> 历史记录：按用户最新要求，tools/tests、tools/reference 及自动验证入口已删除，下文旧命令不再适用。后续测试由用户自行执行。

日期：2026-10-04。环境：Windows x64，MSVC 19.51，原生 MSBuild 18，raylib 5.5，FluidSynth 2.6.1。

## 自动测试

Debug 和 Release 均通过当前全部 13 组原生回归测试。

单主程序工程的最新日志为 `build/verification/single-debug-build.log`、`single-release-build.log`、`single-debug-verification.log`、`single-release-verification.log`。两种配置构建无编译警告，本轮 107 张抽样图全部匹配；源码玩法与渲染逻辑未在本轮调整。

| 测试 | 实际覆盖 |
| --- | --- |
| `runtime_contracts` | 资产/MIDI 结构、8 关初始化网格、400 步玩家动作、862 步敌人与武器原字节码样本、护甲及输入限制 |
| `original_world_traces` | 八关各 160 步，共 1280 步；敌人、物件和最终地形 |
| `original_mechanism_traces` | 16 类主动机关场景，共 2411 步；平台、供电、生成器、破坏物、墙、终端和出口 |
| `original_difficulty_order` | 同进程六次难度切换，初始敌人和后续生成敌人的血量 |
| `original_death_sequence` | 原死亡方法 23 次重绘的粒子、幕布列高与完成时点 |
| `desktop_campaign_and_save` | 输入快照、失焦暂停、菜单、重开、八关顺序、装备继承、旧关重玩、继续、死亡重试和存档恢复 |
| `midi_playback_contracts` | 17 首全量离线合成、非静音/有限数值、打断冻结、续播、禁音、暂停、重触发、EOT 重播 |
| `normal_input_campaign_playthrough` | 第一关正常输入通关及下一关实际存档，不修改位置、生命、装备或通关标志 |
| `original_normal_input_playthrough` | 同一正常路线的 3020 步逐项对照原字节码，包括角色、相机、敌人、物件、可破坏物、弹体、武器、弹药和分数 |
| `normal_input_level_two` | 第二关正常输入通关，并验证实际进入第三关和保存进度 |
| `original_level_two_playthrough` | 第二关 2287 步逐项对照，覆盖生成器销毁后的动态编号、补敌人和清场提示时点 |
| `chgod_contracts` | 大小写与连续输入、错误前缀、致命伤免疫、攻击中关闭、失焦/暂停、重开及过关存档隔离 |
| `original_input_edges` | 六组正常输入共 1114 步：台阶重力、低头松键、火焰致死、平台挡墙、射击取消跳跃、平台起跳 |

两条正常路线均从固定种子 12345 和该关默认装备独立开始；不把独立第二关测试描述为继承第一关装备及随机流的连续通关。

| 关卡 | 逻辑步 | 结束生命 | 分数 | 剩余敌人 / 生成器 |
| --- | --- | --- | --- | --- |
| 第一关 | 3020 | 25 | 135 | 0 / 0 |
| 第二关 | 2287 | 55 | 210 | 0 / 0 |

`level_01_playthrough.json` 和 `level_02_playthrough.json` 保存实际操作，在纯 World 和桌面 Game 路径均完成。`original_playthrough.json` / `original_playthrough2.json` 保存原 class 哈希、路线规范化 JSON 的 SHA256 及每一步完整参考状态，包括清场标志。其余六关尚未进行等价的完整正常输入通关。

静止观察场景每步恢复玩家生命，机关测试按各场景布置位置/装备，八关流程测试注入通关标志。这些测试有明确用途，不是作弊通关后宣称整游戏验收。前两关操作路线属于无状态注入的连贯通关验证。

另外运行八关各四组、共 32 组正常输入扩展对照，5890 个步骤全部匹配。部分场景以玩家死亡自然结束，这些只证明相应输入下的行为，不能算作通关。`input_routes.json` 保留其中六个关键回归；完整扩展可用 `tools/reference/explore_inputs.py --java-home <JDK目录>` 重跑，结果写入 `build/verification/exploration/summary.json`。测试中的 `jump:true` 表示一次 W 按下脉冲，连续多个步骤表示重复按下。

## 像素与原字节码

第一关全部 3020 帧、第二关全部 2287 帧与 55 张界面/动画参考图，共 **5362 张 240×320 图像零像素差**。55 张固定图包括：8 关初始画面、3 个静态菜单、6 个帮助/关于位置、20 个提示框阶段、4 个死亡阶段、12 个菜单动画阶段、2 个声音询问状态。

完整路线的截图为每步原绘制完成后的世界画面，提示阅读等待不在这 5307 帧内；提示和菜单由上述独立样本覆盖。默认验证抽取两条路线中每 100 步，共 52 张，加上固定图共 107 张；阶段十四使用全帧模式，阶段十五仅调整工程组织，重新执行默认抽样。

参考环境直接加载用户原包的 `.class`，只将常量池中的 System 类引用指向确定性时钟适配；没有重编译反编译源码替代游戏方法。MIDP 图形交给 Java2D，音频为 stub。使用固定种子、同一英文译文、桌面振动禁用状态和 EXIT 标签；帮助页仅扩展翻译行缓冲容量。结论限于这些对照条件，不等同于真实手机型号的全部像素与时序。

完整路线保留提示前落地、内部重绘、传送镜头期间的动画/随机状态变化，以及镜头结束后的输入处理。只省略等待玩家读完提示的时间；参考时钟每个逻辑阶段增加 80ms，不模拟真实手机阻塞等待中的墙钟流逝。桌面数字键选枪对应原版武器切换方法，仅扩展选择方式。

在已配置 x64 MSVC 环境的 PowerShell 中执行（Visual Studio Developer PowerShell，目标架构 x64）：

```powershell
.\build.bat Release
. .\.venv\Scripts\Activate.ps1
python tools/reference/verify.py --checks-only
python tools/reference/verify.py --java-home "C:\path\to\jdk" --full-playthrough-pixels
```

完整重生成需要包含 `javac` 的 JDK（当前验证使用 JBR 25）、项目 `.venv` 中的 Pillow，以及原解包目录；默认原包位置为工程父目录的 `Alien_Shooter`，可用 `--original-root` 指定。运行工具前激活项目虚拟环境，不使用系统 Python 包环境。

验证脚本直接调用 MSVC，复用主程序对象文件编译验证工具，输出到 `build/verification/bin/<Configuration>/`；不创建额外 VS 项目。先运行 13 组回归，再执行原版参考场景，将基线导出到 `build/reference/fixtures/`，与 Git 中的 `tests/fixtures/` 比较，不自动修改已提交期望值。随后调用 C++ 真实 GPU 绘制，默认报告位于 `build/verification/pixel-comparison.json`，全帧报告为 `pixel-comparison-full.json`；参考图保存在 `build/reference/`。

日常修改先运行 `build.bat Debug` 或 `build.bat Release`，再执行 `python tools/reference/verify.py --configuration Debug --checks-only`（或 Release）。只跑 C++ 回归无需 JDK。只有原包或参考适配层变更时才需要重生成基线；必须先检查 class 哈希和差异原因。

## 素材流水线验证

整包入口只针对本游戏，输入路径和输出路径均由参数提供：

```powershell
. .\.venv\Scripts\Activate.ps1
$env:ALIEN_TEST_SOURCE = "本游戏解包目录"
python -m unittest discover -s tools/tests -v
```

2026-10-04 实测 10 项通过，包含原目录与无 Java 源码的 JAR、外层目录定位、中文和空格路径、其他工作目录调用、65 个 runtime 文件一致性、缺资源/缺依赖失败后的输出保留，以及原包和项目现有素材不变。未设置 ALIEN_TEST_SOURCE 时跳过原包集成用例，其他检查正常运行。完整记录为 build/verification/pipeline-tests.log，其中缺合成器的报错属于主动注入的失败场景。

默认 tools/output 的独立构建及迁移后再次校验通过：2026 项产物哈希、65 个源文件快照、170 个双向组合帧、数值表、八关记录和原 MIDI 均核对。65 个 runtime 文件中，原有 63 个与项目 assets 一致，新增裁剪音色和许可；音色哈希与阶段十六一致。默认构建日志为 build/verification/pipeline-default.log，提取/校验日志随产物放在 output/logs/。本轮未重新执行原生游戏和声卡验证，相关结论保留前轮范围。

## 实际设备验证

音色库裁剪另用完整母版对照游戏实际音频回调：17 个 MIDI 各覆盖全长和末尾 10 秒，再覆盖不规则缓冲长度下的中断、续播、暂停、禁音与重播；共 12,108,166 个双声道采样帧数值完全一致。记录在 `build/verification/soundfont/zone-comparison.log`，该结论只适用于清单中哈希固定的 MIDI 和当前合成参数。

隐藏但真实的 OpenGL 窗口已验证主界面、八关、文字页和动画截图；Release 在真实 WASAPI 声卡上初始化并播放 MIDI，正常退出。便携包从包含中文和空格的目录、以系统临时目录为工作目录启动，素材、音色、Unicode 存档/日志/截图路径均通过；最终打包核对结果另见阶段总结。

## 仍需人工观察

剩余六关的完整实玩、不同随机种子下的整局体验，以及原手机音色/刷新率/物理屏幕差异尚未证明。自动测试证明已列举的样本和规则，不能代替这些观察。
