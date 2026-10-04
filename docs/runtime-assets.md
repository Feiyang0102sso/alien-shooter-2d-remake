# 游戏素材目录

资源入口为所选素材目录中的 `catalog.json`，JSON 内路径均相对此目录。项目现有素材位于 `assets/`；流水线新生成的素材位于 `output/runtime/`，可用游戏 `--assets "输出目录/runtime"` 加载，不依赖原包或其他中间产物。output 固定为 `tools/output/`。2026-10-04 已实现 C++ 加载器和游戏运行时，见 [开发说明](development.md)。

运行时按用户要求直接读取 `audio/sounds.json` 中的 `midi_source`，通过 FluidSynth 合成；历史 OGG 仍供素材工具试听。构建输出另提供 `audio/synth/GeneralUser-GS.sf2` 及其许可，便携包不附带 OGG。下文保留素材流水线的完整产物约定。

## 正式产物

| 路径 | 用途 |
| --- | --- |
| `textures/characters.png` | 角色、敌人、武器、子弹、掉落物及部分特效的部件图集 |
| `textures/world.png` | 三套场景主题的瓦片图集，重复像素合并存储 |
| `textures/interface.png` | 菜单、启动、加载等界面图片与 21 个帮助图标 |
| `fonts/pixel_green.png`、`.json` | 原绿色位图字库、100 个字形及排版参数 |
| `definitions/character_parts.json` | 155 个部件编号、左右朝向裁切区域及绘制偏移 |
| `definitions/character_frames.json` | 85 个组合帧，每帧保留原来的八个部件槽位 |
| `definitions/character_animations.json` | 动画帧序列及标称时间步长 |
| `definitions/player_poses.json` | 换武器、护甲及蹲姿的部件替换规则 |
| `definitions/world_tiles.json` | 主题、瓦片编号、偏移和组合瓦片关系 |
| `definitions/interface.json` | 界面图片名称与帮助图标编号 |
| `definitions/entity_parameters.json` | 原 d0 对象参数表 |
| `definitions/projectile_parameters.json` | 原 d1 弹体参数表 |
| `definitions/menu_layouts.json` | 原 dm 菜单参数表 |
| `definitions/visual_effects.json` | 发电机灯光、电弧颜色、阴影、震屏参数 |
| `levels/level_01.json` 至 `level_08.json` | 关卡基础瓦片、碰撞高度、玩家起点和对象生成记录 |
| `localization/en/` | interface.json 界面二维文本表、about.txt 与 help.txt 可读富文本 |
| `audio/ogg/`、`audio/midi/`、`audio/sounds.json` | 17 个运行时 OGG、17 个原始 MIDI 母版及声音编号映射 |
| `audio/synth/` | 按本次 MIDI 裁剪的 GeneralUser-GS.sf2 及 LICENSE.txt |
| `config/display.json` | 240×320 逻辑尺寸、默认 720×960 窗口和最近邻缩放 |

音频 `0.mid` 至 `15.mid` 改为 `sound_00.mid` 至 `sound_15.mid`，`mm.mid` 改为 `main_menu.mid`，字节不变。其余声音编号的完整用途尚未核实，不凭猜测命名成枪声或关卡音乐。

用户已确认当前 OGG 听感。每次构建都会按固定音色配置渲染 `sound_00.ogg` 至 `sound_15.ogg` 以及 `main_menu.ogg`，写入 `audio/ogg/`。`sounds.json` 的 `file` 保留 OGG 路径，`midi_source` 为游戏实际使用的原始 MIDI 路径；声音编号、单活动声音及打断/续播约定保留。WAV 对照和渲染报告放在 `output/audio/rendered/`。

## 加载约定

- 图集 `region` 为 `[x, y, width, height]`，以图集左上角为原点。按原分辨率裁切，使用最近邻采样。
- 部件 `offset` 为相对角色锚点的 `[x, y]`，已处理左右朝向与原版基线。按 `part_slots` 顺序绘制，值为 0 的槽位跳过，但不能删除槽位：护甲和武器替换依赖槽位编号。
- 瓦片保留主题 0–2、编号 0–166。102–104 的 `children` 指向同主题瓦片：第一个作为底图，不应用其对象偏移；第二个作为覆盖图，应用它的偏移。78–80 使用原版指定的精灵部件。空图像不是缺失文件。
- 关卡 `tiles` 和 `base_collision_heights` 使用左下原点，生成坐标 `spawn_cell`、`player_spawn_cell` 使用左上原点，以格为单位，每格 16 像素。玩家锚点的横向偏移需结合对象参数表。动态对象引起的地形与碰撞修改由游戏逻辑执行。
- 参数表保留原数组顺序；`shared_parameters_with` 保留原版共享行关系。尚未完全核实的参数列没有强行改名。
- 动画的 80ms 是标称步长；原版实际切帧受游戏状态控制。程序绘图参数也不能替代电弧随机折线、机关状态及倒计时等运行时代码。
- 位图字体沿用英文小写映射到大写的规则；文本中的图标编号对应 `interface.json`。

## 文本格式

`catalog.json` 使用格式版本 2；`language.page_format` 为 `rich-text-v1`，`about` 和 `help` 指向 UTF-8 文本。第一行是独立标题，第一个 LF 后才是正文。读取时不要 trim 或忽略空行，正文末尾 LF 也有布局意义。

```text
Help
<align:center>* GAME *

<align:left>Weapons:

<img:0> - Machine gun
<img:1> - Flamethrower
```

正文每个 LF 都结束当前行，包括空行；图片标签只放在行首，不再自行追加换行。`<img:N>` 对应 `definitions/interface.json` 的 `help_icons`，N 为 0–20；后续文字接在图片右侧，按原版相对图片垂直居中。图片行采用图片高度，普通行采用字高，行距与自动折行由运行时按原字形参数处理。

`<align:left>`、`<align:center>`、`<align:right>` 改变对齐状态，并持续到下一个对齐标签；初始左对齐。标签本身不占宽度、不换行，当前行在提交时应用对齐状态。字面 `&`、`<`、`>` 使用 `&amp;`、`&lt;`、`&gt;` 转义，解码后的字符不再作为标签解析。不支持制表符、HTML 或其他标签，文件结束即文本结束。

标题独立于滚动正文，对应原版第 0 行；这保留了标题后 CRLF 不增加正文高度的行为。原版图片命令隐式换行已转成显式 LF，因此图片前的空行不能擅自合并。工具会反向校验文本控制序列；C++ 加载及绘制已实现，参考环境的像素验证范围见 [验证记录](verification.md)。

## 工具产物

运行 `tools/run_pipeline.bat "游戏目录或 JAR、ZIP 路径"`，仅接受一个路径，固定输出 tools/output/。

runtime/ 保存可供游戏加载的素材；其余子目录保存原资源快照、拆图、关卡、文本、MIDI 事件、音频和预览。index.html 为浏览入口，pipeline.log 为处理日志；环境和依赖缓存也在 output 内。不生成哈希清单，不运行测试。再次运行覆盖旧生成结果并保留依赖缓存。

详细用法见 [工具说明](../tools/README.md)。
