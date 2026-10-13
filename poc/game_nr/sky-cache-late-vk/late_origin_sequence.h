#pragma once
// SPDX-License-Identifier: GPL-3.0-or-later
// Standalone record-only split graph. Native must use its own actual last-use tick ownership.
// No model calls, scheduler, transport or partial Apply. Reuses frozen r2 pass/SPIR-V.
struct OwnedSnapshotViews {CacheOwned::Image color,depth,motion;};
struct LateOriginSequence {
 CacheOwned& n;CacheOwned::Image& carry;
 unsigned captures=0,reprojects=0,pyramids=0,applies=0,fallback_copies=0;
 void record_history(OwnedSnapshotViews&source,CacheOwned::Image&edited,unsigned prev,unsigned next,bool capture,float jx,float jy){
  auto&p=n.banks[prev];auto&t=n.banks[next];
  std::array<CacheOwned::Image*,12> reads={&p.edit,&p.guide,&source.color,&source.depth,&source.motion,&edited,&n.dummy,&n.dummy,&n.dummy,&n.dummy,&n.exposure,&n.dummy};
  auto c=n.base(capture?2:1);c.HistoryValid=capture?0:1;c.JitterDeltaX=jx;c.JitterDeltaY=jy;
  n.pass(0,c,true,reads,{&t.edit,&t.guide,&carry,&t.levels[0],&t.l1guide,&n.stats,&n.dummy});
  if(capture)++captures;else ++reprojects;
  for(unsigned i=1;i<3;i++){auto pc=n.base(3);auto&s=t.levels[i-1].ngx.Resource.ImageViewInfo;auto&d=t.levels[i].ngx.Resource.ImageViewInfo;pc.Width=d.Width;pc.Height=d.Height;pc.SourceWidth=s.Width;pc.SourceHeight=s.Height;reads[5]=&t.levels[i-1];n.pass(i,pc,true,reads,{&t.levels[i],&n.dummy,&carry,&n.dummy,&n.dummy,&n.stats,&n.dummy});++pyramids;}
 }
 void record_apply(CacheOwned::Image&current,OwnedSnapshotViews&origin,unsigned bank,bool whole){
  auto&b=n.banks[bank];std::array<CacheOwned::Image*,12> reads={&b.edit,&b.guide,&current,&origin.depth,&origin.motion,&b.levels[0],&b.l1guide,&b.levels[1],&b.levels[2],&n.dummy,&n.exposure,&n.dummy};
  n.pass(0,n.base(4),whole,reads,{&n.output,&n.dummy,&carry,&n.dummy,&n.dummy,&n.stats,&n.dummy});if(whole)++applies;else ++fallback_copies;
 }
};
