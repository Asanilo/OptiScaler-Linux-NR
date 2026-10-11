# C2 isolated NR helper — single frame + skipEvaluate verified

唯一合同 [C2-CONTRACT.md](C2-CONTRACT.md)，SHA `2a46defd341f87270f3d5afaf45cec4a24012f5e3bdd5fdd8617a40f0cd2c214`，root批准记录 `../integration/c2-contract-review.json`。采用 actual sourceColor 三读回；expected_keep/signed-zero扩展已撤回。旧 C1/G3 及稳定仓库只读。

新增源码：`wine_game_nr_helper.cpp`、`c2_game_transport.h`、`c2_game_control.h`、`c2_production_adapter.h`。后者从已PASS G3 adapter派生，仅1080尺寸、分配日志和10s实际fence等待；原生产CSMain SPIR-V/参数ABI/forwarder/model不改。runtime绝对路径及完整SHA/bytes见 `c2-runtime-pins.json`。

仅编译复现（不会运行PE/Wine/GPU）：

```
cd /home/arinp/Code/nrbridge-design/game-poc-20261011/opti
python3 build_c2.py
```

当前编译 exit0，日志 `/home/arinp/Code/nrbridge-design/game-poc-20261011/opti/out-c2/build-history/20261011T085919.998426Z/helper-build.log`（无warning/error），新产物 `/home/arinp/Code/nrbridge-design/game-poc-20261011/opti/out-c2/wine_game_nr_helper.exe`，PE x86-64、1079808B、SHA `80a95341599817f3a70eae3544ffa657dd392fdae1dc185c75f9249446fc25cd`。全部源/include/runtime身份、source snapshots及compiler命令见 `out-c2/build.json`；摘要状态见 `c2-results.json`。新增文档不是compile input。

fixture CLI 为 `wine_game_nr_helper.exe single|skipEvaluate RUN_DIR_WINDOWS SESSION`。启动只由root审查后的私有游戏native祖进程/direct64 loader串行执行；A不自行运行。全部filename/envelope/input/reset/jitter/预算见唯一合同，不另定义协议。source_frame_ms/当前及上一viewport和SR jitter必须真实取得；manual WhitePoint1不代表已测曝光。

single实际encode→Evaluate(result1/reset1)→resolve；skipEvaluate仍新model/feature/create和实际输入encode，Evaluate/resolve均跳过，poison输出INVALID不可用于merge。private raw16F/sourceColor32F/output32F本地readback总82944000B；source raw input_digest、output raw digest分别核传输，actualEvaluate/rawNaN覆盖/finite/alpha原bits核模型有效性；不同hash不单独证明推理。没有跨进程CPU像素回退。

原SR保持独立fallback。正常handoff/FD public Vulkan self-import消费/末次GPU使用/release/audit/NGXshutdown沿C1/G3通过路径；异常待真实私有helper退出/安全GPU边界，nr.release防异常析构误释放，绝不把异常进程退出称正常FD回收。native全游戏FD表可因cache改变，跟踪本slot payload；Wine同状态fullinventory/模型懒分配差异须实际解释。

## 本轮实际结果

首次真实游戏单帧 NR 目标已完成。Integration/root 串行执行两例；A 只读复核控制、日志及本地 GPU 读回，未复跑、未重新构建。编译记录仍是 `out-c2/build.json` 的 BUILD_ONLY PASS；运行 PASS 单列于 `c2-results.json`，其中 `history` 保留此前完整 BUILD_ONLY/NOT_RUN 记录。原始失败构建/诊断证据与 C1/G1/G2/G3 均原处保留。

| case | actual source | 模型调用/接纳 | 原字节 input / output FNV | 呈现 |
| --- | --- | --- | --- | --- |
| [single](../bbport/runs/c2-game-c2-single-20261011/) | 3895；SR首reset1588、选中SRreset0；新NRreset1 | Evaluate1/result1/resolve1，GPU fence完成，OUTPUT_VALID/model_valid=true | `27ff43e92dd78eb9` / `e94f29d8d7ca5a93` | merge7795→真实guestbuffer→present3896，selected_output1/swapchain1，f002 dump |
| [skipEvaluate](../bbport/runs/c2-game-c2-skip-20261011/) | 3810；SR首reset2050、选中SRreset0 | Evaluate0/result0/resolve0，INVALID/model_valid=false；同正常输出predicate拒绝，仅诊断读取哨兵 | `16c0689dd33ad50d` / `bdaa6cda0dada325` | 原SR fallback，merge7625→present3811，selected_output0/swapchain1，f002 dump |

正例 raw RGB 6,220,800 项全有限且覆盖 NaN poison，resolved RGBA 8,294,400 项全有限且非−1234，2,073,600 个 alpha 原位模式保持。负例 raw RGB 6,220,800 项全部保留 NaN、resolved RGBA 8,294,400 项全部保留−1234；即使传输摘要一致也绝不把 INVALID 输出用于 merge。独立 CPU 读回复算两个 case 的 FNV/有限性/哨兵，与两进程日志及控制记录一致。FNV 只证明共享传输一致；实际 Evaluate/result、GPU 完成、raw poison 覆盖和负例共同提供模型执行证据，输出与输入不同不单独证明推理。

两例均 actual 当前及前帧 viewport/SR jitter0、1920×1080/full valid rect、真实同 texel Depth130→100/MV83→103、MVScale1；未伪填 guides。实际 exposure unknown，manual WhitePoint1/Passthrough0/nrExposure1/nrPreExposure1 是本窄 profile 选择，未验真实曝光或非零 jitter 相位。两帧 source alpha 实际全0，位保持结论只覆盖这些输入；原 merge 继续保留 scene alpha。

两例四个 raw DMA_BUF FD80..83 均在 native imported ACK 后经公开 Vulkan self-import 消耗，临时 allocation 释放；实际 /proc inode/payload 在 raw退役与两端最终清单中均无残留。Wine83→83/native136→136 的完整同状态清单分别精确相等，native 仅将本次全表相等作为证据，不将游戏全局 FD 数量当普遍泄漏判据。末次 merge GPU 使用完成后 release/retired/audit；helper normal release、模型/core/NVAPI/DXGI teardown 全成功，实际 exit0/reap0。root controlled stop 后 ownPG/private prefix 均无残留。异常退出未混入正常 FD 回收结论。

实际模型为只读本地 PE `/home/arinp/Code/dlss5_linux/dll/runtime/nvngx_dlssnr.dll`，165830144B，SHA `6eb209e764f39872625debd6abaf45e2bb6322f6f270f781f70c059ae30b3927`；helper `80a953…`、native libbbgpu `206bf821…` 与两个 run manifest 均匹配。设备 UUID `8734fac9db30c279a340fe117ace6365`，driver UUID `e960882b01705057bf3ec686505f8776`。完整文件哈希、控制及日志定位见机器结果。

外层运行日志：[single](../runs/single-nr-20261011T092353.861428Z/game.log)、[skip](../runs/single-nr-20261011T092634.085116Z/game.log)。helper 全量统计/digest/83MB本地 dumps 为12039/12073ms，仅诊断开销；single 从 frame_start 至 fence/readback 完成的16ms区间包括其他 GPU 工作，绝非纯 NR 性能测量。无画质、性能改善或实时性结论。

**NOT_RUN**：C3 连续 NR/history、NR resize/restart/late/长期生命周期、非零 jitter/guide重采样、FG、external semaphore。此前 G2 shader 生命周期 PASS 不等于 NR 生命周期 PASS。本轮结束，不扩测试；全部 source/exe/build inputs/runtime pins 保持冻结。

