# Q1 Quality NR 初始诊断归档

normal DLSS Quality / HDR / 实际 jitter1 的四个初始 gate 已通过：noop1、single1、skipEvaluate1 阴性对照及 resident4。A/B 只读独立审计与 Root 最终 acceptance 一致。此次仅提交文本源码与小型证据索引；没有新的构建、Wine、GPU 或游戏运行。

| 原始 case | 结论 | 实际范围 |
|---|---|---|
| q1-noop-20261012 | BLOCKED | 首次 native entry 混用 guest camera1920×1080 与实际 motion1280×720；Helper/传输未启动。原记录保留。 |
| q1-noop-gatefix-20261012 | PASS | source11769；无模型 init/create/Evaluate，实际共享 GPU copy1；source/output及原SR half往返逐位一致；唯一 noop Present，正常退役。 |
| q1-single-20261012 | PASS | source8684；实际 Evaluate1/result1/resolve1/reset1、GPU fence；raw poison被覆盖、输出有限、alpha原bits保留，唯一 NR Present。 |
| q1-skip-20261012 | BLOCKED / negative NOT_RUN | Continue加载时PM4断言、exit23，场景/Helper启动前失败；原因UNPROVEN，不能归NR或称后续重试修复。 |
| q1-skip2-20261012 | PASS negative | source7113；feature已初始化但 Evaluate0/resolve0，rawRGB全NaN、输出全−1234；同正常接纳谓词拒绝INVALID，原SR Present，正常退役。 |
| q1-resident4-20261012 | PASS | source9541..9544；同HelperPID1587369、device/feature/四共享image，实际Evaluate/resolve各4，Reset1/0/0/0；四个唯一 selectedNR Present9542..9545。 |

早先 Q0 Continue 时 PM4 dword0x217/submission50616/exit23 的独立加载失败也只在索引中引用。Q1 skip失败为dword0x8/submission43576/exit23，两者原因未证明。成功noop/single/resident不能证明PM4问题已修复。

每个成功 case 的 actual 720p Depth32 原始bits与转换/native/Wine读回逐字节一致；原MVhalf经独立IEEE half→float32解码后，与两端720p RG32F逐字节一致。共享1080p sourceColor与resolvedOutput各自的native/Wine实际读回及FNV一致。跨进程只传控制、摘要及统计，像素为DMA_BUF GPU资源。

single/resident每个正帧raw RGB 6,220,800分量有限且覆盖NaN poison，resolvedRGBA 8,294,400分量有限、非−1234并在half可表示范围，2,073,600个源alpha原bits保留，实际返回half读回有限。模型执行证据来自真实API调用/result、GPU fence和raw poison覆盖；不凭hash变化判推理，不要求模型输出等于输入。skip2的rawRGB全NaN且resolved全哨兵，实际native拒收且未进行NR returnhalf转换。

resident三次slot复用均在prior merge GPU末次使用及consumed完成后：19088/19092/19096；四帧merge/prepare/ready分别19088/19092/19096/19100，source+1 Present关联唯一且不计重播。present另scheduler tick不能与draw tick作数值相等比较。Root检查实际Clinic/HUD四张图；A核日志/读回及dump身份，不把视觉观察称画质认证。

noop Wine72→72；single/skip2/resident Wine83→83，四案native均136→136，完整可读FD清单逐项相等。四共享payload原FD/inode在active退役及最终两端清单均不存在；终态精确tuple、native release/audit及最后GPU使用完成，Helper正常release/reap0，外层controlledstop0且ownPG/privateprefix无残留。这是这些短诊断case的生命周期证据，不等于长期泄漏、Quality NR resize/restart/timeout验收。

NR phase与NR MVecScale仍UNVERIFIED。profile direct720_raw_renderpx_v1采用phase_correction=none、原render-pixel previous−current MV及production scale1候选；不存在NR jitter setter，实际current/previous jitter如实记录。k_output=NOT_IDENTIFIED；manualWhitePoint1/passthrough=false/exposure_unknown，未恢复未知曝光。Color/model/output1920×1080、guides1280×720、WorkingScale1，不能称同尺寸guide或降模型分辨率。正常Quality120/1800、运动画质/相位/单位验证未执行。

每个NR诊断帧包含94,003,200B像素/guide本地读回，另64B validator与128B GPU query统计；single约15.615s、resident约15.595..16.237s的frame elapsed主要包含CPU全量统计。GPU fence/分段timestamp不是pure-NR性能，未评分FPS/画质。额外NumPy最近舍入float32→half逐位比较不是single合同gate；其RGB与实际GPU转换有差异，alpha无差异，具体舍入原因未证。实际合同的finite/range/alpha门槛通过，未改变shader或将该额外比较判验收FAIL。

原[README](README.md)、[静态results](results.json)、所有源码及out/build.json保持冻结原字节；它们原来的BUILD_ONLY/NOT_RUN描述属于构建时状态，不覆盖成事后runtime通过。新[q1-runtime-results](q1-runtime-results.json)区分compile/runtime及历史失败；[evidence-index](evidence-index.json)引用原始路径/bytes/SHA；[build-index](build-index.json)仅保存原构建JSON元数据与126 build pins +2已验loader pins。已跑Helper SHA4d770cf0…、原build SHA20dc6159…、唯一contract SHA482a5bcc…；归档源码或重建副本不能冒充原已跑artifact。

许可沿既有[poc/game_nr/LICENSE-NOTES](../LICENSE-NOTES.md)：保留源码SPDX/root GPL-3.0及既有MIT声明，只读依赖通过本地路径/hash复用。本次不提交PE/SPV/模型/SDK/原始像素/大型日志/游戏文件或存档，不复制vendor运行库。唯一canonical仍由Root独占integration/QUALITY-NR-CONTRACT.md，本归档没有另建协议版本。
