#pragma once
// SPDX-License-Identifier: GPL-3.0-or-later
// r2 copy: only embedded SPIR-V identity guard differs; resource/descriptor/fence math is frozen.
// Reuse frozen owned image/buffer/upload/readback/fence utilities only; never call NR shader/NGX.
#include "../high-fps-fixture/lowres_production_adapter.h"
#include "/home/arinp/Code/dlss5_linux/OptiScaler-Linux-NR/OptiScaler/shaders/dlssnr/DlssNr_CacheCommon.h"
#include "out/sky_cache_spv.h"
#define SCV(n) api<PFN_##n>(#n)
static_assert(offsetof(DlssNrCacheConstants,JitterDeltaY)==152);
class CacheOwned:public LowresProduction {
 public:
 struct Bank {Image edit,guide,l1guide;std::array<Image,3> levels;};
 unsigned width,height,gw,gh;std::array<Bank,2> banks;Image color,nr,depth,motion,output,dummy,exposure,stats;
 Buffer source_upload,nr_upload,depth_upload,mv_upload,source_read,nr_read,depth_read,mv_read,out_read,edit_read;std::array<Buffer,4> constants;
 CacheOwned(CacheContext&c,unsigned w,unsigned h,unsigned dw,unsigned dh):LowresProduction(c.instance,c.physical,c.device,c.cmd,c.queue,c.gipa),width(w),height(h),gw(dw),gh(dh){
  color=image(VK_FORMAT_R32G32B32A32_SFLOAT,false,w,h);nr=image(VK_FORMAT_R32G32B32A32_SFLOAT,false,w,h);output=image(VK_FORMAT_R32G32B32A32_SFLOAT,true,w,h);
  depth=image(VK_FORMAT_R32_SFLOAT,false,dw,dh);motion=image(VK_FORMAT_R32G32_SFLOAT,false,dw,dh);dummy=image(VK_FORMAT_R32G32B32A32_SFLOAT,true,1,1);exposure=image(VK_FORMAT_R32G32B32A32_SFLOAT,false,1,1);stats=image(VK_FORMAT_R32_UINT,true,4,1);
  for(auto&b:banks){b.edit=image(VK_FORMAT_R32G32B32A32_SFLOAT,true,w,h);b.guide=image(VK_FORMAT_R32G32B32A32_SFLOAT,true,w,h);unsigned pw=(w+3)/4,ph=(h+3)/4;b.l1guide=image(VK_FORMAT_R32G32B32A32_SFLOAT,true,pw,ph);for(auto&l:b.levels){l=image(VK_FORMAT_R32G32B32A32_SFLOAT,true,pw,ph);pw=(pw+3)/4;ph=(ph+3)/4;}}
  auto buf=[&](uint64_t n,VkBufferUsageFlags u){return buffer(n,u);};uint64_t rgba=uint64_t(w)*h*16,dbytes=uint64_t(dw)*dh*4;
  source_upload=buf(rgba,VK_BUFFER_USAGE_TRANSFER_SRC_BIT);nr_upload=buf(rgba,VK_BUFFER_USAGE_TRANSFER_SRC_BIT);depth_upload=buf(dbytes,VK_BUFFER_USAGE_TRANSFER_SRC_BIT);mv_upload=buf(dbytes*2,VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
  source_read=buf(rgba,VK_BUFFER_USAGE_TRANSFER_DST_BIT);nr_read=buf(rgba,VK_BUFFER_USAGE_TRANSFER_DST_BIT);depth_read=buf(dbytes,VK_BUFFER_USAGE_TRANSFER_DST_BIT);mv_read=buf(dbytes*2,VK_BUFFER_USAGE_TRANSFER_DST_BIT);out_read=buf(rgba,VK_BUFFER_USAGE_TRANSFER_DST_BIT);edit_read=buf(rgba,VK_BUFFER_USAGE_TRANSFER_DST_BIT);
  for(auto&cb:constants)cb=buf(256,VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
  cache_shader();create_noop_fence();
 }
 void cache_shader(){
  VkSamplerCreateInfo sm{VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO};sm.magFilter=sm.minFilter=VK_FILTER_LINEAR;sm.mipmapMode=VK_SAMPLER_MIPMAP_MODE_NEAREST;sm.addressModeU=sm.addressModeV=sm.addressModeW=VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;sm.maxLod=0;check(SCV(vkCreateSampler)(device,&sm,nullptr,&sampler),"CacheSampler");
  VkDescriptorSetLayoutBinding bindings[21]{};for(unsigned i=0;i<21;i++){auto&b=bindings[i];b.binding=i;b.descriptorCount=1;b.stageFlags=VK_SHADER_STAGE_COMPUTE_BIT;b.descriptorType=i==0?VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:i<=12?VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:i<=19?VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:VK_DESCRIPTOR_TYPE_SAMPLER;}
  VkDescriptorSetLayoutCreateInfo sl{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};sl.bindingCount=21;sl.pBindings=bindings;check(SCV(vkCreateDescriptorSetLayout)(device,&sl,nullptr,&setLayout),"CacheLayout");VkPipelineLayoutCreateInfo pl{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};pl.setLayoutCount=1;pl.pSetLayouts=&setLayout;check(SCV(vkCreatePipelineLayout)(device,&pl,nullptr,&pipeLayout),"CachePipeLayout");
  VkShaderModuleCreateInfo sci{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};sci.codeSize=sizeof(sky_cache_spv);sci.pCode=sky_cache_spv;VkShaderModule sh{};check(SCV(vkCreateShaderModule)(device,&sci,nullptr,&sh),"CacheSPIRV");VkComputePipelineCreateInfo pc{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};pc.layout=pipeLayout;pc.stage.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;pc.stage.stage=VK_SHADER_STAGE_COMPUTE_BIT;pc.stage.module=sh;pc.stage.pName="CSMain";auto r=SCV(vkCreateComputePipelines)(device,nullptr,1,&pc,nullptr,&pipe);SCV(vkDestroyShaderModule)(device,sh,nullptr);check(r,"CachePipeline");
  VkDescriptorPoolSize sizes[]={{VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,4},{VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE,48},{VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,28},{VK_DESCRIPTOR_TYPE_SAMPLER,4}};VkDescriptorPoolCreateInfo dp{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};dp.maxSets=4;dp.poolSizeCount=4;dp.pPoolSizes=sizes;check(SCV(vkCreateDescriptorPool)(device,&dp,nullptr,&descriptorPool),"CacheDescriptors");
 }
 DlssNrCacheConstants base(unsigned mode){DlssNrCacheConstants c{};c.Mode=mode;c.Width=width;c.Height=height;c.DepthWidth=gw;c.DepthHeight=gh;c.MotionWidth=gw;c.MotionHeight=gh;c.MvScaleX=c.MvScaleY=1;c.Epsilon=1.f/512;c.DepthTolerance=.1f;c.ColourTolerance=.5f;c.HighDecay=.97f;c.RefreshBlend=c.LowGain=c.HighGain=1;c.Bilateral=1;c.ExposurePreMul=1;c.MaxLumaEdit=1.5f;return c;}
 void pass(unsigned slot,DlssNrCacheConstants c,bool whole,std::array<Image*,12> reads,std::array<Image*,7> writes){
  require(slot<4&&safe,"owned one-slot recording prerequisites",3);auto&cb=constants[slot];memcpy(cb.mapped,&c,256);uint32_t valid=whole?1:0;memcpy(static_cast<char*>(cb.mapped)+156,&valid,4);
  VkBufferMemoryBarrier hb{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};hb.srcAccessMask=VK_ACCESS_HOST_WRITE_BIT;hb.dstAccessMask=VK_ACCESS_UNIFORM_READ_BIT;hb.srcQueueFamilyIndex=hb.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;hb.buffer=cb.handle;hb.size=256;SCV(vkCmdPipelineBarrier)(command,VK_PIPELINE_STAGE_HOST_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,0,0,nullptr,1,&hb,0,nullptr);
  for(auto*p:reads)transition(*p,VK_IMAGE_LAYOUT_GENERAL);for(auto*p:writes)transition(*p,VK_IMAGE_LAYOUT_GENERAL);
  VkDescriptorSetAllocateInfo ai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};ai.descriptorPool=descriptorPool;ai.descriptorSetCount=1;ai.pSetLayouts=&setLayout;VkDescriptorSet set{};check(SCV(vkAllocateDescriptorSets)(device,&ai,&set),"CacheSet");VkDescriptorBufferInfo bi{cb.handle,0,256};VkDescriptorImageInfo ii[20]{};VkWriteDescriptorSet ds[21]{};
  for(unsigned k=0;k<12;k++){ii[k].imageView=reads[k]->view;ii[k].imageLayout=VK_IMAGE_LAYOUT_GENERAL;}
  for(unsigned k=0;k<7;k++){ii[k+12].imageView=writes[k]->view;ii[k+12].imageLayout=VK_IMAGE_LAYOUT_GENERAL;}ii[19].sampler=sampler;
  for(unsigned k=0;k<21;k++){auto&d=ds[k];d.sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;d.dstSet=set;d.dstBinding=k;d.descriptorCount=1;d.descriptorType=k==0?VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER:k<=12?VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE:k<=19?VK_DESCRIPTOR_TYPE_STORAGE_IMAGE:VK_DESCRIPTOR_TYPE_SAMPLER;if(k==0)d.pBufferInfo=&bi;else d.pImageInfo=&ii[k-1];}
  SCV(vkUpdateDescriptorSets)(device,21,ds,0,nullptr);SCV(vkCmdBindPipeline)(command,VK_PIPELINE_BIND_POINT_COMPUTE,pipe);SCV(vkCmdBindDescriptorSets)(command,VK_PIPELINE_BIND_POINT_COMPUTE,pipeLayout,0,1,&set,0,nullptr);SCV(vkCmdDispatch)(command,(c.Width+7)/8,(c.Height+7)/8,1);memoryBarrier();
 }
 void sequence(unsigned prev,unsigned next,bool capture,bool whole,float dx,float dy){
  auto&p=banks[prev];auto&n=banks[next];std::array<Image*,12> reads={&p.edit,&p.guide,&color,&depth,&motion,&nr,&dummy,&dummy,&dummy,&dummy,&exposure,&dummy};
  // Out2 carries the current unedited frame in Reproject; a separate image prevents feedback.
  auto first=base(capture?2:1);first.HistoryValid=capture?0:1;first.JitterDeltaX=dx;first.JitterDeltaY=dy;
  pass(0,first,true,reads,{&n.edit,&n.guide,&output,&n.levels[0],&n.l1guide,&stats,&dummy});
  for(unsigned i=1;i<3;i++){auto c=base(3);const auto&src=n.levels[i-1].ngx.Resource.ImageViewInfo;const auto&dst=n.levels[i].ngx.Resource.ImageViewInfo;c.Width=dst.Width;c.Height=dst.Height;c.SourceWidth=src.Width;c.SourceHeight=src.Height;reads[5]=&n.levels[i-1];pass(i,c,true,reads,{&n.levels[i],&dummy,&output,&dummy,&dummy,&stats,&dummy});}
  reads={&n.edit,&n.guide,&color,&depth,&motion,&n.levels[0],&n.l1guide,&n.levels[1],&n.levels[2],&dummy,&exposure,&dummy};auto c=base(4);pass(3,c,whole,reads,{&output,&dummy,&dummy,&dummy,&dummy,&stats,&dummy});
 }
};

#undef SCV
