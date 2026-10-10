# Linux NR integration: direct NR → SR

2026-10-10 当前状态：`fee544fe` 已修正 DS2 平面深度与 post-SR 运动抖动，当前场景短测通过；安装构建 `eee98821` 保留修复并加入原生 FG 诊断。早期闪烁失败仍作为历史记录保留。完整当前状态见 [工作区说明](../../docs/game.md)，交接见 [handoff](../../docs/handoff-20261010.md)。开发分支尚未合并到远程默认主线。

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

后续稳定性阶段已实现真实队列 fence 回收、每个设备/SR 句柄独立状态，以及默认关闭的每帧 edit 重投影稳定器。
本轮已补上 [Sky 隔帧缓存和完整防闪](sky-cache.md)，默认关闭；未接入截图 benchmark 或自动游戏性能验收。
独立 Proton GPU 测试通过，DS2 当前场景静止、移动与 Sky 间隔1/2短测通过；长期稳定性与受控性能门槛仍待验收，详见 [next-stage.md](next-stage.md)。

## 配置和比较

合并到原有 ini 的对应段：

```ini
[DlssNr]
Enabled=true
PreUpscale=true
DualFeature=false
Passes=1
WorkingScale=1.0
StabilizationEnabled=false
StabilizationStep=0.5
StabilizationDespeckle=false

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

`LastGpuTime` 现使用实际队列完成的计时读数，包含准备/模型/resolve/可选稳定器。
稳定器另有独立的最后一个已完成 GPU 样本；这些读数不等于受控性能统计。
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
随后日志确认 pre-NR 和 post-NR 实际 evaluate。用户最初报告两种顺序正常，
但继续观察后确认 SR→NR 保持模式仍持续闪烁，而 NR→SR 未观察到闪烁。
当时 SR→NR 画面验收失败；首次反馈只代表初步观察。随后 `fee544fe` 修正两处输入问题，用户确认当前场景稳定，证据见工作区 `testlogs/nr-flicker-fee544fe/acceptance.json`，不追溯改写旧构建失败记录。
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

## 验收状态与反证检查

直接 pre-SR、Sky 缓存/防闪与输入修复已完成，DS2 当前场景短测通过；完整 Linux 长期稳定性与跨游戏验收未完成。
下一阶段的固定步骤、准备工具与剩余验收见 [next-stage.md](next-stage.md)。
本机唯一游戏实测为 DS2 / RTX 4060 Laptop / NVIDIA 615.71.09 / GE-Proton11-7 / Wayland；
不能推广为所有 Linux、Proton、游戏或 GPU 已兼容。

| 检查 | 当前结论 | 证据边界 |
| --- | --- | --- |
| MSVC Release | 通过 | 编译和链接，不证明运行正确 |
| typed/untyped 参数恢复 | 通过 | 生产 helper 与独立类型槽 mock，不是实际 NGX 实现 |
| 创建/跳过帧顺序 | 生产编排函数单测通过 | 五帧测试调用 hook 复用的 EvaluateWithNr；未执行实际 vendor NGX 或 GPU |
| DS2 NR→SR | `fee544fe` 实际执行，用户确认当前场景正常 | 未测长时间、动态分辨率和其他游戏 |
| DS2 SR→NR | 旧构建持续闪烁；`fee544fe`当前场景静止/移动通过 | 24帧输入验证通过；其他场景与长期未测 |
| 闪烁输入缺陷 | 平面深度复制丢失深度、post-SR运动保留抖动已修正 | 两处同时安装，独立贡献未量化；非所有原因已排除 |
| GPU 对象回收/descriptor 复用 | 已实现并通过真实 Proton GPU 检查 | 生产 Reset/Release/Execute/Signal 与负对照；实际 NGX/游戏长期验收待测 |
| edit 重投影/稳定器 | 最小每帧版仍保留；用户复测无效 | 旧版 post-SR 画面失败记录保留 |
| Sky 隔帧缓存/完整防闪 | 已移植；默认关闭；DS2间隔1/2短测正常 | 生产wrapper/GPU负对照通过；长期及受控FPS待验收 |

对 portable helper 做了三个负向控制：临时删除恢复、错误地改用 untyped setter、
绕过 master toggle，现有测试均失败；原始实现通过。修改仅发生在临时目录，
没有将错误实现写入生产仓库。这说明单测能够发现这些指定缺陷，不能证明 GPU 同步或闪烁已解决。

未修改基准 `7b7220bb` 已完成独立构建并在 post-SR 复现闪烁：
[基准构建记录](https://github.com/Asanilo/OptiScaler-Linux-NR/actions/runs/37946226862)。
归因对照须使用同一 SF-v2 模型、驱动、Proton、存档、镜头、SR 质量及曝光配置，
Passes=1、WorkingScale=1、Detail strength=1、FG/动态分辨率关闭。
每种模式使用新进程或明确记录历史预热，不能把之前 NR→SR 留下的 SR 历史当成无 NR 基线。
分别记录持续静止、缓慢运动和切换阶段；日志没有错误不等于画面没有闪烁。
本次基准已复现相同症状，说明并非移植独有；仍不能区分上游集成、共同运行时或模型导致的具体原因。
基准复现本身不能将 SR→NR 标为通过；后续输入修复仅凭独立24帧验证和用户静止/移动短测授予当前场景验收，不能推广到所有场景。

安装清单和原文件备份位于工作区 `backups/ds2-26aea636`。
以下是早期NR-only恢复入口。当前安装叠加FG和FSR测试，先从工作区运行 `build/install_fsr41_test.py --rollback`，然后处理FG层；不要直接跳过后续层调用早期NR恢复。所有工具默认只预览，必须关闭游戏后才使用 `--apply`；遇到安装后变化的文件会保留文件并停止：

```sh
python3 tools/rollback_test_install.py ../backups/ds2-26aea636
python3 tools/rollback_test_install.py ../backups/ds2-26aea636 --apply
```

使用普通 Steam 启动项；不再要求 Python 包装 Steam 命令。菜单键F10；后续步骤见 [next-stage.md](next-stage.md)。旧稳定器A/B已完成且报告无效，不能当成仍需重复的起点。

已确认 NR off 启动、pre-NR 首次创建/后续 evaluate、post-NR 和临时比较切换。
仍需验证 Alt-Tab、改变 SR 质量或窗口尺寸，以及连续游戏帧时间/显存。
验收应以初始化和实际 evaluate 日志、画面及 GPU 表现为依据，不能仅看 DLL 加载成功。
