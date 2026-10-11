# C2 real-game single-frame NR contract v1

2026-10-11。唯一实验合同，依据 `../plan.md` C2 补充和 `../integration/c2-acceptance.json`。状态：REVIEW_REQUIRED；仅合同准备，source/build/GPU NOT_RUN。Integration 审核后才构建，实际游戏/GPU仅 Integration 串行执行。本合同不冻结 NRBridge ABI，不实现 C3。

## 1. 隔离、CLI 与 profile

新增 `opti/wine_game_nr_helper.cpp`、`c2_production_adapter.h`、`c2_game_transport.h`、`c2_game_control.h`、`build_c2.py`、`c2-runtime-pins.json`；产物/历史仅 `opti/out-c2/`。新 CLI：

```
wine_game_nr_helper.exe single|skipEvaluate RUN_DIR_WINDOWS SESSION
```

新 case 私有 RUN/PREFIX/profile/seed，session 为 1..63 个 ASCII 字母/数字/连字符。mode 只能 `single` 或 `skipEvaluate`，均 generation=0、frame=0、max_frames=1、resource_set=SESSION-g0；终端 release/retire/audit frame=1。新 helper 不覆盖 `wine_game_helper.exe`，C1/G1/G2/G3 source、exe、build inputs/合同及既有证据只读保留；B 独占新增 native C2 文件，A 不编辑 bbport。

唯一 profile_id=`hdr-run-dlaa-1to1-j0-manual-wp1-v1`。fresh 私有游戏进程从启动即实际 `BB_JITTER=0`，实际 `!Scaled()` / HDR `Run()` / DLSS DLAA、render=output=1920x1080。当前和上一帧 viewport 与 SR jitter 均须真实为 0；不得仅填写 metadata 0。四资源 valid rect 均 origin=(0,0)、extent=(1920,1080)。选择实际可见 Clinic 3D 帧，SR成功/ExtraSharpen后、原 RGB merge 前；原 SR image 永不覆盖。

## 2. 真实输入、历史与模型参数

Color 为本帧 SR RGBA16F97 转 shared RGBA32F109，保持 HDR linear 输入语义；不是显示后 LDR，也不恢复未知曝光。真实硬件 Depth130 的 depth aspect 同 texel 转 R32F100，保 forward device-Z，不 linearize/invert；真实 MV83 同 texel转 RG32F103，保 current→previous / previous-current render-pixel units、不 resample、不额外减 jitter；MVScale=(1,1)。不得填合成 .5/0 guides。

SR feature 首建/reset 历史和从启动零 jitter 的实际日志保存；选中帧 source_reset 是真实 SR 值，可为 false，不强制重置成熟 SR。NR 独立新 feature，没有前帧 NR 历史，首次/唯一 Evaluate reset=1。本关仅以零 jitter/full valid rect/1:1 消除相位变换；不宣称非零 jitter guides 已验证。

明确 manual profile：WhitePoint=1、Passthrough=0、nr_exposure=1、nr_pre_exposure=1；真实游戏 exposure/preExposure 未取得，exposure_source="unknown"、exposure_valid=false。两个 1 是本实验选择的模型参数，不是测得曝光。NR scene_linear=1、DepthInverted=0、nr_jitter=(0,0)、WorkingScale=1、passes=1、preset=0、intensity=1、style=0、localStructure/localTone/skinStructure=1、autoMask=1、uiCorrection=0；encode/resolve TransferStrength=ColourStrength=1、MaxRatio=2、Transfer=1、cache/debug/compare=0。参数均实际记录，保 G3 调用形式/安全参数 ABI；不运行 SR、DualFeature、enlarger、FG 或重新实现核心算法。

## 3. 四共享资源及私有 NR 状态

| role | shared format | extent | usage | ownership |
| --- | --- | --- | --- | --- |
| color | 109 RGBA32F | 1920x1080 | 15 TRANSFER_SRC/DST + SAMPLED + STORAGE | native 写实际 SR Color，Wine读 |
| depth | 100 R32F | 1920x1080 | 15 | native 写真实 depth，Wine读 |
| mv | 103 RG32F | 1920x1080 | 15 | native 写真实 MV，Wine读 |
| output | 109 RGBA32F | 1920x1080 | 15 | Wine独立写，native读 |

四独立 Wine-created DMA_BUF512、EXCLUSIVE、optimal tiling0、image_type1、samples1、mips/layers1、flags0、dedicated=true、offset0。每项必须记录真实 fd、allocation_size、req_memory_type_bits、alignment、memory_type_index、external query 与 device/driver UUID；不能靠格式 query 宣称模型执行。metadata 保 C1 `handle_type=512` 顶层字段；resource 中 `sharing_mode=0`、`runtime_layout=1`，actual VkBool32 req 字段沿 C1 用 0/1，不改成 bool。

所有外部 handoff 为 GENERAL + EXTERNAL，仅 Wine 首初始化允许 UNDEFINED→GENERAL。native import/acquire已有图像绝不从 UNDEFINED 丢弃内容。共享包装为 borrowed，不入私有 owned-image vector、不重复释放 view。

私有 proxy109、raw97 RGBA16F、keep109、dummy109，全部同一 1920x1080。生产 constants Width/Height/GuideWidth/GuideHeight、NGX wrappers/create/evaluate/subrect、VkImageCreateInfo/copyExtent/readback、实际 dispatch 全统一；8x8 groups=(240,135,1)，不得残留 G3 640/360/80x45。生产 SPIR-V CSMain 原样复用。

单 slot 本地 readback：raw16F=16588800B，sourceColor32F=33177600B，output32F=33177600B，合计82944000B，不复制 G3 四帧数组。加两组常量 buffer 与实际 private/device allocations；逐项记录 req size 与本地内存预算。无跨进程 CPU pixel 文件/传输回退。

## 4. 启动与有界预算

启动前原子 `game-config.json`，字段精确为：session(string)、generation(0)、mode(string)、profile_id(string)、width/height/render_width/render_height(1920/1080)、max_frames(1)、cold_init_timeout_ms(120000)、control_timeout_ms(60000)、gpu_timeout_ms(10000)、frame_completion_timeout_ms(30000)、manual_white_point(1 number)、passthrough(false)、device_uuid/driver_uuid(真实32位lowerhex)。配置读取10s。

保实际 direct64 preloader→绝对 Unix Wine ELF、私有 prefix 和 C1实际PE/maps/argv0/run/session/startticks/祖链验证；`pid-request.json` 仍 session/exe/win_pid，native.pid 文本为核实的真实 exporter UnixPID，绝不取 PE proc/self/stat wineserver PID。identity gate 60s；模型 cold-ready 预算120s独立计时，native bootstrap不阻塞每帧渲染线程。观察所有 cold stage 起止时间；同步 core/model API 无可安全取消接口时由 native超时quarantine + root私有helper watchdog限制，不能伪造回调取消。

模型 core/init/caps/安全 setter/create、create GPU fence、private shader/images/readbacks初始化先完成，才发布 baseline；baseline保模型/private resources 到退休同状态审计后。`baseline` 额外 model_init=true、model_feature_created=true、mode 与预算。skipEvaluate 也创建同一模型/feature及private状态，只跳 Evaluate/resolve，才能作相同输入/模型依赖对照。

每次实际 GPU fence最多10s；INPUT_READY 后 native completion最多30s，包含1080 encode/Evaluate/resolve/本地raw/sourceColor/output三段readback+CPU stats/digest。setup/ack/release control等待每项60s；原游戏全程 root600s outer watchdog不会扩张这些阶段期限。冷初始化与模型1080速度未实测，以上是有限诊断预算，不是实时性能承诺。超过任一阶段只quarantine/fallback，不自动延长。

## 5. envelope 与实际文件名

所有 envelope 严格 JSON 类型、无重复 top-level key、<=64KiB、atomic rename。每项基础字段：

session/string、generation/u64、frame/u64、resource_set/string、source_game_frame/u64、profile_id/string、width/height/render_width/render_height/u64、mode/string、op/string、status/string、completed_GPU/bool、output_valid/bool、model_valid/bool。

setup source_game_frame=0，frame=0；input/complete/consumed source_game_frame=真实本帧SF>0；末尾 frame=1/source=同SF。native控制 output_valid/model_valid=false；completed_GPU=true 必须已完成其实际相关GPU阶段，不能凭文件存在。

| 实际 filename | sender | op / status | frame/source |
| --- | --- | --- | --- |
| baseline-SESSION-g0.json | Wine | baseline / BASELINE | 0/0 |
| native-baseline-SESSION-g0.json | native | baseline_audit / BASELINE_AUDITED | 0/0 |
| meta-g0.json | Wine | metadata / READY | 0/0 |
| imported-SESSION-g0.json | native | imported / IMPORTED | 0/0 |
| fd-retired-SESSION-g0.json | Wine | fd_retired / FD_RETIRED | 0/0 |
| input-SESSION-g0-f0.json | native | input / INPUT_READY | 0/SF |
| complete-SESSION-g0-f0.json | Wine | complete / OUTPUT_VALID(single) 或 INVALID(skipEvaluate) | 0/SF |
| consumed-SESSION-g0-f0.json | native | consumed / CONSUMED | 0/SF |
| native-released-SESSION-g0.json | native | release / NATIVE_RELEASED | 1/SF |
| retired-SESSION-g0.json | Wine | retired / RETIRED | 1/SF |
| native-audited-SESSION-g0.json | native | audit / FD_AUDITED | 1/SF |

固定名仍 game-config.json、pid-request.json、native.pid、native.abort、wine-result.json、wine-helper.log/wine-helper-stderr.log、native-helper-exit.json。沿 C1真实 `-g0` 勘误，不沿旧合同缺g0写法。

metadata 基础 envelope + completed_GPU=true/两个valid=false，另有 linux_pid、device_uuid、driver_uuid、handle_type512、resources(4)、max_frames1、四期限、manual_white_point1、passthroughfalse。四 resources 数值/FD/createinfo/UUID/req fields严格匹配；每 role 唯一/FD distinct，native actual dup fstat payload对应原 exporter，allocate import成功消耗 dup，失败仍caller-owned。

## 6. INPUT_READY 额外字段

native只在实际 !Scaled/HDR/1:1/四有效rect、当前与上一 jitter0、SR成功及真实共享 GPU 写入/release/当前tick提交有界完成后发布。Flush 后获取新 command buffer，保 C1已修 barriers，不跨Flush复用旧cmd。

必填：hdr=true、exposure_source="unknown"、exposure_valid=false、source_reset(bool)、source_frame_ms(finite>0)、viewport_jitter_x/y、previous_viewport_jitter_x/y、sr_jitter_x/y、previous_sr_jitter_x/y（八个finite number实际均0）；valid_rect={x:0,y:0,width:1920,height:1080}，相同rect适用四图并有actual descriptor证据；depth_inverted=false、mv_scale_x/y=1、nr_reset=1、nr_jitter_x/y=0、manual_white_point=1、passthrough=false。

SR历史附 sr_initial_reset_observed=true、sr_initial_source_game_frame/u64、sr_history_jitter_zero_from_start=true，须由日志中真实feature首次create/reset及current/previous jitter支撑；helper仅核profile条件并echo，不能伪造game历史。若任何actual值未取得/不匹配，C2 profile gate BLOCKED、原SR继续，不填0凑通过。

## 7. 单帧生产路径与输出有效性

exact INPUT tuple → acquire四图 → private raw所有channel poison NaN、shared Output四channel poison -1234 → 生产 encode实际当前shared Color到proxy+keep → single调用实际 forwarder Evaluate一次，reset1/实际资源及尺寸，result必须1 → 生产 resolve一次写 shared Output。skipEvaluate仍encode/poison，但 Evaluate=0、resolve=0。随后 local raw/sourceColor/output GPU readback，恢复全部 GENERAL/releaseEXTERNAL，真实 submit/fence完成，才检查/发 complete。跨进程只发控制/digest/stats，无CPU pixels。

当前输入不与G3合成gradient/.75比：native记录本帧共享Color原始RGBA32F digest，INPUT附 input_digest_algorithm="fnv1a64_rgba32f_bytes"、input_digest_bytes=33177600、input_digest/hex16；Wine对实际共享sourceColor本地GPU readback核同digest并记录 current_input_digest_match=true。生产encode原样保持 max(source.rgb,0) 写private keep（原shader precompile/dlssnr.hlsl:525-530），因此不把keep的RGB摘要强求等于未clamp原Color。单slot第三段readback用sourceColor替代旧G3 keep readback，不额外增加三段总预算。这是当前输入/传输证据，不单独证明Evaluate。

single complete 的基础 status=OUTPUT_VALID/output_valid=true/model_valid=true/completed_GPU=true，额外精确键：evaluate_calls=1、evaluate_result=1、resolve_calls=1、reset=1、raw_rgb_finite=raw_rgb_written=raw_rgb_total=6220800；resolved_rgba_finite=resolved_rgba_written=resolved_rgba_total=8294400；source_alpha_finite=source_alpha_preserved=source_alpha_total=2073600，当前原始sourceColor alpha与resolved alpha逐像素逐位比较，不固定.75/1。raw RGB NaN→finite覆盖证明模型实际写入；raw alpha不作NR有效性门槛。真实Color alpha有限，允许实际0/任意finite原值；原merge继续保scene alpha，不把raw模型alpha替代游戏alpha。

single严格输出有效性还要求 actual core/model/create/Evaluate success、无GPUpending、未expired/aborted、raw RGB全部finite+written、resolved RGBA全部finite/非sentinel、alpha保真、current input match；仅Evaluate返回/输出hash改变/feature创建不能PASS。模型可能不编辑输入，不要求output与input不同或bitexact。

skipEvaluate complete status=INVALID/output_valid=false/model_valid=false/completed_GPU=true、evaluate_calls=evaluate_result=resolve_calls=0、reset=1（仅说明计划参数未实际Evaluate）、raw_rgb_finite=raw_rgb_written=0、raw_rgb_total=6220800、resolved_rgba_finite=8294400/resolved_rgba_written=0/resolved_rgba_total=8294400、source_alpha_preserved=0、source_alpha_total=2073600；sharedOutput全部-1234。native先使用与single同一normal acceptance predicate，必须拒绝为可用输出；随后只作GPU diagnostic sample/readback验证8294400 sentinel、digestmatch，原SR merge/present继续。不能将负例INVALID混为helper error，不调用resolve伪造输出。consumed invalid表示资源交接，不是模型有效。

## 8. 输出 digest 与 native merge

两模式 complete 都含 output_digest_algorithm="fnv1a64_rgba32f_bytes"、output_digest_bytes=33177600、output_digest/16lowerhex、sentinel=-1234、current_input_digest_match=true，echo第6节actual参数。Wine vkCmdCopyImageToBuffer紧密row-majorRGBA32F小端原字节；FNV basis0xcbf29ce484222325、prime0x100000001b3、uint64 wrap，逐byte h=(h XOR b)*prime。native同sharedOutput独立localGPU copy/readback同字节核digest，另保实际sample/finite/sentinel检查。非加密摘要用于传输一致性，不是质量/模型有效性证明。

single仅active未过期exacttuple+全部模型有效证据+digest一致时把sharedOutput109转换为独立native return97用于原merge。原SR97不可写/别名复用，NR结果不与SR做bitexact要求；conversion需finite检查。真实guest-buffer/merge/prepare/ready/present wait/screenshot/swapchain证据沿C1，scheduler数值仅同scheduler比较，不夸大next-present关联为完整因果证明。skip没有local NR返回图选中，实际记录原SRfallback/present。

## 9. FD退役、正常释放与异常

baseline在model/feature/private/readback/pipeline完成后、shared allocation前真实/proc取样。native全部4dup/import/bind成功且无pending才 imported；Wine此后仅用已PASS公开Vulkan self-import消费originalrawFD，free临时alloc/image，fd-retired含 closed_fd[{role,fd}]四项，native核实际/proc原payload消失。失败FD仍caller-owned，不能PECRT_close/私有D3DKMT/退出冒充常驻回收。

single native完成Output读回/localcopy/EXTERNAL release真实fence后 consumed；独立return图到原merge最后GPU使用完成才退役nativeimports并 release frame1。skip diagnostic和EXTERNAL release fenced后 consumed，native确认imports末次使用已完成，释放并 release frame1。Wine收到release且无pending才free四shared，retired；native审真实/proc FD/payload及两端shared所有权，audit后才free私有NR/feature/model/core/DXGI/NVAPI/device并实际exit0。

保持G3正常shutdown次序：feature release → private production资源free → 已验证model Shutdown1 → core DestroyParameters → 公开无参数core Shutdown → FreeLibrary fw/core → NVAPI → DXGI → Vulkan device/instance/loader。仅已pin公开接口，绝不再用此前失败私有coreShutdown1 ABI。

Wine same-state baseline/retired full inventory比较，model懒初始化FD差异具体记录解释，不自动忽略/判漏；native游戏持续cache不能机械要求全/proc count/inventory冷末态相等，按本slot exported payload/inode、dup/import、实际末次GPU use/退役逐项验收，全表仍保留。

任何control/profile/model/params/GPU/readback/hash错误或deadline：exacttuple quarantine、永拒迟到、原SRfallback、native.abort；hold全部pending images/allocations，不复用/销毁，确认真实私有helper退出或真实fence/drain后才安全清理。异常exit/强制退出不得报告normalrelease或正常FD退役。配置/尺寸改变直接bypass，不加NR resize/restart框架。

## 10. 只读依赖与构建清单

依赖总表复用冻结 `opti/runtime-pins.json`，新 `c2-runtime-pins.json` 仅新增本轮scope和source/SDK快照，不放宽GE搜索目录。与实际PASS G3 `.../joint-nr/bbport/runs/g3-20261011T064043.903889Z/results.json` / helper81afee4f同来源。

| runtime | exact SHA256 | bytes |
| --- | --- | --- |
| /home/arinp/Code/dlss5_linux/dll/runtime/nvngx_dlssnr.dll | 6eb209e764f39872625debd6abaf45e2bb6322f6f270f781f70c059ae30b3927 | 165830144 |
| /usr/lib64/nvidia/wine/_nvngx.dll | 172faedfd9b1c7faddf07b846667484fcf35d8a4cae17d1abada5e94714071db | 2012272 |
| /home/arinp/Code/dlss5_linux/OptiScaler-Linux-NR/build_out/nr-runtime-poc-20261010/build_out/poc-runtime-20261010/nvngx.dll_dlssnr.dll | 744a1b5b95d8721925261b454366cd3463eb158f02ddde02ad8e554b47386c75 | 56832 |
| GE files/lib/wine/nvapi/x86_64-windows/nvapi64.dll | 9811daad678402249ea244e06dfee47a1c063f26b1d081624f9f19f72307bdd7 | 1888256 |
| GE files/lib/wine/dxvk/x86_64-windows/dxgi.dll | a32191d1292ee388d7e0c3b8e43716f944971c7b49ce77f213be2cf4f6432756 | 4857856 |

GE根严格 `/home/arinp/.local/share/Steam/compatibilitytools.d/GE-Proton11-7-x86_64/`；helper按G3绝对PE LoadLibraryExW DXGI先于NVAPI/core，LOAD_WITH_ALTERED_SEARCH_PATH、actual modulepathmatch日志，持有至core/NVAPI shutdown后按次序FreeLibrary。无DLL复制/D3D11或D3D12替换/私有NVAPI调用/Proton全run。

child env严格复用成功条件：DXVK_ENABLE_NVAPI=1、NVIDIA_WINE_DLL_DIR=/usr/lib64/nvidia/wine、WINEDLLOVERRIDES="mscoree,mshtml=;nvapi64=n;dxgi=n"、WINEDLLPATH=GE/files/lib/vkd3d:GE/files/lib/wine无继承extra、DXVK_NVAPI_LOG_LEVEL=trace；WINEDEBUG保持当前fixture diagnostic并原样日志。三已pin vkd3d PE/hostnvngx/Unixngx/loader/preloader/compiler/SDK/Nlohmann/安全parameterABI/common constants/productionSPV/header均逐sha+bytes验证，不仅检查文件存在。model assets只读不复制。

G3 source `wine_joint_g3.cpp:31-114`设备/model/init、`:127-176`真实encode/Evaluate/resolve/readback、`:178-192`shutdown是复用定位；`g3_production_adapter.h:14,59-95,216-259,261-289,368-372`是尺寸/dispatch/readback/borrowed wrap必须一致适配处。二者路径前缀 `/home/arinp/Code/nrbridge-design/poc-20261011/joint-nr/opti/`。新adapter保 GPL-3.0-or-later/SPDX/上游 attribution（包括原shader RenoDX attribution），transport保MIT；旧源不改。

root审核合同后，build_c2独立编译产物、新source/include/runtime全pin、build/source snapshots/rawcompilerlog、exit code与build-only NOT_RUN状态；不得改旧build_game或覆盖out。B对应新增native bridge/process/shader/hook profile/trigger与独立build manifest，实际限定源位置：`vk_temporal_upscaler.cpp` HDR Run桥接及merge hook、`vk_camera_motion.*`实际current/previousjitter读取、现有C1 process/bridge/present ledger作为只读模板，不修稳定SR算法。

## 11. 本轮交付边界

当前只有合同，C2 compile/model/game/present/skipEvaluate均 NOT_RUN。C1正例PASS与10.011s timeout负例隔离/reap证据保留；late response仍NOT_RUN。后续运行分positive/negative保存actual tuple/input/reset/params/calls/result/GPU/poison/alpha/digests/FD/exit/present，独立判transport/model/profile/lifecycle。构建/加载/创建成功只能子项，不宣称NR1080实时性能、图像质量提升、连续NR、非零jitter或曝光正确。
