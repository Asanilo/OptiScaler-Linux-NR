# DS2 / RTX 4060 Laptop: native DLSS-G high multiplier test

The existing DS2 native 2X path is the reference. Test 4X first, then 6X through the existing
in-memory Ada unlock. Keep OptiScaler FG Input/Output at None so the game retains its native
Streamline swapchain, UI inputs, Reflex and presentation path. This avoids substituting an
OptiScaler-generated frame count for the vendor runtime's counters.

User-supplied production runtime: Streamline 2.14.1 and DLSS-G 310.9.1. Stage only these files from
the SDK `bin/x64`: `nvngx_dlssg.dll`, `sl.common.dll`, `sl.dlss_g.dll`, `sl.interposer.dll`,
`sl.reflex.dll`, `sl.pcl.dll`, `sl.dlss.dll`. Leave the current SR and NR model DLLs unchanged.
Keep hashes, versions, original game DLL backups and a rollback manifest outside the game.

For the 4X case:

```ini
[FrameGen]
Enabled=false
FGInput=nofg
FGOutput=nofg
FGNvngxReplacement=None

[DLSSG]
AdaMfgUnlock=true
AdaBlackwellKernels=true
OverrideInterpolationCount=3
OverrideForceDMFG=false
ForceDMFG=false
```

`OverrideInterpolationCount=5` requests 6X. These settings control the native DLSS-G override;
`FrameGen.Enabled=false` disables OptiScaler's separate FG feature, not the game's native FG.
Enable DLSS Frame Generation in the game's graphics settings. User launches normal Steam; no
Python launch wrapper. Fixed multiplier tests should use the same resolution, SR quality, NR
placement/cache and scene. Disable VSync and FPS limits for the comparison and restore them later.

The unlock requires restart to change its applied state. With the runtime already unlocked,
`Override DLSSG Ratio` can be changed live. Leave Dynamic MFG off during fixed multiplier tests.

Record three distinct observations:

1. Both unlock signatures matched, and Blackwell kernel containers were retargeted.
2. The native `SetOptions` call forwarded three/five generated frames and returned `eOk`.
3. The native runtime `GetState` counters, sampled before any OptiScaler override, report presents
   and status; the operator reports visible smoothness, image artifacts and FPS.

The histogram is presents **since the previous runtime GetState call**, not a guaranteed multiplier
on every poll. Zero or accumulated counts can reflect polling cadence. Capability ceiling and
successful requests alone do not prove unique interpolated frames or even frame spacing.
Check slow camera movement for world motion between generated frames, HUD artifacts, stutter,
flicker and crashes. A high FPS counter with repeated images does not pass acceptance.

The diagnostic does not add GetState calls, synchronize the GPU, change the vendor result or alter
frame generation requests beyond the existing configured override. It logs native statistics every
two seconds only when Ada unlock is enabled and both OptiScaler FG Input/Output are None.

## Current-machine observations (2026-10-10)

Build `eee988215be4a1087c9414d86264a063f563c40c`, MSVC Release run `38049791749`, format run
`38049778100`: both passed. DS2 loaded the supplied production runtime. Both unlock signatures
matched and 31 kernel containers were retargeted at runtime.

The initial fixed 4X configuration overrode the game's 2X and 6X choices with three generated
frames. Consequently, changing the game setting initially kept FPS around 110. Resetting
`Override DLSSG Ratio` to **Default** allowed native game requests to select the count. The saved
configuration now uses `OverrideInterpolationCount=auto`; no restart or NR change was required.
When comparing **game** multipliers, always cancel OptiScaler's fixed override first.

| Game multiplier | Operator FPS | Native requested/forwarded generated frames | Raw presents per poll in steady samples |
| --- | --- | --- | --- |
| 2X | 67 | 1 / 1 | 2 |
| 4X | 110 | 3 / 3 | 4 |
| 5X | 127 | 4 / 4 | 5 |
| 6X | 142 | 5 / 5 | 6 |

Native requests returned `eOk`, and sampled runtime status bits were zero. The operator reports
similar subjective smoothness at 6X versus 4X and no observed tearing. This is one session on the
current machine; the numbers are in-game counter reports, not a controlled benchmark. No claim
of independently verified unique generated images, uniform display frame spacing or improved
input latency follows from these counters.

RSYNC repeatedly logs `setDynamicMFGParams failed with status 1` even with
`DisableReflexSync=true` and `DisableFlipMetering=true`. Withholding the ReflexSync entry point
did **not** eliminate those errors in this run. Pacing compatibility remains unresolved; do not
treat either configuration option as a proven fix or hide the errors to pass acceptance.

Workspace evidence: `testlogs/fg-eee98821/test-status.json`, `operator-fps-report.json`,
`native-multiplier-observations.json` and `operator-fps-confirmed/OptiScaler.log`.
The original seven game runtime DLLs and previous OptiScaler files are retained in
`backups/ds2-fg-eee98821/`. `build/install_fg_test.py --rollback` previews a guarded restore;
`--apply` requires the game closed and all current file hashes matching the rollback record.

Current shared-workspace installation also includes the later FSR SR test layer. Restore that layer first using `build/install_fsr41_test.py --rollback`; its adopted user-saved INI has a separate update history. Then preview FG rollback. See [FSR deployment](../../docs/fsr41-ds2-test.md) and [handoff](../../docs/handoff-20261010.md). Do not bypass a current-INI hash mismatch in the older FG rollback.
