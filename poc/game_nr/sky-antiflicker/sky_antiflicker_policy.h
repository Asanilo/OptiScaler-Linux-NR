#pragma once
// SPDX-License-Identifier: GPL-3.0-or-later
// Local kernel ABI only, not a transport protocol or permission to extend edit lifetime.
#include <cstdint>
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <cstring>
#include "/home/arinp/Code/dlss5_linux/OptiScaler-Linux-NR/OptiScaler/shaders/dlssnr/DlssNr_CacheCommon.h"
namespace sky_antiflicker {
enum class Candidate : uint32_t {low_band=1, low_band_antipop=2};
struct Tail {uint32_t whole;float anti_pop_step,frame_tolerance;uint32_t context_width,context_height;};
static_assert(sizeof(Tail)==20);static_assert(sizeof(DlssNrCacheConstants)==256);static_assert(offsetof(DlssNrCacheConstants,JitterDeltaY)==152);
struct Admission {uint64_t current=0,origin=0;bool whole=false,complete_chain=false,reset=false,version_epoch_match=false;bool allowed()const{return whole&&complete_chain&&!reset&&version_epoch_match&&origin>0&&current>=origin&&current-origin<=4;}};
struct ShownState {
 bool valid=false;uint64_t source=0,oldest_origin=0;
 bool may_use(const Admission&a)const{return a.allowed()&&valid&&source<UINT64_MAX&&source+1==a.current&&oldest_origin>0&&a.current>=oldest_origin&&a.current-oldest_origin<=4;}
 void retire() {valid=false;source=oldest_origin=0;}
 // Keep the oldest CONTRIBUTING origin, not just the new model target's ID. A fade cannot reset age.
 void record(const Admission&a,bool used_previous){if(!a.allowed()){retire();return;}oldest_origin=used_previous?std::min(oldest_origin,a.origin):a.origin;source=a.current;valid=true;}
};
struct FrameClock {double smoothed=1.0/60.0;float step(double seconds){if(!std::isfinite(seconds)||seconds<=0)throw std::invalid_argument("actual positive frame dt");smoothed=smoothed*.9+std::clamp(seconds,1.0/500.0,1.0/10.0)*.1;return float(.75*smoothed);}};
inline void tail(DlssNrCacheConstants&c,bool whole,float step,uint32_t qw,uint32_t qh){Tail t{whole?1u:0u,step,.15f,qw,qh};std::memcpy(reinterpret_cast<unsigned char*>(&c)+156,&t,sizeof t);}
}
