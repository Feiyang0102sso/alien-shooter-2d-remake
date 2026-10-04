# 本游戏素材预处理

在项目根目录运行，或把游戏包拖到 run_pipeline.bat 上：

```bat
tools\run_pipeline.bat "游戏目录或 JAR、ZIP 路径"
```

只接受一个包路径。输出固定在脚本旁的 tools/output/，不提供其他参数。

- runtime/：供游戏加载的素材、原 MIDI、裁剪音色和许可。
- source/：原包资源快照。
- 其他子目录：拆图、关卡、文字、MIDI 事件、音频和预览。
- index.html：素材浏览页；pipeline.log：处理日志。
- .venv/、.cache/：自动准备的 Python 环境、音频工具和完整音色库。

电脑需要 Python 3.10 或更新版本。首次运行自动安装 Python 依赖并下载音频依赖，需要联网；后续复用 output 内的缓存，不读取项目 assets、src/3rdparty 或旧的项目虚拟环境。

再次运行会覆盖旧生成结果，保留环境和依赖缓存。原包须位于 output 外，且不能包含 output。只处理当前 Alien Shooter 的资源格式。

不运行测试，不计算 SHA256，不生成哈希清单。
