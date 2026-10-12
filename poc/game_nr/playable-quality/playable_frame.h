#pragma once
// SPDX-License-Identifier: GPL-3.0-or-later
// Wire-independent persistent GPU frame module: actual fixed forwarder + frozen production shader.
// Caller owns feature/core/device, external ownership, command reset/begin, submit/fence, idle/terminal.
// No new SR/NR/FG algorithm or public/private model API.
#include "playable_runtime_types.h"
#include "../quality-nr/quality_production_adapter.h"
#include "../quality-nr/gpu_validation.h"
class PlayableQualityFrame {
 public:
 using N=NrSharedProduction;
 static constexpr uint32_t Width=1920,Height=1080,GuideWidth=1280,GuideHeight=720;
 using Evaluate=int(*)(void*,void*,void*,void*,void*,void*,void*,unsigned,unsigned,unsigned,unsigned,int,int,float,int,float,float,float,int,float,float);
 struct AppliedFrame {
  uint64_t evaluate_calls=0,resolve_calls=0;
  int evaluate_result=0;bool reset=true,skip=false,diagnostic=false;
  uint32_t expected_GPU_serial=0;
  double evaluate_CPU_call_ms=0;
 };
 struct DiagnosticReadbacks {
  N::Buffer raw{},source{},output{},depth{},motion{};
 };
 private:
 N&n;
 N::Image proxy{},raw{},keep{},dummy{};
 N::Buffer encode_cb{},resolve_cb{};
 std::unique_ptr<C3GpuValidation> validator;
 DiagnosticReadbacks readbacks{};
 VkQueryPool timestamps{};
 const bool diagnostics_allocated;
 const uint32_t timestamp_bits;
 const double timestamp_period;
 const uint32_t query_count;
 AppliedFrame frame{};bool recording=false,recorded=false;
 template<class T>T f(const char*name){return n.api<T>(name);}
 void stamp(unsigned index){if(timestamps)f<PFN_vkCmdWriteTimestamp>("vkCmdWriteTimestamp")(n.command,index==0?VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT:VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,timestamps,index);}
 static void image_contract(const N::Image&i,VkFormat format,unsigned w,unsigned h){const auto&v=i.ngx.Resource.ImageViewInfo;require(i.handle&&i.view&&i.format==format&&v.Width==w&&v.Height==h,"actual Quality per-role image extent/format");}
 public:
 explicit PlayableQualityFrame(N&production,bool allocate_diagnostics,uint32_t valid_bits,double period)
 :n(production),diagnostics_allocated(allocate_diagnostics),timestamp_bits(valid_bits),timestamp_period(period),query_count(allocate_diagnostics?8u:6u){
  require(n.safe&&n.pipe&&n.frameFence&&valid_bits<=64,"cold pipeline/fence safe before private allocation");
  proxy=n.image(VK_FORMAT_R32G32B32A32_SFLOAT,false);raw=n.image(VK_FORMAT_R16G16B16A16_SFLOAT,true);
  keep=n.image(VK_FORMAT_R32G32B32A32_SFLOAT,true);dummy=n.image(VK_FORMAT_R32G32B32A32_SFLOAT,true);
  encode_cb=n.buffer(sizeof(DlssNrConstants),VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);resolve_cb=n.buffer(sizeof(DlssNrConstants),VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
  if(diagnostics_allocated){
   readbacks.raw=n.buffer(16588800,VK_BUFFER_USAGE_TRANSFER_DST_BIT);readbacks.source=n.buffer(33177600,VK_BUFFER_USAGE_TRANSFER_DST_BIT);readbacks.output=n.buffer(33177600,VK_BUFFER_USAGE_TRANSFER_DST_BIT);
   readbacks.depth=n.buffer(3686400,VK_BUFFER_USAGE_TRANSFER_DST_BIT);readbacks.motion=n.buffer(7372800,VK_BUFFER_USAGE_TRANSFER_DST_BIT);validator=std::make_unique<C3GpuValidation>(n);
  }
  if(timestamp_bits){require(std::isfinite(period)&&period>0,"actual timestampPeriod");VkQueryPoolCreateInfo q{VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};q.queryType=VK_QUERY_TYPE_TIMESTAMP;q.queryCount=query_count;check(f<PFN_vkCreateQueryPool>("vkCreateQueryPool")(n.device,&q,nullptr,&timestamps),"playable persistent query pool");}
 }
 // Call after actual prior fence and command Reset/Begin, before EXTERNAL acquire.
 void begin(bool reset,bool skip,bool diagnostic){
  require(n.safe&&!recording&&!recorded,"prior completed frame before descriptor/query reset");require(!diagnostic||diagnostics_allocated,"diagnostic capability must be cold allocated");
  // Timeline shape stays stable for this resource set; hot does not allocate diagnostic resources.
  require(diagnostic==diagnostics_allocated,"execution diagnostic mode fixed per helper resource set");
  n.reset_frame_descriptors();frame={};frame.reset=reset;frame.skip=skip;frame.diagnostic=diagnostic;frame.expected_GPU_serial=n.serial+1;recording=true;
  if(timestamps)f<PFN_vkCmdResetQueryPool>("vkCmdResetQueryPool")(n.command,timestamps,0,query_count);stamp(0);
 }
 void record(N::Image&color,N::Image&depth,N::Image&motion,N::Image&output,void*feature,void*params,Evaluate evaluate){
  require(recording&&!recorded&&n.safe&&feature&&params&&evaluate,"live feature and prior fence before NR record");
  image_contract(color,VK_FORMAT_R32G32B32A32_SFLOAT,Width,Height);image_contract(output,VK_FORMAT_R32G32B32A32_SFLOAT,Width,Height);
  image_contract(depth,VK_FORMAT_R32_SFLOAT,GuideWidth,GuideHeight);image_contract(motion,VK_FORMAT_R32G32_SFLOAT,GuideWidth,GuideHeight);
  require(color.handle!=output.handle,"immutable source and independent resolved output");
  n.transition(color,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);n.transition(depth,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);n.transition(motion,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  n.clear(raw,NAN);n.clear(output,-1234.f);n.transition(proxy,VK_IMAGE_LAYOUT_GENERAL);n.transition(keep,VK_IMAGE_LAYOUT_GENERAL);n.transition(dummy,VK_IMAGE_LAYOUT_GENERAL);stamp(1);
  DlssNrConstants c{};c.Mode=DlssNrMode_Encode;c.Width=Width;c.Height=Height;c.GuideWidth=GuideWidth;c.GuideHeight=GuideHeight;c.WhitePoint=1;c.TransferStrength=c.ColourStrength=1;c.MaxRatio=2;c.Transfer=1;
  n.dispatch(c,color,color,color,color,proxy,keep,&encode_cb);n.transition(proxy,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);stamp(2);
  if(!frame.skip){++frame.evaluate_calls;const auto start=c4_cpu_now_ms();frame.evaluate_result=evaluate(n.command,feature,params,&proxy.ngx,&depth.ngx,&motion.ngx,&raw.ngx,Width,Height,GuideWidth,GuideHeight,0,int(frame.reset),1,0,1,1,1,1,1,1);frame.evaluate_CPU_call_ms=c4_cpu_now_ms()-start;}
  stamp(3);n.memoryBarrier();
  if(!frame.skip&&frame.evaluate_result==1){n.transition(raw,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);n.transition(keep,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);c.Mode=DlssNrMode_Resolve;n.dispatch(c,proxy,raw,keep,motion,output,dummy,&resolve_cb);++frame.resolve_calls;}
  stamp(4);
  if(frame.diagnostic){validator->record(color,raw,output);stamp(5);n.read(raw,true,&readbacks.raw);n.read(color,false,&readbacks.source);n.read(output,false,&readbacks.output);n.read_guide(depth,readbacks.depth);n.read_guide(motion,readbacks.motion);stamp(6);}
  n.transition(color,VK_IMAGE_LAYOUT_GENERAL);n.transition(depth,VK_IMAGE_LAYOUT_GENERAL);n.transition(motion,VK_IMAGE_LAYOUT_GENERAL);n.transition(output,VK_IMAGE_LAYOUT_GENERAL);recorded=true;
 }
 // Call only AFTER caller's EXTERNAL release of all four shared images, BEFORE actual submit.
 void finish_recording(){require(recording&&recorded,"complete NR recording before release stamp");stamp(diagnostics_allocated?7:5);recording=false;}
 // The caller must have performed the actual submit + <=10s GPU fence, not just API recording.
 AppliedFrame completed(){require(!recording&&recorded&&n.safe&&n.serial==frame.expected_GPU_serial,"current exact actual GPU fence before complete");recorded=false;return frame;}
 std::array<uint32_t,16> diagnostic_counters(){require(diagnostics_allocated&&validator&&n.safe&&n.serial==frame.expected_GPU_serial&&!recording,"diagnostic counts after actual current fence");return validator->read();}
 const DiagnosticReadbacks&diagnostic_buffers()const{require(diagnostics_allocated&&n.safe&&n.serial==frame.expected_GPU_serial&&!recording,"full readbacks only current completed diagnostic frame");return readbacks;}
 Json gpu_times(){
  require(n.safe&&!recording&&n.serial==frame.expected_GPU_serial,"timestamp read only after current GPU fence");
  Json j{{"available",false},{"status","NOT_AVAILABLE"},{"timestamp_valid_bits",timestamp_bits},{"timestamp_period_ns",timestamp_period},{"wrap_handling","unsigned delta masked to timestampValidBits"},{"scope","same Helper queue commands/dependencies; Evaluate interval is not pure model benchmark"},{"query_count",query_count},{"query_readback_bytes",timestamps?query_count*16u:0u},{"wait_bit",false}};
  const std::vector<const char*>names=diagnostics_allocated?std::vector<const char*>{"acquire_poison_ms","encode_ms","evaluate_commands_ms","resolve_ms","validation_ms","readbacks_ms","release_ms"}:std::vector<const char*>{"acquire_poison_ms","encode_ms","evaluate_commands_ms","resolve_ms","release_ms"};
  for(auto name:names)j[name]=nullptr;j["total_ms"]=nullptr;if(!timestamps)return j;
  std::array<std::array<uint64_t,2>,8>q{};const auto result=f<PFN_vkGetQueryPoolResults>("vkGetQueryPoolResults")(n.device,timestamps,0,query_count,query_count*16,q.data(),16,VK_QUERY_RESULT_64_BIT|VK_QUERY_RESULT_WITH_AVAILABILITY_BIT);j["vk_result"]=int(result);
  Json availability=Json::array();bool ready=result==VK_SUCCESS;for(unsigned i=0;i<query_count;i++){availability.push_back(q[i][1]!=0);ready=ready&&q[i][1]!=0;}j["query_availability"]=availability;if(!ready)return j;
  const uint64_t mask=timestamp_bits==64?UINT64_MAX:((uint64_t(1)<<timestamp_bits)-1);for(unsigned i=0;i<names.size();i++)j[names[i]]=double((q[i+1][0]-q[i][0])&mask)*timestamp_period/1e6;
  j["total_ms"]=double((q[query_count-1][0]-q[0][0])&mask)*timestamp_period/1e6;j["available"]=true;j["status"]="AVAILABLE";return j;
 }
 bool hot()const{return !diagnostics_allocated;}
 uint64_t pixel_readback_bytes()const{return diagnostics_allocated?94003200ull:0ull;}
 uint64_t validator_readback_bytes()const{return diagnostics_allocated?64:0;}
 ~PlayableQualityFrame(){if(!n.safe){validator.release();return;}validator.reset();if(timestamps)f<PFN_vkDestroyQueryPool>("vkDestroyQueryPool")(n.device,timestamps,nullptr);}
};
