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
