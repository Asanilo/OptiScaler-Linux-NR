#pragma once
// SPDX-License-Identifier: GPL-3.0-or-later
// Independent candidate: r2 resource/math primitives; adds shown/context state, explicit tail and23bindings.
// Reuse frozen owned image/buffer/upload/readback/fence utilities only; never call NR shader/NGX.
#include "/home/arinp/Code/nrbridge-design/game-poc-20261011/opti-highfps-worktree/poc/game_nr/high-fps-fixture/lowres_production_adapter.h"
#include "sky_antiflicker_policy.h"
#include "/home/arinp/Code/dlss5_linux/OptiScaler-Linux-NR/OptiScaler/shaders/dlssnr/DlssNr_CacheCommon.h"
#include "out/sky_antiflicker_spv.h"
#define SCV(n) api<PFN_##n>(#n)
static_assert(offsetof(DlssNrCacheConstants,JitterDeltaY)==152);
class SkyAntiflickerOwned:public LowresProduction {
 public:
 struct Bank {Image edit,guide,l1guide;std::array<Image,3> levels;};
 unsigned width,height,gw,gh;std::array<Bank,2> banks;Image color,nr,depth,motion,output,dummy,exposure,stats;
 std::array<Buffer,6> constants;
 Image raw_edit;std::array<Image,2> shown,shown_guide,context;sky_antiflicker::ShownState state;sky_antiflicker::FrameClock clock;unsigned shown_index=0;bool pending=false,pending_old=false,pending_history=false,context_valid=false,pending_context=false;uint64_t context_source=0;unsigned pending_next=0;uint64_t pending_floor=0;sky_antiflicker::Admission pending_admission;
 SkyAntiflickerOwned(CacheContext&c,unsigned w,unsigned h,unsigned dw,unsigned dh):LowresProduction(c.instance,c.physical,c.device,c.cmd,c.queue,c.gipa),width(w),height(h),gw(dw),gh(dh){
  require(w==1280&&h==720&&dw==1280&&dh==720,"initial exact pre-SR720 owned profile",3);
  color=image(VK_FORMAT_R32G32B32A32_SFLOAT,false,w,h);nr=image(VK_FORMAT_R32G32B32A32_SFLOAT,false,w,h);output=image(VK_FORMAT_R32G32B32A32_SFLOAT,true,w,h);
  depth=image(VK_FORMAT_R32_SFLOAT,false,dw,dh);motion=image(VK_FORMAT_R32G32_SFLOAT,false,dw,dh);dummy=image(VK_FORMAT_R32G32B32A32_SFLOAT,true,1,1);exposure=image(VK_FORMAT_R32G32B32A32_SFLOAT,false,1,1);stats=image(VK_FORMAT_R32_UINT,true,4,1);
  for(auto&b:banks){b.edit=image(VK_FORMAT_R32G32B32A32_SFLOAT,true,w,h);b.guide=image(VK_FORMAT_R32G32B32A32_SFLOAT,true,w,h);unsigned pw=(w+3)/4,ph=(h+3)/4;b.l1guide=image(VK_FORMAT_R32G32B32A32_SFLOAT,true,pw,ph);for(auto&l:b.levels){l=image(VK_FORMAT_R32G32B32A32_SFLOAT,true,pw,ph);pw=(pw+3)/4;ph=(ph+3)/4;}}
  auto buf=[&](uint64_t n,VkBufferUsageFlags u){return buffer(n,u);};
  raw_edit=image(VK_FORMAT_R32G32B32A32_SFLOAT,true,w,h);for(unsigned i=0;i<2;i++){shown[i]=image(VK_FORMAT_R32G32B32A32_SFLOAT,true,w,h);shown_guide[i]=image(VK_FORMAT_R32G32B32A32_SFLOAT,true,w,h);context[i]=image(VK_FORMAT_R32G32B32A32_SFLOAT,true,(w+3)/4,(h+3)/4);}
  for(auto&cb:constants)cb=buf(256,VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
  cache_shader();create_noop_fence();
 }
 ~SkyAntiflickerOwned(){if(pending&&serial<=pending_floor)safe=false;} // recorded-but-unproved/unknown => HOLD, not normal API retirement
 void cache_shader(){
  VkSamplerCreateInfo sm{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};sm.magFilter=sm.minFilter=VK_FILTER_LINEAR;sm.mipmapMode=VK_SAMPLER_MIPMAP_MODE_NEAREST;sm.addressModeU=sm.addressModeV=sm.addressModeW=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;sm.maxLod=0;check(SCV(vkCreateSampler)(device,&sm,nullptr,&sampler),"CacheSampler");
  VkDescriptorSetLayoutBinding bindings[23]{};for(unsigned i=0;i<23;i++){auto&b=bindings[i];b.binding=i;b.descriptorCount=1;b.stageFlags=VK_SHADER_STAGE_COMPUTE_BIT;b.descriptorType=i==0?VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:i<=12?VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:i<=19?VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:i==20?VK_DESCRIPTOR_TYPE_SAMPLER:VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;}
  VkDescriptorSetLayoutCreateInfo sl{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};sl.bindingCount=23;sl.pBindings=bindings;check(SCV(vkCreateDescriptorSetLayout)(device,&sl,nullptr,&setLayout),"CacheLayout");VkPipelineLayoutCreateInfo pl{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};pl.setLayoutCount=1;pl.pSetLayouts=&setLayout;check(SCV(vkCreatePipelineLayout)(device,&pl,nullptr,&pipeLayout),"CachePipeLayout");
  VkShaderModuleCreateInfo sci{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};sci.codeSize=sizeof(sky_antiflicker_spv);sci.pCode=sky_antiflicker_spv;VkShaderModule sh{};check(SCV(vkCreateShaderModule)(device,&sci,nullptr,&sh),"CacheSPIRV");VkComputePipelineCreateInfo pc{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};pc.layout=pipeLayout;pc.stage.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;pc.stage.stage=VK_SHADER_STAGE_COMPUTE_BIT;pc.stage.module=sh;pc.stage.pName="CSMain";auto r=SCV(vkCreateComputePipelines)(device,nullptr,1,&pc,nullptr,&pipe);SCV(vkDestroyShaderModule)(device,sh,nullptr);check(r,"CachePipeline");
  VkDescriptorPoolSize sizes[]={{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,6},{VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,84},{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,42},{VK_DESCRIPTOR_TYPE_SAMPLER,6}};VkDescriptorPoolCreateInfo dp{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};dp.maxSets=6;dp.poolSizeCount=4;dp.pPoolSizes=sizes;check(SCV(vkCreateDescriptorPool)(device,&dp,nullptr,&descriptorPool),"CacheDescriptors");
 }
 DlssNrCacheConstants base(unsigned mode){DlssNrCacheConstants c{};c.Mode=mode;c.Width=width;c.Height=height;c.DepthWidth=gw;c.DepthHeight=gh;c.MotionWidth=gw;c.MotionHeight=gh;c.MvScaleX=c.MvScaleY=1;c.Epsilon=1.f/512;c.DepthTolerance=.1f;c.ColourTolerance=.5f;c.HighDecay=.97f;c.RefreshBlend=c.LowGain=c.HighGain=1;c.Bilateral=1;c.ExposurePreMul=1;c.MaxLumaEdit=1.5f;return c;}
 void pass(unsigned slot,DlssNrCacheConstants c,bool whole,std::array<Image*,14> reads,std::array<Image*,7> writes){
  require(slot<6&&safe,"owned one-slot recording prerequisites",3);auto&cb=constants[slot];memcpy(cb.mapped,&c,256);uint32_t valid=whole?1:0;memcpy(static_cast<char*>(cb.mapped)+156,&valid,4);
  VkBufferMemoryBarrier hb{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};hb.srcAccessMask=VK_ACCESS_HOST_WRITE_BIT;hb.dstAccessMask=VK_ACCESS_UNIFORM_READ_BIT;hb.srcQueueFamilyIndex=hb.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;hb.buffer=cb.handle;hb.size=256;SCV(vkCmdPipelineBarrier)(command,VK_PIPELINE_STAGE_HOST_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,0,0,nullptr,1,&hb,0,nullptr);
  for(auto*p:reads)transition(*p,VK_IMAGE_LAYOUT_GENERAL);for(auto*p:writes)transition(*p,VK_IMAGE_LAYOUT_GENERAL);
  VkDescriptorSetAllocateInfo ai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};ai.descriptorPool=descriptorPool;ai.descriptorSetCount=1;ai.pSetLayouts=&setLayout;VkDescriptorSet set{};check(SCV(vkAllocateDescriptorSets)(device,&ai,&set),"CacheSet");VkDescriptorBufferInfo bi{cb.handle,0,256};VkDescriptorImageInfo ii[22]{};VkWriteDescriptorSet ds[23]{};
  for(unsigned k=0;k<12;k++){ii[k].imageView=reads[k]->view;ii[k].imageLayout=VK_IMAGE_LAYOUT_GENERAL;}
  for(unsigned k=0;k<7;k++){ii[k+12].imageView=writes[k]->view;ii[k+12].imageLayout=VK_IMAGE_LAYOUT_GENERAL;}ii[19].sampler=sampler;for(unsigned i=12;i<14;i++){ii[i+8].imageView=reads[i]->view;ii[i+8].imageLayout=VK_IMAGE_LAYOUT_GENERAL;}
  for(unsigned k=0;k<23;k++){auto&d=ds[k];d.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;d.dstSet=set;d.dstBinding=k;d.descriptorCount=1;d.descriptorType=k==0?VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:k<=12?VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:k<=19?VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:k==20?VK_DESCRIPTOR_TYPE_SAMPLER:VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE;if(k==0)d.pBufferInfo=&bi;else d.pImageInfo=&ii[k-1];}
  SCV(vkUpdateDescriptorSets)(device,23,ds,0,nullptr);SCV(vkCmdBindPipeline)(command,VK_PIPELINE_BIND_POINT_COMPUTE,pipe);SCV(vkCmdBindDescriptorSets)(command,VK_PIPELINE_BIND_POINT_COMPUTE,pipeLayout,0,1,&set,0,nullptr);SCV(vkCmdDispatch)(command,(c.Width+7)/8,(c.Height+7)/8,1);memoryBarrier();
 }
 void record_history(unsigned prev,unsigned next,bool capture,Image&source,Image&source_depth,Image&source_mv,Image&edited,float jx,float jy){
  require(safe&&!pending&&prev<2&&next<2&&prev!=next,"one completed slot and separate legal history banks",3);auto&p=banks[prev];auto&n=banks[next];std::array<Image*,14> reads={&p.edit,&p.guide,&source,&source_depth,&source_mv,&edited,&dummy,&dummy,&dummy,&dummy,&exposure,&dummy,&dummy,&dummy};
  auto c=base(capture?2:1);c.HistoryValid=capture?0:1;c.JitterDeltaX=jx;c.JitterDeltaY=jy;
  pass(0,c,true,reads,{&n.edit,&n.guide,&output,&n.levels[0],&n.l1guide,&stats,&dummy});
  for(unsigned i=1;i<3;i++){auto pc=base(3);auto&a=n.levels[i-1].ngx.Resource.ImageViewInfo;auto&b=n.levels[i].ngx.Resource.ImageViewInfo;pc.Width=b.Width;pc.Height=b.Height;pc.SourceWidth=a.Width;pc.SourceHeight=a.Height;reads[5]=&n.levels[i-1];pass(i,pc,true,reads,{&n.levels[i],&dummy,&output,&dummy,&dummy,&stats,&dummy});}
  pending=true;pending_history=true;pending_floor=serial;
 }
 void record_apply(unsigned bank,Image&current,Image&current_depth,Image&current_mv,const sky_antiflicker::Admission&a,sky_antiflicker::Candidate candidate,double real_dt,float jx,float jy){
  require(safe&&!pending&&bank<2,"prior actual fence before descriptor/constant/image reuse",3);require(current.handle!=output.handle&&current.handle!=raw_edit.handle&&current.handle!=shown[0].handle&&current.handle!=shown[1].handle,"unedited raw is independent of candidate output/history",3);require(current.format==VK_FORMAT_R32G32B32A32_SFLOAT&&current_depth.format==VK_FORMAT_R32_SFLOAT&&current_mv.format==VK_FORMAT_R32G32_SFLOAT,"converted actual current formats",3);for(auto*im:{&current,&current_depth,&current_mv})require(im->ngx.Resource.ImageViewInfo.Width==1280&&im->ngx.Resource.ImageViewInfo.Height==720,"actual current image extents",3);require(candidate==sky_antiflicker::Candidate::low_band||candidate==sky_antiflicker::Candidate::low_band_antipop,"exact local candidate",3);auto&b=banks[bank];bool whole=a.allowed(),old=state.may_use(a);unsigned next=1-shown_index;
  const bool context_on=candidate==sky_antiflicker::Candidate::low_band_antipop;const float actual_step=clock.step(real_dt);const float step=context_on&&old&&context_valid&&context_source==state.source?actual_step:0.f;
  // Context comes from CURRENT unedited colour, never the just edited output or origin NR.
  auto ctx=base(12);ctx.Width=(width+3)/4;ctx.Height=(height+3)/4;ctx.SourceWidth=width;ctx.SourceHeight=height;
  std::array<Image*,14> reads={&b.edit,&b.guide,&current,&current_depth,&current_mv,&dummy,&dummy,&dummy,&dummy,&dummy,&exposure,&dummy,&context[next],&context[shown_index]};
  if(context_on) pass(0,ctx,true,reads,{&context[next],&dummy,&dummy,&dummy,&dummy,&stats,&dummy});
  auto c=base(4);c.Temporal=0;c.LowTemporal=.95f;c.CrossfadeOn=0;c.JitterDeltaX=jx;c.JitterDeltaY=jy;sky_antiflicker::tail(c,whole,step,ctx.Width,ctx.Height);
  reads[5]=&b.levels[0];reads[6]=&b.l1guide;reads[7]=&b.levels[1];reads[8]=&b.levels[2];
  // Apply4 reconstructs full-strength edit in raw_edit; the region limiter does not alter Intensity or gains.
  pass(1,c,whole,reads,{&output,&dummy,&dummy,&raw_edit,&dummy,&stats,&dummy});
  c.Mode=10;c.TemporalValid=old?1u:0u;reads[5]=&raw_edit;reads[6]=&shown[shown_index];reads[7]=&shown_guide[shown_index];
  pass(2,c,whole,reads,{&output,&dummy,&dummy,&shown[next],&dummy,&stats,&dummy});
  // Owned immutable previous shown guide survives any later origin catchup bank writes.
  transition(b.guide,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);transition(shown_guide[next],VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
  VkImageCopy cp{};cp.srcSubresource=cp.dstSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};cp.extent={width,height,1};SCV(vkCmdCopyImage)(command,b.guide.handle,b.guide.layout,shown_guide[next].handle,shown_guide[next].layout,1,&cp);transition(b.guide,VK_IMAGE_LAYOUT_GENERAL);transition(shown_guide[next],VK_IMAGE_LAYOUT_GENERAL);memoryBarrier();
  pending=true;pending_history=false;pending_context=context_on;pending_admission=a;pending_old=old;pending_next=next;pending_floor=serial; // not a completion claim
 }
 void commit_shown_after_fence(){require(pending&&safe&&serial>pending_floor,"actual owned submit+completed fence before history publication",3);if(!pending_history){state.record(pending_admission,pending_old);shown_index=pending_next;context_valid=pending_context&&state.valid;context_source=context_valid?state.source:0;}pending=false;
 }

};

#undef SCV
