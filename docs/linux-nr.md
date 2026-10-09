# Linux NR integration: direct NR → SR

开发基准固定为 y4m `7b7220bbb4994a9c8ae60cfc75a44cb67995efb8`。
本轮移植参考 Sky `591fd305a2a8e1eca92eee8ccd584925ac2791ae` 的
`EvaluateBeforeUpscale`、typed/untyped 参数恢复和临时 placement comparison。
复用 y4m 已有的分离输入/输出 pass，不整文件替换 `DlssNr_Dx12.cpp`。

## 本轮范围

- D3D12 SuperSampling 在 SR 前运行 NR，将自己的输出临时用作 Color。
- typed/untyped Color 使用原来的参数类型恢复；失败的 SR evaluate 同样恢复。
- NR 创建、跳过或失败时，SR 使用原始 Color，不追加全分辨率 post-NR。
- pre-upscale 输出使用可写的 typed format，并检查 UAV 支持；记录自有纹理的资源状态。
- `PreSr` 作为 Sky 配置兼容别名；明确的 `PreUpscale` 优先，保存仍用原来的键。
- 临时对比 Saved / NR off / NR→SR / SR→NR，不写回 ini；NR 总开关仍优先。
- 保留 y4m 的 Vulkan 适配器识别、Proton 菜单队列、创建帧 root state envelope
  和输入/输出 barrier 修复。

这是一条直接的 NR→SR 链，不是 DualFeature 的「1:1 DLSS/RR → NR → enlarger」链。
RR、FG、原生 Vulkan 和 D3D11 bridge 不在本轮 pre-SR 验收范围。
临时 placement comparison 只对直接 D3D12 路径有效；开启 DualFeature 时使用保存的配置。

尚未移植 edit cache、重投影、抗闪烁、截图 benchmark 或自动采样。
上游的资源延迟回收和 descriptor ring 仍没有完整 GPU fence 证明，不能宣称长期稳定。
本轮也未修复同一帧多个 SR feature 共享全局 NR 状态的所有问题。

## 配置和比较

合并到原有 ini 的对应段：

```ini
[DlssNr]
Enabled=true
PreUpscale=true
DualFeature=false
Passes=1
WorkingScale=1.0

[FrameGen]
Enabled=false

[Menu]
ShortcutKey=0x79

[Log]
LogToFile=true
LogLevel=2
```

游戏内先选择 DLSS SR Quality 或 Balanced，关闭帧生成、动态分辨率和 RR。
F10 打开 overlay；`Live placement comparison` 可在同一场景切换三条路径。
该下拉框位于 `DLSS Neural Rendering` 内，紧接 `Enable Neural Rendering`
复选框与快捷键提示之后，在 Running 状态与 Passes 控件之前。
切换前后保持 SR 质量、WorkingScale、Passes 和色彩设置一致；等待创建和历史稳定后测量。
比较是临时状态，重启回到保存配置，选择 Saved placement 也恢复保存配置。

`LastGpuTime` 是上游计时器给出的 NR pass 读数，包含准备/模型/resolve。
它不是整帧时间，也不是对 4060L 的性能承诺。测量真正渲染 FPS 时保持 FG 关闭。
本轮没有把 CPU dispatch 次数伪装成渲染帧数。

## 构建和回归验证

本机参数测试使用真实生产头文件，模拟 NGX 的两个独立资源类型槽：

```sh
g++ -std=c++17 -Wall -Wextra -Werror -fsanitize=address,undefined -g \
    tests/nr_pre_upscale_test.cpp -o /tmp/nr-pre-test
ASAN_OPTIONS=detect_leaks=0 /tmp/nr-pre-test
```

本机沙箱的 ptrace 限制使 LeakSanitizer 不能运行；AddressSanitizer/UBSan 可运行。
测试覆盖 typed/untyped、失败 Get、空参数、创建/跳过帧、成功/失败/异常退出的恢复、
禁止 RR/FG 和 DualFeature 的直接 pre-SR、master toggle 与临时比较的优先级。
这些是参数契约测试，不替代 D3D12/GPU 或游戏验收。

Windows 编译使用现有 `Build (Fast)` workflow，指定 `linux-nr` ref，MSVC Release x64。
产物仅含 `OptiScaler.dll` 和 `nvngx.dll_dlssnr.dll`，须搭配模型和游戏的 DLSS runtime。
不调用会发布 nightly 的构建流程。

新增的独立测试/构建 workflow 保留于本地 `ci/linux-nr-build` 分支。
当前 GitHub OAuth 凭据没有 workflow scope，因此不包含在已推送的开发分支中。

## 本机游戏验收

首个目标是 Steam app 3280350，`DEATH STRANDING 2: ON THE BEACH` 的 `DS2.exe`。
本机发现 `nvngx_dlss.dll` 和 `sl.dlss.dll`；开发者也确认游戏提供 DLSS：
[Nixxes 功能说明](https://support.nixxes.com/hc/en-us/articles/49584441309971-What-are-the-PC-features-in-DEATH-STRANDING-2-ON-THE-BEACH)。

Detroit 的 `DetroitBecomeHuman.exe` 导入 `vulkan-1.dll`，检查到的文件/字符串未提供 NGX 入口。
它不能验证这次的 D3D12 pre-SR hook，后续应单独验证通用 Vulkan 注入。
开发者对 Vulkan 渲染器的说明：
[Quantic Dream](https://blog.dev.quanticdream.com/quantech-blog-3d-engine-optimisation-for-pc/)。

本机 NR runtime 来自用户提供的 `nvngx_dlssnr_310.8.SF-v2_community.zip`，
解压文件 165830144 bytes，SHA-256：
`6eb209e764f39872625debd6abaf45e2bb6322f6f270f781f70c059ae30b3927`。
模型不进入 git 或 GitHub 构建产物。

MSVC Release 构建 `26aea636` 已通过：
[构建记录](https://github.com/Asanilo/OptiScaler-Linux-NR/actions/runs/37940214870)。
已在用户指定的 DS2 目录安装测试代理、forwarder、模型和 ini，启动 GE-Proton11-7。
初始 NR 关闭，PreUpscale 选中；本机日志确认加载代理、DLSS 文件和 RTX 4060L。
随后日志确认 pre-NR 和 post-NR 实际 evaluate；用户于 2026-10-09 确认两种顺序画面均正常。
用户键盘没有 Insert，因此测试安装将菜单键改成 F10 (`0x79`)，并启用 Info 文件日志。
配置读取发生在启动时，换键后须正常退出并重新启动。

本次单层测试的周期性 GPU 计时样本：

| 顺序 | NR 分辨率 | 样本数 | NR 总耗时中位数 | 最小～最大 | 集成耗时中位数 |
| --- | --- | ---: | ---: | ---: | ---: |
| NR→SR | 1280×720 | 20 | 7.35 ms | 6.98～9.69 ms | 0.14 ms |
| SR→NR | 1920×1080 | 20 | 15.83 ms | 14.05～21.11 ms | 0.27 ms |

场景和设置期间有变化，包含切换后的样本；这是日志读数摘要，并非固定场景 FPS benchmark。
SR 前运行 NR 的处理像素更少，本次 NR pass 耗时中位数约低 54%。
不能将此比例当作整帧性能提升，也未测量 1% low 或完成长时间稳定性验收。
本机原始日志快照、模式切换记录与测量 JSON 保存在工作区 `testlogs/ds2-f10`，不上传游戏日志。
日常建议从直接 NR→SR、Passes=1 开始。

安装清单和原文件备份位于工作区 `backups/ds2-26aea636`。
回滚工具默认只预览，必须关闭游戏后才使用 `--apply`；遇到安装后变化的文件会保留文件并停止：

```sh
python3 tools/rollback_test_install.py ../backups/ds2-26aea636
python3 tools/rollback_test_install.py ../backups/ds2-26aea636 --apply
```

本次通过临时环境变量 `WINEDLLOVERRIDES=dxgi=n,b` 启动，没有持久修改 Steam 启动项。
以后通过 Steam 启动时需要在游戏属性中设置该 override。

已确认 NR off 启动、pre-NR 首次创建/后续 evaluate、post-NR 和临时比较切换。
仍需验证 Alt-Tab、改变 SR 质量或窗口尺寸，以及连续游戏帧时间/显存。
验收应以初始化和实际 evaluate 日志、画面及 GPU 表现为依据，不能仅看 DLL 加载成功。
