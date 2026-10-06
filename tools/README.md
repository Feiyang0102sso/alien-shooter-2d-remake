# 素材预处理流水线

把原版游戏包转换成游戏使用的素材。

## 源素材标注

原包里的 `.png` 不全是图片。XOR 加密的密钥逐字节加 1。

| 文件 | 内容 |
| --- | --- |
| `hd` | 图片索引 |
| `pck0–2` | 瓦片图片包 |
| `cc` | 瓦片参数 |
| `up0–3/9`、`zp0–3/9` | 精灵图片包，右向 / 左向 |
| `cu` | 精灵参数 |
| `ca` | 组合帧（每帧 8 个部件） |
| `fnt`、`cf` | 字库图集、字形参数 |
| `ll` | 8 关地图与对象 |
| `obg` | 地图模块（4×4 瓦片） |
| `d0`、`d1`、`dm` | 对象、弹体、菜单参数表 |
| `tt`、`t0`、`t1` | 界面文字、关于页、帮助页（XOR 加密，俄文） |
| `lz` | 发行商 Logo（XOR 加密） |
| `or`、`or_f`、`lo`、`ldr`、`la`、`ico` | 界面图片 |
| `*.mid` | 17 首音乐音效 |

## 代码功效

| 脚本 | 作用 |
| --- | --- |
| `pipeline.py` | 入口：准备环境、解包、调度 |
| `extract_images.py` | 拆图片包，还原瓦片、精灵、组合帧、字体 |
| `extract_data.py` | 参数表、文字翻译、MIDI |
| `extract_maps.py` | 拼地图 |
| `extract_previews.py` | 预览图和浏览页 |
| `render_audio.py`、`trim_soundfont.py` | 渲染试听音频，裁剪音色库 |
| `publish_assets.py` | 打包成游戏素材 |

## 产出

输出到 `tools/output/`：

- `runtime/`：游戏素材，复制到项目 `assets/` 即可使用
- `index.html`：素材浏览页
- 其余目录：拆出的图片、地图、文字、音频，供核对

## rerun

```bat
tools\run_pipeline.bat "游戏目录或 JAR、ZIP 路径"
```

需要 Python 3.10+。首次运行会自动安装依赖（需联网）。重跑会覆盖旧结果。
