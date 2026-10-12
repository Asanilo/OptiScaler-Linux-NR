// SPDX-License-Identifier: GPL-3.0-or-later
// Standalone Q2 controlled behavior fixture; execution PASS is not MV semantic/quality PASS.
#include "q2_common.h"
#include "q2_case.h"
#include "q2_vulkan_context.h"
#include "q2_production_adapter.h"
#include <chrono>
struct Fixture:Q2Context {
 using N=Q2Production;std::unique_ptr<N> nr;
 HMODULE dxgi{},nvapi{},fw{},core{};NVSDK_NGX_Parameter*params{};void*feature{};
 std::string dir;Json spec,result;unsigned creates=0,releases=0,evaluates=0,resolves=0;
 double started=0,frame_started=0;bool cold_finished=false,normal_shutdown=false;
 N::Image color{},depth{},motion{},output{},proxy{},raw{},keep{},dummy{};
 N::Buffer color_upload{},depth_upload{},mv_upload{},source_read{},depth_read{},mv_read{},raw_read{},out_read{},encode_cb{},resolve_cb{};
 unsigned gw=0,gh=0;float sx=1,sy=1;
 ~Fixture(){if(!normal_shutdown)nr.release();} // HOLD private resources on every abnormal path, including unknown GPU completion.
 void total_check(){require(c4_cpu_now_ms()-started<300000,"outer300s local watchdog",3);}
 void cold_check(const char*phase){const auto elapsed=c4_cpu_now_ms()-started;printf("COLD_PHASE phase=%s elapsed_ms=%.6f budget_ms=120000\n",phase,elapsed);require(elapsed<120000,"cold120s watchdog",3);total_check();}
 void frame_check(const char*phase){const auto elapsed=c4_cpu_now_ms()-frame_started;printf("FRAME_PHASE phase=%s elapsed_ms=%.6f budget_ms=30000\n",phase,elapsed);require(elapsed<30000,"frame30s watchdog",3);total_check();}
 void persist(){result["feature_create_calls"]=creates;result["feature_release_calls"]=releases;result["evaluate_calls"]=evaluates;result["resolve_calls"]=resolves;result["normal_shutdown"]=normal_shutdown;atomic_text(dir+"\\run-result.json",result.dump(2)+"\n");}
 void init_model(){
  // Actual NVAPI trace requires DXVK's Vulkan-aware DXGI factory; no D3D DLL replacement.
  SetLastError(ERROR_SUCCESS);
  dxgi=LoadLibraryExW(wide(dxgi_path).c_str(),nullptr,LOAD_WITH_ALTERED_SEARCH_PATH);
  const DWORD dxgi_load_error=GetLastError();
  printf("DXGI_LOAD requested=%s flags=LOAD_WITH_ALTERED_SEARCH_PATH handle=%p winerror=%lu before_NVAPI_core=1 DLL_copy=0\n",dxgi_path.c_str(),dxgi,(unsigned long)dxgi_load_error);
  require(dxgi,"pinned installed DXVK DXGI PE load",3);
  wchar_t dxgi_actual_wide[32768]{};SetLastError(ERROR_SUCCESS);
  const DWORD dxgi_actual_length=GetModuleFileNameW(dxgi,dxgi_actual_wide,32768),dxgi_path_error=GetLastError();
  require(dxgi_actual_length>0&&dxgi_actual_length<32768,"DXGI actual module path",3);
  const std::string dxgi_actual(dxgi_actual_wide,dxgi_actual_wide+dxgi_actual_length);
  const bool dxgi_path_matches=_stricmp(dxgi_actual.c_str(),dxgi_path.c_str())==0;
  printf("DXGI_MODULE actual=%s requested_path_match=%u winerror=%lu held_until_after_core_and_NVAPI_release=1\n",dxgi_actual.c_str(),dxgi_path_matches,(unsigned long)dxgi_path_error);
  require(dxgi_path_matches,"DXGI module differs from pinned absolute path",3);
  // Single availability adaptation: installed pinned PE only; no NVAPI function calls.
  SetLastError(ERROR_SUCCESS);
  nvapi=LoadLibraryExW(wide(nvapi_path).c_str(),nullptr,LOAD_WITH_ALTERED_SEARCH_PATH);
  const DWORD load_error=GetLastError();
  printf("NVAPI_LOAD requested=%s flags=LOAD_WITH_ALTERED_SEARCH_PATH handle=%p winerror=%lu private_NVAPI_calls=0 DLL_copy=0\n",nvapi_path.c_str(),nvapi,(unsigned long)load_error);
  require(nvapi,"pinned installed NVAPI PE load",3);
  wchar_t actual_wide[32768]{};SetLastError(ERROR_SUCCESS);
  const DWORD actual_length=GetModuleFileNameW(nvapi,actual_wide,32768),path_error=GetLastError();
  require(actual_length>0&&actual_length<32768,"NVAPI actual module path",3);
  const std::string actual(actual_wide,actual_wide+actual_length);
  const bool path_matches=_stricmp(actual.c_str(),nvapi_path.c_str())==0;
  printf("NVAPI_MODULE actual=%s requested_path_match=%u winerror=%lu\n",actual.c_str(),path_matches,(unsigned long)path_error);
  require(path_matches,"NVAPI module differs from pinned absolute path",3);
  SetLastError(ERROR_SUCCESS);const auto query_interface=GetProcAddress(nvapi,"nvapi_QueryInterface");const DWORD symbol_error=GetLastError();
  printf("NVAPI_EXPORT nvapi_QueryInterface=%p winerror=%lu called=0 held_until_after_core_shutdown=1\n",reinterpret_cast<void*>(query_interface),(unsigned long)symbol_error);
  require(query_interface,"installed NVAPI public export unavailable",3);
  fw=LoadLibraryW(wide(forwarder_path).c_str());core=LoadLibraryW(wide(core_path).c_str());require(fw&&core,"pinned PE core/forwarder load",3);
  printf("RUNTIME model=%s core=%s forwarder=%s\n",model_path.c_str(),module(core).c_str(),module(fw).c_str());
  int mask=get<int(*)(const wchar_t*)>(fw,"dlssnr_vk_probe")(wide(model_path).c_str());printf("MODEL_VK_EXPORT_MASK %d expected=15\n",mask);require(mask==15,"NR model real exports",3);
  cold_check("PE_runtime_loaded");
  auto ir=get<decltype(&NVSDK_NGX_VULKAN_Init_Ext)>(core,"NVSDK_NGX_VULKAN_Init_Ext")(0x24480451ull,wide(dir).c_str(),instance,physical,device,NVSDK_NGX_Version_API,nullptr);
  cold_check("core_init_returned");
  printf("CORE_VK_INIT result=0x%08x\n",(unsigned)ir);require(ir==NVSDK_NGX_Result_Success,"core init");
  auto caps=get<decltype(&NVSDK_NGX_VULKAN_GetCapabilityParameters)>(core,"NVSDK_NGX_VULKAN_GetCapabilityParameters")(&params);
  printf("VK_CAPABILITY result=0x%08x ptr=%p\n",(unsigned)caps,params);require(caps==NVSDK_NGX_Result_Success&&params,"core caps");
  require(nr_discover_float(params,get<void(*)(void*,const char*,float,int)>(fw,"dlssnr_call_probe_float"),get<void(*)(int)>(fw,"dlssnr_call_set_float_slot")),"safe parameter float ABI");
  cold_check("caps_safe_parameter_abi");
  int mr=get<int(*)(const wchar_t*,const wchar_t*,void*,void*,void*,int)>(fw,"dlssnr_vk_init")(wide(model_path).c_str(),wide(dir).c_str(),instance,physical,device,0x15);
  printf("MODEL_VK_INIT result=0x%08x\n",mr);require(mr==1,"model init");

  cold_check("model_init");
 }
 void init_resources(){
  for(auto f:{VK_FORMAT_R32G32B32A32_SFLOAT,VK_FORMAT_R16G16B16A16_SFLOAT})query(f,1920,1080);
  query(VK_FORMAT_R32_SFLOAT,gw,gh);query(VK_FORMAT_R32G32_SFLOAT,gw,gh);
  nr=std::make_unique<N>(instance,physical,device,cmd,queue,gipa);nr->gpu_timeout_ms=10000;nr->shader();
  color=nr->image(VK_FORMAT_R32G32B32A32_SFLOAT,false);output=nr->image(VK_FORMAT_R32G32B32A32_SFLOAT,true);
  depth=nr->image(VK_FORMAT_R32_SFLOAT,false,gw,gh);motion=nr->image(VK_FORMAT_R32G32_SFLOAT,false,gw,gh);
  proxy=nr->image(VK_FORMAT_R32G32B32A32_SFLOAT,false);raw=nr->image(VK_FORMAT_R16G16B16A16_SFLOAT,true);
  keep=nr->image(VK_FORMAT_R32G32B32A32_SFLOAT,true);dummy=nr->image(VK_FORMAT_R32G32B32A32_SFLOAT,true);
  color_upload=nr->buffer(33177600,VK_BUFFER_USAGE_TRANSFER_SRC_BIT);depth_upload=nr->buffer(uint64_t(gw)*gh*4,VK_BUFFER_USAGE_TRANSFER_SRC_BIT);mv_upload=nr->buffer(uint64_t(gw)*gh*8,VK_BUFFER_USAGE_TRANSFER_SRC_BIT);
  source_read=nr->buffer(33177600,VK_BUFFER_USAGE_TRANSFER_DST_BIT);depth_read=nr->buffer(uint64_t(gw)*gh*4,VK_BUFFER_USAGE_TRANSFER_DST_BIT);mv_read=nr->buffer(uint64_t(gw)*gh*8,VK_BUFFER_USAGE_TRANSFER_DST_BIT);
  raw_read=nr->buffer(16588800,VK_BUFFER_USAGE_TRANSFER_DST_BIT);out_read=nr->buffer(33177600,VK_BUFFER_USAGE_TRANSFER_DST_BIT);
  encode_cb=nr->buffer(sizeof(DlssNrConstants),VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);resolve_cb=nr->buffer(sizeof(DlssNrConstants),VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
  cold_check("owned_resources_and_production_pipeline");
 }
 void create_feature(unsigned ordinal,Json&br){
  require(!feature&&nr&&nr->safe&&!in_flight,"prior feature released and actual GPU complete before create");begin();
  using Create=void*(*)(void*,void*,unsigned,unsigned,int,float,int,float,float,float,int,int);
  ++creates;feature=get<Create>(fw,"dlssnr_vk_create")(cmd,params,1920,1080,0,1,0,1,1,1,1,0);
  auto*last=reinterpret_cast<int*>(GetProcAddress(fw,"dlssnr_vk_last_create"));int cr=last?*last:0;
  br["feature_ordinal"]=ordinal;br["feature_handle"]=uint64_t(reinterpret_cast<uintptr_t>(feature));br["create_result"]=cr;br["fresh_feature_created"]=feature&&cr==1;
  printf("MODEL_CREATE ordinal=%u branch=%s call=%u handle=%p result=0x%08x model=1920,1080 guides=%u,%u prior_released=1\n",ordinal,br["branch_id"].get<std::string>().c_str(),creates,feature,cr,gw,gh);
  submit();br["creation_GPU_complete"]=true;br["creation_fence_serial"]=serial;
  require(feature&&last&&cr==1,"new NR feature create/result after real GPU completion");
  if(ordinal==1){cold_check("first_feature_create_GPU_complete");cold_finished=true;}else total_check();persist();
 }
 std::string load_asset(const Json&a,const char*role){
  const auto path=a["windows_path"].get<std::string>();const size_t n=a["bytes"].get<size_t>();
  auto data=file_read(path,n,33177600);require(data.size()%4==0,"float asset alignment");
  for(size_t i=0;i<data.size();i+=4){float f;memcpy(&f,data.data()+i,4);require(std::isfinite(f),"input asset finite");if(!strcmp(role,"depth"))require(f>=0&&f<=1,"device-Z input asset range");}
  printf("ASSET_LOADED role=%s path=%s bytes=%zu SHA256_ROOT_PIN=%s helper_SHA=NOT_RUN oracle_asset_loaded=0\n",role,path.c_str(),n,a["sha256"].get<std::string>().c_str());return data;
 }
 void frame(const Json&request,Json&fr,const std::string&branch,unsigned ordinal){
  frame_started=c4_cpu_now_ms();const unsigned f=request["frame"].get<unsigned>(),reset=request["reset"].get<unsigned>();
  fr=Json{{"frame",f},{"reset",reset},{"evaluate_calls",0},{"evaluate_result",0},{"resolve_calls",0},{"completed_GPU",false},{"upload_readback_equal",false},{"raw_poison","NaN"},{"output_poison",-1234.0}};
  auto source=load_asset(request["source"],"source"),dep=load_asset(request["depth"],"depth"),mv=load_asset(request["mv"],"mv");frame_check("assets_loaded");
  require(nr->safe&&!in_flight,"prior actual frame fence before upload/reuse");begin();nr->reset_frame_descriptors();
  nr->upload(color,color_upload,source.data(),source.size());nr->upload(depth,depth_upload,dep.data(),dep.size());nr->upload(motion,mv_upload,mv.data(),mv.size());
  nr->read(color,false,&source_read);nr->read_guide(depth,depth_read);nr->read_guide(motion,mv_read);
  nr->transition(color,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);nr->transition(depth,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);nr->transition(motion,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  nr->submit();frame_check("independent_upload_GPU_complete");
  const bool ce=memcmp(source.data(),source_read.mapped,source.size())==0,de=memcmp(dep.data(),depth_read.mapped,dep.size())==0,me=memcmp(mv.data(),mv_read.mapped,mv.size())==0;
  fr["upload_readback_equal"]=ce&&de&&me;fr["upload_readbacks"]=Json{{"source",ce},{"depth",de},{"mv",me},{"independent_GPU_fence_serial",nr->serial}};
  const std::string prefix=branch+"/frame-"+std::to_string(f);
  fr["source_readback"]=prefix+"-source.rgba32f";fr["depth_readback"]=prefix+"-depth.r32f";fr["mv_readback"]=prefix+"-mv.rg32f";fr["raw_readback"]=prefix+"-raw.rgba16f";fr["output_readback"]=prefix+"-output.rgba32f";
  dump_bytes(dir+"/"+fr["source_readback"].get<std::string>(),source_read.mapped,size_t(source_read.bytes));dump_bytes(dir+"/"+fr["depth_readback"].get<std::string>(),depth_read.mapped,size_t(depth_read.bytes));dump_bytes(dir+"/"+fr["mv_readback"].get<std::string>(),mv_read.mapped,size_t(mv_read.bytes));
  printf("UPLOAD_READBACK branch=%s frame=%u source_equal=%u depth_equal=%u mv_equal=%u real_GPU_fence=1\n",branch.c_str(),f,ce,de,me);persist();require(ce&&de&&me,"GPU input upload/readback byte mismatch");frame_check("input_readback_dumped");
  begin();nr->reset_frame_descriptors();nr->clear(raw,NAN);nr->clear(output,-1234.f);nr->transition(proxy,VK_IMAGE_LAYOUT_GENERAL);nr->transition(keep,VK_IMAGE_LAYOUT_GENERAL);nr->transition(dummy,VK_IMAGE_LAYOUT_GENERAL);
  printf("FRAME_PREPOISON ordinal=%u branch=%s frame=%u raw97=NaN resolved109=-1234 passes=1 WorkingScale=1 SR=0 FG=0\n",ordinal,branch.c_str(),f);
  DlssNrConstants c{};c.Mode=DlssNrMode_Encode;c.Width=1920;c.Height=1080;c.GuideWidth=gw;c.GuideHeight=gh;c.WhitePoint=1;c.TransferStrength=c.ColourStrength=1;c.MaxRatio=2;c.Transfer=1;
  nr->dispatch(c,color,color,color,color,proxy,keep,&encode_cb);nr->transition(proxy,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);frame_check("encode_recorded");
  using Eval=int(*)(void*,void*,void*,void*,void*,void*,void*,unsigned,unsigned,unsigned,unsigned,int,int,float,int,float,float,float,int,float,float);
  ++evaluates;fr["evaluate_calls"]=1;const auto evalstart=c4_cpu_now_ms();const int er=get<Eval>(fw,"dlssnr_vk_evaluate")(cmd,feature,params,&proxy.ngx,&depth.ngx,&motion.ngx,&raw.ngx,1920,1080,gw,gh,0,int(reset),1,0,1,1,1,1,sx,sy);
  fr["evaluate_result"]=er;fr["evaluate_CPU_call_ms"]=c4_cpu_now_ms()-evalstart;fr["evaluate_parameters"]=Json{{"width",1920},{"height",1080},{"guide_width",gw},{"guide_height",gh},{"depth_inverted",0},{"reset",reset},{"intensity",1.0},{"style",0},{"local_structure",1.0},{"local_tone",1.0},{"skin_structure",1.0},{"use_auto_mask",1},{"mvec_scale_x",sx},{"mvec_scale_y",sy},{"enabled",1},{"jitter_setter","NONE"}};
  fr["parameter_application"]=result["parameter_application"];
  fr["evaluate_parameters"]["resources"]=Json{{"color",{{"role","encoded_proxy"},{"format",109},{"extent",Json::array({1920,1080})}}},{"depth",{{"role","uploaded_depth"},{"format",100},{"extent",Json::array({gw,gh})}}},{"mvec",{{"role","uploaded_mv"},{"format",103},{"extent",Json::array({gw,gh})}}},{"output",{{"role","model_raw"},{"format",97},{"extent",Json::array({1920,1080})}}}};
  fr["evaluate_parameters"]["subrects"]=Json{{"color",Json::array({0,0,1920,1080})},{"output",Json::array({0,0,1920,1080})},{"depth",Json::array({0,0,gw,gh})},{"mvec",Json::array({0,0,gw,gh})}};
  printf("MODEL_EVALUATE ordinal=%u branch=%s frame=%u reset=%u calls=1 total=%u result=0x%08x model=1920,1080 guides=%u,%u MVecScale=%g,%g DepthInverted=0 Intensity=1 Style=0 LocalStructure=1 LocalTone=1 SkinStructure=1 UseAutoMask=1 resources=proxy109/depth100/mvec103/raw97 subrect_bases=0,0 frame_ms/exposure/pre_exposure=HOST_FIXTURE_DECLARATION_NOT_SENT_TO_NR_API runtime_default_values=UNVERIFIED extra_MV_multiplier=NONE jitter_setter=NONE\n",ordinal,branch.c_str(),f,reset,evaluates,er,gw,gh,sx,sy);frame_check("evaluate_returned");nr->memoryBarrier();
  if(er==1){nr->transition(raw,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);nr->transition(keep,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);c.Mode=DlssNrMode_Resolve;nr->dispatch(c,proxy,raw,keep,motion,output,dummy,&resolve_cb);++resolves;fr["resolve_calls"]=1;printf("MODEL_RESOLVE ordinal=%u branch=%s frame=%u calls=1 total=%u shader=production_CSMain groups=240,135\n",ordinal,branch.c_str(),f,resolves);}
  nr->read(raw,true,&raw_read);nr->read(output,false,&out_read);nr->submit();fr["completed_GPU"]=true;fr["GPU_fence_serial"]=nr->serial;fr["GPU_fence_wait_CPU_ms"]=nr->fence_wait_cpu_ms;frame_check("model_resolve_GPU_complete");
  dump_bytes(dir+"/"+fr["raw_readback"].get<std::string>(),raw_read.mapped,size_t(raw_read.bytes));dump_bytes(dir+"/"+fr["output_readback"].get<std::string>(),out_read.mapped,size_t(out_read.bytes));
  uint64_t rawfinite=0,rawWritten=0,outfinite=0,outwritten=0,alpha=0;const auto*rb=static_cast<const uint16_t*>(raw_read.mapped);const auto*ob=static_cast<const float*>(out_read.mapped);const auto*src=static_cast<const uint32_t*>(source_read.mapped);const auto*ow=static_cast<const uint32_t*>(out_read.mapped);
  for(unsigned p=0;p<1920*1080;p++){for(unsigned channel=0;channel<4;channel++){unsigned i=p*4+channel;if(channel<3){float v=N::unhalf(rb[i]);rawfinite+=std::isfinite(v);rawWritten+=std::isfinite(v);}outfinite+=std::isfinite(ob[i]);outwritten+=std::isfinite(ob[i])&&ob[i]!=-1234.f;}alpha+=src[p*4+3]==ow[p*4+3];}
  fr["raw_rgb_finite"]=rawfinite;fr["raw_rgb_written"]=rawWritten;fr["raw_rgb_total"]=6220800;fr["resolved_rgba_finite"]=outfinite;fr["resolved_rgba_written"]=outwritten;fr["resolved_rgba_total"]=8294400;fr["alpha_preserved"]=alpha;fr["alpha_total"]=2073600;
  printf("READBACK ordinal=%u branch=%s frame=%u raw_RGB_finite=%llu raw_written=%llu/6220800 resolved_RGBA_finite=%llu resolved_written=%llu/8294400 alpha_bit_preserved=%llu/2073600\n",ordinal,branch.c_str(),f,(unsigned long long)rawfinite,(unsigned long long)rawWritten,(unsigned long long)outfinite,(unsigned long long)outwritten,(unsigned long long)alpha);
  fr["frame_elapsed_QPC_ms"]=c4_cpu_now_ms()-frame_started;persist();frame_check("full_statistics_and_dumps_complete");require(er==1&&rawfinite==6220800&&rawWritten==6220800&&outfinite==8294400&&outwritten==8294400&&alpha==2073600,"actual Evaluate/raw poison/output finite/write/alpha gates");fr["status"]="PASS";persist();
 }
 void release_feature(Json&br){require(feature&&nr->safe&&!in_flight,"last completed GPU before feature release");br["last_GPU_complete_before_release"]=true;br["last_GPU_fence_serial"]=nr->serial;get<void(*)(void*)>(fw,"dlssnr_vk_release")(feature);feature=nullptr;++releases;br["feature_released"]=true;printf("FEATURE_RELEASE ordinal=%llu calls=%u prior_GPU_complete=1\n",(unsigned long long)br["feature_ordinal"].get<uint64_t>(),releases);persist();}
 void shutdown(){
  require(!feature&&nr&&nr->safe&&!in_flight&&creates==5&&releases==5,"shutdown actual feature/GPU lifecycle gates");nr.reset();puts("TEARDOWN private_production_fixture released");
  int sr=get<int(*)(const wchar_t*,void*)>(fw,"nr_poc_vk_shutdown")(wide(model_path).c_str(),device);printf("TEARDOWN model_Shutdown1 result=0x%08x\n",sr);require(sr==1,"model Shutdown1");
  auto dr=get<decltype(&NVSDK_NGX_VULKAN_DestroyParameters)>(core,"NVSDK_NGX_VULKAN_DestroyParameters")(params);printf("TEARDOWN core_DestroyParameters result=0x%08x\n",(unsigned)dr);require(dr==NVSDK_NGX_Result_Success,"DestroyParameters");params=nullptr;
  using CoreShutdown=NVSDK_NGX_Result(NVSDK_CONV*)(void);auto cs=get<CoreShutdown>(core,"NVSDK_NGX_VULKAN_Shutdown")();printf("TEARDOWN core_Shutdown result=0x%08x\n",(unsigned)cs);require(cs==NVSDK_NGX_Result_Success,"public core Shutdown");
  require(FreeLibrary(fw)&&FreeLibrary(core),"PE FreeLibrary");fw=core=nullptr;
  BOOL n=FreeLibrary(nvapi);printf("TEARDOWN NVAPI_FreeLibrary result=%u after_core_shutdown=1\n",unsigned(n));require(n,"NVAPI FreeLibrary");nvapi=nullptr;BOOL x=FreeLibrary(dxgi);printf("TEARDOWN DXGI_FreeLibrary result=%u after_core_shutdown_and_NVAPI=1\n",unsigned(x));require(x,"DXGI FreeLibrary");dxgi=nullptr;
  D(vkDestroyFence)(device,fence,nullptr);D(vkDestroyCommandPool)(device,pool,nullptr);D(vkDestroyDevice)(device,nullptr);device=nullptr;I(vkDestroyInstance)(instance,nullptr);instance=nullptr;require(FreeLibrary(loader),"Vulkan loader FreeLibrary");loader=nullptr;normal_shutdown=true;puts("NORMAL_SHUTDOWN features_create=5 features_release=5 model=1 core=1 device=1");
 }
 void run(){
  gw=spec["guide_extent"][0].get<unsigned>();gh=spec["guide_extent"][1].get<unsigned>();sx=spec["nr_mvec_scale"][0].get<float>();sy=spec["nr_mvec_scale"][1].get<float>();
  result=Json{{"schema",spec["schema"]},{"case_id",spec["case_id"]},{"status","RUNNING"},{"model_extent",spec["model_extent"]},{"guide_extent",spec["guide_extent"]},{"phase",spec["phase"]},{"nr_mvec_scale",spec["nr_mvec_scale"]},{"model_parameters",spec["model_parameters"]},{"budgets_ms",spec["budgets_ms"]},{"frame_budget_ms",30000},{"branches",Json::array()},{"normal_shutdown",false},{"scope","execution_and_functional_ONLY_not_semantics_quality_or_shared_transport"}};
  // Request echo is not a claim these declarations were applied. The fixed Evaluate ABI has no such args/setters.
  const char*declaration="HOST_FIXTURE_DECLARATION_NOT_SENT_TO_NR_API";
  result["parameter_application"]=Json{{"frame_ms",declaration},{"exposure",declaration},{"pre_exposure",declaration},{"runtime_default_values","UNVERIFIED"}};
  result["model_parameters_scope"]="REQUEST_ECHO_ONLY_application_status_is_parameter_application_actual_Evaluate_args_are_per_frame";
  persist();
  init_nr_device();result["device_uuid"]=hex(ids.deviceUUID);result["driver_uuid"]=hex(ids.driverUUID);cold_check("device_init");init_model();init_resources();
  for(unsigned b=0;b<5;b++){
   const auto&req=spec["branches"][b];const auto name=req["branch_id"].get<std::string>();require(CreateDirectoryA((dir+"/"+name).c_str(),nullptr),"new branch output directory",3);
   result["branches"].push_back(Json{{"branch_id",name},{"frames",Json::array()},{"fresh_feature_created",false},{"creation_GPU_complete",false},{"last_GPU_complete_before_release",false},{"feature_released",false}});auto&br=result["branches"][b];create_feature(b+1,br);
   for(unsigned f=0;f<2;f++){br["frames"].push_back(Json::object());frame(req["frames"][f],br["frames"][f],name,b+1);}release_feature(br);
  }
  require(evaluates==10&&resolves==10,"exact Evaluate10 Resolve10");shutdown();total_check();result["status"]="PASS";result["elapsed_QPC_ms"]=c4_cpu_now_ms()-started;persist();
 }
};
int main(int argc,char**argv){
 if(argc!=3){fputs("Usage: wine_q2_motion_fixture.exe RUN_DIR_WINDOWS CASE_JSON_WINDOWS\n",stderr);return 2;}
 Fixture c;c.dir=argv[1];c.started=c4_cpu_now_ms();
 try{
  DWORD attr=GetFileAttributesA(c.dir.c_str());require(attr!=INVALID_FILE_ATTRIBUTES&&(attr&FILE_ATTRIBUTE_DIRECTORY)&&!(attr&FILE_ATTRIBUTE_REPARSE_POINT),"unique existing regular RUN_DIR",3);require(GetFileAttributesA((c.dir+"\\run-result.json").c_str())==INVALID_FILE_ATTRIBUTES,"RUN_DIR not previously used",3);
  FILE*log=freopen((c.dir+"\\q2-fixture.log").c_str(),"wb",stdout);require(log,"fixture log",3);setvbuf(stdout,nullptr,_IONBF,0);
  c.spec=q2::parse(file_read(argv[2],0,1<<20));q2::validate(c.spec);printf("Q2_CASE case_id=%s schema=q2-known-translation-v1 feature_branches=5 frames_per_branch=2 local_upload=1 shared_transport=NOT_RUN oracle_assets_loaded=0\n",c.spec["case_id"].get<std::string>().c_str());c.run();return 0;
 }catch(const Stop&e){fprintf(stderr,"Q2_STOP code=%d reason=%s\n",e.code,e.what());printf("Q2_STOP code=%d reason=%s HOLD=%u normal_shutdown=0\n",e.code,e.what(),unsigned(c.in_flight||(c.nr&&!c.nr->safe)));try{c.result["status"]=e.code==3?"BLOCKED":"FAIL";c.result["error"]=e.what();c.result["exit_code"]=e.code;c.result["resources_held"]=!c.normal_shutdown;c.persist();}catch(...){ }return e.code;
 }catch(const std::exception&e){fprintf(stderr,"Q2_EXCEPTION reason=%s\n",e.what());printf("Q2_EXCEPTION reason=%s normal_shutdown=0\n",e.what());try{c.result["status"]="FAIL";c.result["error"]=e.what();c.result["exit_code"]=2;c.result["resources_held"]=!c.normal_shutdown;c.persist();}catch(...){ }return 2;}
}
