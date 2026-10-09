# Sky 隔帧 NR 与防闪移植

参考固定为 Sky `591fd305a2a8e1eca92eee8ccd584925ac2791ae`，集成基点为
`502c41409797f460a9828e0a6c66dcc61f311eac`。本轮补上 temporal edit cache、运动重投影、
刷新过渡、亮度变化限幅、去孤立噪点、细节时域平滑和区域亮度稳定。
上次最小稳定器在 DS2 的 SR→NR 下未能消除持续闪烁；这条失败记录保留。
新移植的游戏画面、性能与长期稳定性均待实测，不能用合成 GPU 检查替代。

## 使用

F10 → DLSS Neural Rendering → **Sky anti-flicker + temporal cache (experimental)**。
原来的 **Edit stabilization (experimental)** 在 Sky 开启时停用，避免叠加两个滤波器。

- **Every-frame NR + anti-flicker**：每帧运行 NR，便于单独对照防闪。
- **Alternate-frame NR + anti-flicker**：每两个输入帧运行一次 NR，另一个帧重投影缓存修改。
- **NR refresh interval**：范围 1–16；历史失效会提前刷新。
- **Adapt interval to revealed pixels**：可选的 Sky 三档运动调度；菜单显示实际间隔和运行次数。

缓存保存 RGB 的 NR 修改量，不保存整张旧游戏画面。每个缓存帧仍使用当前游戏颜色、深度、
运动矢量和 alpha。NR→SR 的私有输出先复制当前 Color，防止使用上帧的输出。
隔帧优化不改变 DLSS 插帧倍率。

库默认 `CacheEnabled=false`。默认间隔改为 2、自适应关闭，使首次启用的调度容易对照；
Sky 原默认是间隔 3、自适应开启。各防闪权重沿用 Sky。Detail、Intensity、LowGain、HighGain
默认均为 1，不通过降低效果强度让测试通过。

```ini
[DlssNr]
CacheEnabled=true
CacheInterval=2
CacheAdaptive=false
CacheModelHistory=1
CacheStabilize=0.5
CacheDespeckle=true
CacheCrossfade=true
CacheTemporal=0.5
CacheLowTemporal=0.95
StabilizationEnabled=false
```

`CacheModelHistory=1` 累积被跳过帧的运动矢量，再交给 NR 模型；累积失败时重置模型。
`2` 每次刷新重置模型；`0` 使用游戏单帧矢量，是高级对照选项。
NR→SR 输入带 jitter，沿用 Sky 禁用细节 temporal 和 crossfade，SR 负责细节累积；
区域亮度稳定仍开启。缺失或非有限 jitter 时刷新历史，不能继续复用旧的 jittered 修改。

当前接入直接 D3D12 NR→SR / SR→NR。DualFeature、非直接 seam、proxy、主 DebugView、
主 Compare 和截图期间暂停缓存。库内每个设备/SR 句柄拥有自己的缓存；游戏启动仍由用户操作。

## GPU 耗时显示

D3D12 的 Running 读数显示最近 120 个 NR 输入帧中已完成 GPU 查询的算术平均值，
同时显示 Refresh（完整模型刷新）、Cached（复用缓存）各自平均值及采样覆盖数。
权重来自实际采到的帧，不按配置间隔推算，不把最后一个缓存帧的耗时当成整体开销。
查询未完成、槽耗尽或 recording 被丢弃时不编造读数；若窗口内存在两类帧而只采到其中一类，
整体均值暂不显示。缺失样本可能影响代表性，因此覆盖率必须与读数一起查看。

开关、placement、历史重建、渲染缓存键或刷新间隔变化会重置统计。
模型与总耗时按同一个输入帧编号配对，避免切换后出现负数“ours”开销。
NR 使用有实际 submission fence 的 16 槽查询环，逐一消费全部完成的查询；
其它 upscaler 保留原有 3 槽路径。这里测的是 NR GPU 区间开销，不是输入延迟或游戏整体帧时间。

DS2 `c39663b8` 首轮用户观察：隔帧开启约 54→64 FPS（约 +18.5%）；截图中
关闭显示 8.69 ms，开启显示 0.67 ms（最后一个缓存帧）。原始日志确认 interval 2 与
近 1:1 的刷新/缓存调用；该观察没有受控帧时间测量，不能作为完整性能验收，
也没有证明 SR→NR 的持续闪烁已解决。

计时回归覆盖实际 RTX/Proton 时间戳、未读取的已完成查询、两类帧的身份、未提交/丢弃
recording、查询环耗尽后的恢复及非 NR 旧路径。CPU 检查使用独立数值覆盖实际权重、
乱序配对、缺失类别、重复样本、切换后迟到样本、窗口淘汰与非法数值，允许合法零耗时。

## 从 Sky 修正的兼容性问题

- 用现有真实 recording/submission fence 管理纹理、CB、descriptor heap、pipeline 和读回。
  不沿用按 32 帧释放资源、过 3 帧读取统计、过 8 帧写截图的假定。
- 每帧先保留足够的 descriptor/CB 槽，槽未完成或仍可重放时不得覆写。预算不足暂停缓存，
  失败回到完整 NR；统计读回全忙时只丢弃遥测，不覆写等待 GPU 的读回。
- 成功记录输出后才增加刷新/缓存计数；丢弃 recording、游戏 reset、尺寸/格式、placement、
  guide 编码、模型/合成/缓存参数变化会使历史失效。暂停后恢复会重置 NR 模型历史。
- Sky 原 shader 的 Apply 分支仅在 `Temporal>0` 时输出中间 edit，导致 pre-SR 将细节
  temporal 设为 0 后，区域 `LowTemporal` 路径不能独立工作。本轮将 wrapper 与 shader
  的判断统一为 `Temporal>0 || LowTemporal>0`，重新编译生产 DXBC。
- 当前 y4m resolve 使用 CPU 持有的 white point；缓存使用相同数值，不擅自改接实时 exposure SRV。
- 恢复本帧实际读取的 guide clone 状态，避免 guide 从 typeless 切回 typed 时错误转换旧 clone。

原始文件与 shader 编译来源见 [sky-cache-provenance.json](sky-cache-provenance.json)。
模型 DLL、游戏、日志、工具链均不上传仓库。

## 验证边界

CPU ASan/UBSan 调度测试覆盖间隔 1/2/3/16、失败刷新重试、reset、测量刷新、自适应迟滞和非有限参数。
独立 GE-Proton11-7 前缀在 RTX 4060L 上执行生产 cache wrapper、生产 DXBC、Shader_Dx12 helpers、
NrGpuLifetime 及真实 Execute/Signal adapter。配置/日志/应用状态由 fixture 提供；NR 输入是受控数据。
实际游戏导出 hook、vendor NGX 和最终画面不在该 fixture 的验证范围。

数值检查覆盖新画面/alpha、pre-SR 独立区域平滑、完整 post-SR 防闪、reset、jitter 位移、
隔帧模型 MV 累积、深度/颜色/越界校验、reversed-Z 重投影、非 8 整除尺寸、resize、
未提交 recording 的槽耗尽/读回保护、
完成后恢复、未提交命令被丢弃后的历史刷新、GPU 执行前销毁缓存和真实队列 drain。
另有三组故意禁用新画面复制、区域平滑或 jitter 修正的对照，均必须报失败。
原有真实延迟/跨队列/replay lifetime fixture、两个提前覆写/释放负对照及旧稳定器回归检查保留。

重建与复测（这是独立测试命令，不是 Steam 启动项）：

```sh
python3 tools/build_nr_gpu_tests.py \
  --toolchain ../build/toolchain/llvm-mingw-20261006-ucrt-ubuntu-22.04-x86_64 \
  --detours ../build/toolchain/detours-v4.0.1-original \
  --output ../build/nr-sky-cache-final

python3 tools/run_nr_cache_gpu_tests.py \
  --build ../build/nr-sky-cache-final \
  --output ../testlogs/nr-sky-cache-final \
  --proton /home/arinp/.local/share/Steam/compatibilitytools.d/GE-Proton11-7-x86_64/proton \
  --prefix ../build/nr-lifetime-prefix \
  --steam /home/arinp/.local/share/Steam
```

结果必须同时具备 fixture 自写报告、预期退出码、GPU 标识与具体检查结果；启动器成功不是通过。
测试保存源码、二进制和报告 hash，输出目录必须新建，保留之前的失败。

## 游戏验收

使用同一存档和预先选定的观察区域，保持 Passes=1、WorkingScale=1、Detail/Intensity=1，
先在当前 placement 对照 Sky off、间隔 1、间隔 2，再对照另一个 placement。
记录固定镜头闪烁、缓慢转动的拖影，以及菜单实际刷新/缓存计数。
隔帧 2 时每帧 NR 调用数理论上减半；这不是 FPS 翻倍承诺，缓存和 SR 自身仍有成本。
实际性能须做三次相同场景测量，区分 NR GPU cost、真实输入帧/Present 和生成帧。
原有长时间、切换/Alt-Tab/分辨率、内存与性能门槛仍保留；尚未通过的门槛不标为已验收。
