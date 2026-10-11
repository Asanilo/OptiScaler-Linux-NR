#pragma once
// SPDX-License-Identifier: GPL-3.0-or-later
// C3 independent diagnostic adapter. Public Vulkan only; no NR algorithm implementation.
#include <array>
#include <cstdint>
#include <cstring>
#include "out/validate_spv.h"
class C3GpuValidation {
 NrSharedProduction& n;
 VkDescriptorSetLayout layout{};VkPipelineLayout pipelineLayout{};VkPipeline pipeline{};VkDescriptorPool pool{};VkDescriptorSet set{};
 NrSharedProduction::Buffer counter{};
 template<class T>T f(const char*name){return n.api<T>(name);}
 public:
 explicit C3GpuValidation(NrSharedProduction& nr):n(nr){
  counter=n.buffer(64,VK_BUFFER_USAGE_STORAGE_BUFFER_BIT|VK_BUFFER_USAGE_TRANSFER_DST_BIT);
  VkDescriptorSetLayoutBinding bindings[4]{};
  for(unsigned i=0;i<4;i++){bindings[i].binding=i;bindings[i].descriptorCount=1;bindings[i].stageFlags=VK_SHADER_STAGE_COMPUTE_BIT;bindings[i].descriptorType=i<3?VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;}
  VkDescriptorSetLayoutCreateInfo li{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO};li.bindingCount=4;li.pBindings=bindings;check(f<PFN_vkCreateDescriptorSetLayout>("vkCreateDescriptorSetLayout")(n.device,&li,nullptr,&layout),"validation layout");
  VkPipelineLayoutCreateInfo pl{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};pl.setLayoutCount=1;pl.pSetLayouts=&layout;check(f<PFN_vkCreatePipelineLayout>("vkCreatePipelineLayout")(n.device,&pl,nullptr,&pipelineLayout),"validation pipeline layout");
  VkShaderModuleCreateInfo sm{VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO};sm.codeSize=sizeof(c3_validate_spv);sm.pCode=c3_validate_spv;VkShaderModule shader{};check(f<PFN_vkCreateShaderModule>("vkCreateShaderModule")(n.device,&sm,nullptr,&shader),"validation SPIRV");
  VkComputePipelineCreateInfo pc{VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO};pc.layout=pipelineLayout;pc.stage.sType=VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;pc.stage.stage=VK_SHADER_STAGE_COMPUTE_BIT;pc.stage.module=shader;pc.stage.pName="main";
  const auto result=f<PFN_vkCreateComputePipelines>("vkCreateComputePipelines")(n.device,nullptr,1,&pc,nullptr,&pipeline);f<PFN_vkDestroyShaderModule>("vkDestroyShaderModule")(n.device,shader,nullptr);check(result,"validation compute pipeline");
  VkDescriptorPoolSize sizes[2]={{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,3},{VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,1}};VkDescriptorPoolCreateInfo dp{VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO};dp.maxSets=1;dp.poolSizeCount=2;dp.pPoolSizes=sizes;check(f<PFN_vkCreateDescriptorPool>("vkCreateDescriptorPool")(n.device,&dp,nullptr,&pool),"validation pool");
  VkDescriptorSetAllocateInfo ai{VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO};ai.descriptorPool=pool;ai.descriptorSetCount=1;ai.pSetLayouts=&layout;check(f<PFN_vkAllocateDescriptorSets>("vkAllocateDescriptorSets")(n.device,&ai,&set),"validation persistent set");
 }
 void record(NrSharedProduction::Image& source,NrSharedProduction::Image& raw,NrSharedProduction::Image& output){
  need(n.safe,"validation descriptors require prior GPU completion");
  n.transition(source,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);n.transition(raw,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);n.transition(output,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  f<PFN_vkCmdFillBuffer>("vkCmdFillBuffer")(n.command,counter.handle,0,64,0);
  VkBufferMemoryBarrier b{VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER};b.buffer=counter.handle;b.offset=0;b.size=64;b.srcQueueFamilyIndex=b.dstQueueFamilyIndex=VK_QUEUE_FAMILY_IGNORED;b.srcAccessMask=VK_ACCESS_TRANSFER_WRITE_BIT;b.dstAccessMask=VK_ACCESS_SHADER_READ_BIT|VK_ACCESS_SHADER_WRITE_BIT;
  f<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(n.command,VK_PIPELINE_STAGE_TRANSFER_BIT,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,0,0,nullptr,1,&b,0,nullptr);
  VkDescriptorImageInfo ii[3]={{n.sampler,source.view,source.layout},{n.sampler,raw.view,raw.layout},{n.sampler,output.view,output.layout}};VkDescriptorBufferInfo bi{counter.handle,0,64};VkWriteDescriptorSet writes[4]{};
  for(unsigned i=0;i<4;i++){writes[i].sType=VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;writes[i].dstSet=set;writes[i].dstBinding=i;writes[i].descriptorCount=1;writes[i].descriptorType=i<3?VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER:VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;if(i<3)writes[i].pImageInfo=&ii[i];else writes[i].pBufferInfo=&bi;}
  f<PFN_vkUpdateDescriptorSets>("vkUpdateDescriptorSets")(n.device,4,writes,0,nullptr);f<PFN_vkCmdBindPipeline>("vkCmdBindPipeline")(n.command,VK_PIPELINE_BIND_POINT_COMPUTE,pipeline);f<PFN_vkCmdBindDescriptorSets>("vkCmdBindDescriptorSets")(n.command,VK_PIPELINE_BIND_POINT_COMPUTE,pipelineLayout,0,1,&set,0,nullptr);f<PFN_vkCmdDispatch>("vkCmdDispatch")(n.command,240,135,1);
  b.srcAccessMask=VK_ACCESS_SHADER_WRITE_BIT;b.dstAccessMask=VK_ACCESS_HOST_READ_BIT;f<PFN_vkCmdPipelineBarrier>("vkCmdPipelineBarrier")(n.command,VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,VK_PIPELINE_STAGE_HOST_BIT,0,0,nullptr,1,&b,0,nullptr);
 }
 std::array<uint32_t,16> read(){need(n.safe,"stats require observed actual GPU fence");std::array<uint32_t,16> result{};std::memcpy(result.data(),counter.mapped,64);return result;}
 ~C3GpuValidation(){if(!n.safe)return;if(pool)f<PFN_vkDestroyDescriptorPool>("vkDestroyDescriptorPool")(n.device,pool,nullptr);if(pipeline)f<PFN_vkDestroyPipeline>("vkDestroyPipeline")(n.device,pipeline,nullptr);if(pipelineLayout)f<PFN_vkDestroyPipelineLayout>("vkDestroyPipelineLayout")(n.device,pipelineLayout,nullptr);if(layout)f<PFN_vkDestroyDescriptorSetLayout>("vkDestroyDescriptorSetLayout")(n.device,layout,nullptr);}
};
