// SPDX-License-Identifier: GPL-3.0-or-later
// PE source/ABI compile gate only. This executable is never run by the builder.
#include "/home/arinp/Code/nrbridge-design/game-poc-20261011/opti-highfps-worktree/poc/game_nr/high-fps-fixture/lowres_common.h"
#include "/home/arinp/Code/nrbridge-design/game-poc-20261011/opti-highfps-worktree/poc/game_nr/sky-cache-vk/cache_context.h"
#include "sky_antiflicker_owned.h"
extern "C" __declspec(dllexport) SkyAntiflickerMaskedOwned* construct_sky_owned(CacheContext&c){return new SkyAntiflickerMaskedOwned(c,1280,720,1280,720,sky_antiflicker::Candidate::low_band_antipop);}
extern "C" __declspec(dllexport) void record_sky_owned(SkyAntiflickerMaskedOwned&n,SkyAntiflickerMaskedOwned::Image&raw,SkyAntiflickerMaskedOwned::Image&depth,SkyAntiflickerMaskedOwned::Image&mv,const sky_antiflicker::Admission&a,double actual_frame_dt,float actual_jitter_dx,float actual_jitter_dy){n.record_apply(0,raw,depth,mv,a,sky_antiflicker::Candidate::low_band_antipop,actual_frame_dt,actual_jitter_dx,actual_jitter_dy);}
extern "C" __declspec(dllexport) void record_mask_init(SkyAntiflickerMaskedOwned&n){n.record_initialize_masks();}
extern "C" __declspec(dllexport) VkBuffer return_mask(SkyAntiflickerMaskedOwned&n){return n.pending_return_mask().handle;}
int main(){puts("BUILD_GATE_ONLY_NO_GPU_RUNTIME_ACCEPTANCE");return 0;}
