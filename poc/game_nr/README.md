# 隔离游戏 NR-only 实验源码

本 worktree 从稳定 linux-nr/eaada4c2667a26947f6b934008870025e3e1cfb5 创建，实验分支 poc/game-nr-20261011。仅此 worktree 开发/提交；稳定工作树与原 game-poc/opti、C1/G1/G2/G3 原始证据冻结。

c2-baseline 为逐字节复制的 C2 已验收源码/合同/构建说明；baseline-provenance.json 记录原始路径及 SHA，C2-original-build-record.json 仍指原已跑 executable80a953，不是本副本的 build 记录。本副本尚未构建/运行，不含任何 exe/model PE/游戏存档/大 outputs。原 actual single+skip 的审计仍在 ../integration/c2-final-audit.json（相对 game-poc 根），不移动。

需要复现归档副本时，使用 c2-baseline/build_c2.py；父目录 integration/c2-contract-review.json 为原只读 review 字节副本。脚本以自己的 ROOT 写忽略的 out-c2，仍只读复用原 fixture header/runtime/model 的绝对路径并验证完整 pins。新生成 exe 必须独立 build/provenance/runtime 验收，不能继承原80a953产物的 PASS。C3 的源/协议/build记录置 c3/；GPU 只由 Integration/root 串行运行，共享锁及 bounded watchdog 不变。
