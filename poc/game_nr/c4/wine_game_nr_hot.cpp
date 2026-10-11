// SPDX-License-Identifier: GPL-3.0-or-later
// Isolated C4 hot/diagnostic adapter of accepted C3; production algorithm/SPIR-V unchanged.
#include "../c2-baseline/c2_game_transport.h"
#include "../c2-baseline/c2_game_control.h"
#include "nvsdk_ngx_vk.h"
#include "/home/arinp/Code/dlss5_linux/OptiScaler-Linux-NR/build_out/nr-runtime-poc-20261010/poc/nr_runtime/parameter_abi.h"
#include <memory>
using Library=HMODULE;
static void need(bool b,const char*m){require(b,m);}
static void check(VkResult r,const char*n){api(r,n);}
template<class T>static T get(Library l,const char*n){auto f=reinterpret_cast<T>(GetProcAddress(l,n));require(f,n,3);return f;}
template<class T>static T proc(PFN_vkGetInstanceProcAddr g,VkInstance i,const char*n){auto f=reinterpret_cast<T>(g(i,n));require(f,n,3);return f;}
static std::wstring wide(const std::string&s){return std::wstring(s.begin(),s.end());}
static std::string winpath(const std::string&s){std::string p="Z:"+s;std::replace(p.begin(),p.end(),'/','\\');return p;}
static double c4_cpu_now_ms(){
 static const double frequency=[](){LARGE_INTEGER f{};require(QueryPerformanceFrequency(&f)&&f.QuadPart>0,"monotonic CPU frequency");return double(f.QuadPart);}();
 LARGE_INTEGER q{};require(QueryPerformanceCounter(&q),"monotonic CPU counter");return double(q.QuadPart)*1000.0/frequency;
}
#include "c4_production_adapter.h"
#include "gpu_validation.h"
static const std::string model_path=winpath("/home/arinp/Code/dlss5_linux/dll/runtime/nvngx_dlssnr.dll");
static const std::string forwarder_path=winpath("/home/arinp/Code/dlss5_linux/OptiScaler-Linux-NR/build_out/nr-runtime-poc-20261010/build_out/poc-runtime-20261010/nvngx.dll_dlssnr.dll");
static const std::string core_path=winpath("/usr/lib64/nvidia/wine/_nvngx.dll");
static const std::string nvapi_path=winpath("/home/arinp/.local/share/Steam/compatibilitytools.d/GE-Proton11-7-x86_64/files/lib/wine/nvapi/x86_64-windows/nvapi64.dll");
static const std::string dxgi_path=winpath("/home/arinp/.local/share/Steam/compatibilitytools.d/GE-Proton11-7-x86_64/files/lib/wine/dxvk/x86_64-windows/dxgi.dll");
static std::string digest_bytes(const void*data,size_t bytes){uint64_t h=0xcbf29ce484222325ull;const auto*p=static_cast<const uint8_t*>(data);for(size_t i=0;i<bytes;i++){h^=p[i];h*=0x100000001b3ull;}char out[17];snprintf(out,sizeof out,"%016llx",(unsigned long long)h);return out;}
static void dump_bytes(const std::string&path,const void*data,size_t bytes){FILE*f=fopen(path.c_str(),"wb");require(f,"local evidence dump");bool ok=fwrite(data,1,bytes,f)==bytes;ok=(fclose(f)==0)&&ok;require(ok,"local evidence write");}

struct GameNrHelper:C2Transport {
 using N=NrSharedProduction;
 Library fw=nullptr,core=nullptr,nvapi=nullptr,dxgi=nullptr;
 NVSDK_NGX_Parameter*params=nullptr;void*feature=nullptr;
 std::unique_ptr<N> nr;std::unique_ptr<C3GpuValidation> validator;VkQueryPool timestamps{};uint32_t timestamp_bits=0;N::Image proxy{},raw{},keep{},dummy{},shared_color{},shared_depth{},shared_motion{},shared_output{};
 N::Buffer raw_read{},out_read{},source_read{},encode_cb{},resolve_cb{};
 std::string mode,execution_mode,profile="hdr-run-dlaa-1to1-j0-manual-wp1-v1",expected_device,expected_driver,input_digest;
 uint64_t source_game_frame=0,prev_source_game_frame=0,cold_start=0,frame_start=0;
 unsigned frame_count=0,frame_index=0,nr_reset=1,query_count=0;bool diagnostic=false,audit_full=false;double control_input_read_ms=0;std::string diagnostic_case,reset_reason;uint32_t input_xor=0,input_sum=0;
 uint32_t control_timeout_ms=60000,cold_init_timeout_ms=120000,frame_completion_timeout_ms=30000;
 unsigned evaluates=0,resolves=0,frames_done=0;bool negative=false,model_initialized=false;
 Json actual_frame_params=Json::object();
 static double cpu_now_ms(){return c4_cpu_now_ms();}
 const char*validation_profile()const{return diagnostic?"c4_diagnostic":"c4_hot_path";}
 Json validation_fields()const{
  if(diagnostic)return Json{{"validation_profile","c4_diagnostic"},{"audit_kind","full"},{"dump_count",0}};
  return Json{{"validation_profile","c4_hot_path"},{"audit_kind","off"},{"numerical_validation","NOT_CHECKED"},{"pixel_validation","NOT_RUN"},{"signature","NOT_RUN"},{"dump_count",0}};
 }
 void phase_tick(const char*stage){
  require(GetFileAttributesA((dir+"\\native.abort").c_str())==INVALID_FILE_ATTRIBUTES,"native abort/quarantine; no normal release",3);
  const auto elapsed=GetTickCount64()-cold_start;
  printf("COLD_PHASE stage=%s elapsed_ms=%llu budget_ms=%u synchronous_API_cancel=NOT_AVAILABLE\n",stage,(unsigned long long)elapsed,cold_init_timeout_ms);
  require(elapsed<cold_init_timeout_ms,"cold model readiness deadline expired; hold resources",3);
 }
 Json envelope(uint64_t frame,const char*op,const char*status,bool valid=false)const{
  Json j{{"protocol_version",4},{"execution_mode",execution_mode},{"session",session},{"generation",generation},{"frame",frame},{"resource_set",resource_set()},
   {"source_game_frame",source_game_frame},{"profile_id",profile},{"width",width},{"height",height},
   {"render_width",width},{"render_height",height},{"mode",mode},{"status",status},{"op",op},
   {"completed_GPU",true},{"output_valid",valid},{"model_valid",valid}};j.update(validation_fields());return j;
 }
 void publish(uint64_t f,const char*file,const char*op,const char*status,bool valid=false,bool numbered=true){
  atomic_text(dir+"\\"+name(file,f,numbered),envelope(f,op,status,valid).dump()+"\n");
 }
 Json read_control(const std::string&filename){return parse_control(read_bounded(dir,filename,control_timeout_ms));}
 void validate_tuple(const Json&j,uint64_t f,const char*op,const char*status,bool match_source=true){
  require(uint_value(j,"protocol_version")==4&&string_value(j,"execution_mode")==execution_mode,"C4 protocol/execution_mode mismatch; C3 peers rejected",3);
  require(string_value(j,"session")==session&&uint_value(j,"generation")==0&&uint_value(j,"frame")==f&&string_value(j,"resource_set")==resource_set(),"C4 exact session/generation/frame/resource_set mismatch",3);
  require(string_value(j,"profile_id")==profile&&string_value(j,"mode")==mode&&uint_value(j,"width")==1920&&uint_value(j,"height")==1080&&uint_value(j,"render_width")==1920&&uint_value(j,"render_height")==1080,"C4 profile/mode/extents mismatch",3);
  require(string_value(j,"op")==op&&string_value(j,"status")==status&&bool_value(j,"completed_GPU")&&!bool_value(j,"output_valid")&&!bool_value(j,"model_valid"),"C4 control op/status/GPU/validity mismatch",3);
  if(match_source)require(uint_value(j,"source_game_frame")==source_game_frame,"C4 source frame mismatch",3);
  if(std::string(op)=="consumed")for(const char*key:{"prev_source_game_frame","history_version","params_version","nr_reset","reset_reason","audit_kind"})require(j.contains(key)&&j.at(key)==actual_frame_params.at(key),"C4 consumption history/reset/audit identity mismatch",3);
  printf("CONTROL_ACCEPT frame=%llu source_game_frame=%llu mode=%s op=%s status=%s exact_tuple=1\n",(unsigned long long)f,(unsigned long long)uint_value(j,"source_game_frame"),mode.c_str(),op,status);
 }
 void wait_control(uint64_t f,const char*file,const char*op,const char*status,bool numbered=true){validate_tuple(read_control(name(file,f,numbered)),f,op,status);}
 static bool hex32(const std::string&v){return v.size()==32&&std::all_of(v.begin(),v.end(),[](char c){return(c>='0'&&c<='9')||(c>='a'&&c<='f');});}
 static bool hex16(const std::string&v){return v.size()==16&&std::all_of(v.begin(),v.end(),[](char c){return(c>='0'&&c<='9')||(c>='a'&&c<='f');});}
 void config(){
  auto j=parse_control(read_bounded(dir,"game-config.json",10000));
  require(uint_value(j,"protocol_version")==4,"C4 explicit protocol_version4 required before allocation",3);
  execution_mode=string_value(j,"execution_mode");require(execution_mode=="hot"||execution_mode=="diagnostic","C4 hot|diagnostic only",3);diagnostic=execution_mode=="diagnostic";
  require(string_value(j,"session")==session&&string_value(j,"mode")==mode&&uint_value(j,"generation")==0&&string_value(j,"profile_id")==profile,"only exact C4 mode/profile/session/gen0",3);
  require(uint_value(j,"width")==1920&&uint_value(j,"height")==1080&&uint_value(j,"render_width")==1920&&uint_value(j,"render_height")==1080,"C4 actual HDR Run1080 1:1 required",3);
  const auto count64=uint_value(j,"frame_count");require(count64==4||count64==120||count64==1800||count64==18000,"C4 uint64 frame count must be 4/120/1800/18000 before narrowing",3);frame_count=unsigned(count64);require(uint_value(j,"max_frames")==count64,"C4 exact frame_count/max_frames",3);
  diagnostic_case=string_value(j,"diagnostic_case");
  require((!diagnostic&&mode!="skipEvaluate")||frame_count==4,"C4 diagnostic/skipEvaluate require exactly4",3);
  require((mode=="resident"&&((frame_count==4&&diagnostic_case=="smoke")||(frame_count==120&&(diagnostic_case=="static"||diagnostic_case=="motion"))||((frame_count==1800||frame_count==18000)&&diagnostic_case=="sustained")))||(mode=="skipEvaluate"&&frame_count==4&&diagnostic_case=="skipEvaluate"),"C4 exact mode/case/count",3);
  require(uint_value(j,"history_version")==1&&uint_value(j,"params_version")==1,"C4 immutable history/params epoch",3);
  require(uint_value(j,"cold_init_timeout_ms")==120000&&uint_value(j,"control_timeout_ms")==60000&&uint_value(j,"gpu_timeout_ms")==10000&&uint_value(j,"frame_completion_timeout_ms")==30000,"C4 fixed finite diagnostic budgets required",3);
  require(finite_value(j,"manual_white_point")==1&&!bool_value(j,"passthrough"),"C4 explicit manual whitepoint1/passthrough0",3);
  expected_device=string_value(j,"device_uuid");expected_driver=string_value(j,"driver_uuid");require(hex32(expected_device)&&hex32(expected_driver),"actual captured device/driver UUID syntax",3);
  width=N::W;height=N::H;
  printf("CONFIG C4_HELPER session=%s mode=%s profile=%s extent=%ux%u frames=%u groups=240,135,1 protocol_version=4 single_slot=1 cold_ms=120000 control_ms=60000 gpu_ms=10000 completion_ms=30000 NR_ONLY=1 SR=0 FG=0 CPU_PIXEL_IPC=0\n",session.c_str(),mode.c_str(),profile.c_str(),width,height,frame_count);
  printf("C4_EXECUTION_MODE value=%s validation_profile=%s numerical_scope=%s\n",execution_mode.c_str(),validation_profile(),diagnostic?"full_diagnostic":"API_GPU_tuple_only_NOT_CHECKED");
 }
 void environment(){
  for(const char*key:{"DXVK_ENABLE_NVAPI","WINEDLLPATH","WINEDLLOVERRIDES","NVIDIA_WINE_DLL_DIR","WINEDEBUG","DXVK_NVAPI_LOG_LEVEL"})printf("CHILD_ENV %s=%s\n",key,getenv(key)?getenv(key):"<unset>");
  const char*ge="/home/arinp/.local/share/Steam/compatibilitytools.d/GE-Proton11-7-x86_64/files/lib/";
  const std::string dllpath=std::string(ge)+"vkd3d:"+ge+"wine";
  require(getenv("DXVK_ENABLE_NVAPI")&&std::string(getenv("DXVK_ENABLE_NVAPI"))=="1","C4 fixture DXVK_ENABLE_NVAPI1",3);
  require(getenv("WINEDLLPATH")&&std::string(getenv("WINEDLLPATH"))==dllpath,"C4 exact installed GE WINEDLLPATH only",3);
  require(getenv("WINEDLLOVERRIDES")&&std::string(getenv("WINEDLLOVERRIDES"))=="mscoree,mshtml=;nvapi64=n;dxgi=n","C4 exact native override",3);
  require(getenv("NVIDIA_WINE_DLL_DIR")&&std::string(getenv("NVIDIA_WINE_DLL_DIR"))=="/usr/lib64/nvidia/wine","C4 exact host NGX directory",3);
  puts("RUNTIME_POLICY pinned_PE_only=1 DLL_copy=0 private_NVAPI_calls=0 own_private_prefix=REQUIRED no_SR_DualFeature_enlarger_FG=1");
 }
 void baseline(){
  auto j=envelope(0,"baseline","BASELINE");j.update(validation_fields());j["model_init"]=model_initialized;j["model_feature_created"]=feature!=nullptr;
  j["cold_init_timeout_ms"]=cold_init_timeout_ms;j["control_timeout_ms"]=control_timeout_ms;j["gpu_timeout_ms"]=gpu_timeout_ms;j["frame_completion_timeout_ms"]=frame_completion_timeout_ms;
  atomic_text(dir+"\\"+name("baseline",0,false),j.dump()+"\n");wait_control(0,"native-baseline","baseline_audit","BASELINE_AUDITED",false);
 }
 void exports(){
  create_images();shared_color=N::wrap(images[0].image,images[0].view,images[0].format,false);
  shared_depth=N::wrap(images[1].image,images[1].view,images[1].format,false);
  shared_motion=N::wrap(images[2].image,images[2].view,images[2].format,false);
  shared_output=N::wrap(images[3].image,images[3].view,images[3].format,true);
  initial_release();auto j=parse_control(metadata());j.update(envelope(0,"metadata","READY"));
  j["max_frames"]=frame_count;j["frame_count"]=frame_count;j["history_version"]=1;j["params_version"]=1;j["cold_init_timeout_ms"]=cold_init_timeout_ms;j["control_timeout_ms"]=control_timeout_ms;j["gpu_timeout_ms"]=gpu_timeout_ms;j["frame_completion_timeout_ms"]=frame_completion_timeout_ms;
  j["manual_white_point"]=1;j["passthrough"]=false;j["validation_profile"]=validation_profile();atomic_text(dir+"\\meta-g0.json",j.dump()+"\n");
  wait_control(0,"imported","imported","IMPORTED",false);retire_fds();auto fd=envelope(0,"fd_retired","FD_RETIRED");fd["closed_fd"]=Json::parse(closed_fds);atomic_text(dir+"\\"+name("fd-retired",0,false),fd.dump()+"\n");
 }
 void input(){
  const auto control_start=cpu_now_ms();auto j=read_control(name("input",frame_index));control_input_read_ms=cpu_now_ms()-control_start;validate_tuple(j,frame_index,"input","INPUT_READY",false);
  prev_source_game_frame=source_game_frame;source_game_frame=uint_value(j,"source_game_frame");require(source_game_frame>prev_source_game_frame,"C4 duplicate/backwards source frame rejected",3);
  const bool source_reset=bool_value(j,"source_reset");nr_reset=(frame_index==0||source_reset||source_game_frame!=prev_source_game_frame+1)?1u:0u;
  reset_reason=frame_index==0?"first":source_reset?"sr_reset":source_game_frame!=prev_source_game_frame+1?"source_gap":"continuous";
  require(uint_value(j,"prev_source_game_frame")==prev_source_game_frame&&uint_value(j,"history_version")==1&&uint_value(j,"params_version")==1&&uint_value(j,"nr_reset")==nr_reset&&string_value(j,"reset_reason")==reset_reason,"C4 real source history/reset mismatch",3);
  audit_full=diagnostic;
  require(string_value(j,"audit_kind")== (diagnostic?"full":"off"),"C4 mode-specific audit_kind mismatch",3);
  if(j.contains("audit_full"))require(bool_value(j,"audit_full")==audit_full,"C4 audit_full mismatch",3);
  require(string_value(j,"validation_profile")==validation_profile(),"C4 input required validation_profile mismatch",3);
  if(!diagnostic)require(string_value(j,"numerical_validation")=="NOT_CHECKED"&&string_value(j,"pixel_validation")=="NOT_RUN"&&string_value(j,"signature")=="NOT_RUN"&&uint_value(j,"dump_count")==0,"C4 input required exact hot scope",3);
  if(diagnostic){
   require(string_value(j,"signature_algorithm")=="rgba32f_mix32_xor_sum_v1","C4 diagnostic signature algorithm",3);
   const auto ix=uint_value(j,"input_signature_xor32"),is=uint_value(j,"input_signature_sum32");require(ix<=UINT32_MAX&&is<=UINT32_MAX,"C4 actual uint32 signatures",3);input_xor=uint32_t(ix);input_sum=uint32_t(is);
  }else{
   for(const char*key:{"signature_algorithm","input_signature_xor32","input_signature_sum32","output_signature_xor32","output_signature_sum32","input_digest","input_digest_bytes","input_digest_algorithm","output_digest","output_digest_bytes","output_digest_algorithm","current_input_signature_match","current_input_digest_match","raw_rgb_finite","raw_rgb_written","raw_rgb_total","raw_rgb_nan","resolved_rgba_finite","resolved_rgba_written","resolved_rgba_total","output_sentinel_count","source_rgba_finite","source_alpha_finite","source_alpha_preserved","source_alpha_total","visited_pixels"})require(!j.contains(key),"C4 hot must omit numerical/signature/FNV fields",3);
  }
  require(bool_value(j,"hdr")&&!bool_value(j,"exposure_valid")&&string_value(j,"exposure_source")=="unknown","C4 HDR and explicit unknown real exposure required",3);
  require(!bool_value(j,"depth_inverted")&&finite_value(j,"mv_scale_x")==1&&finite_value(j,"mv_scale_y")==1,"C4 real forward depth/MVscale1",3);
  require(finite_value(j,"nr_jitter_x")==0&&finite_value(j,"nr_jitter_y")==0&&finite_value(j,"manual_white_point")==1&&!bool_value(j,"passthrough"),"C4 model manual params required",3);
  for(const char*key:{"viewport_jitter_x","viewport_jitter_y","previous_viewport_jitter_x","previous_viewport_jitter_y","sr_jitter_x","sr_jitter_y","previous_sr_jitter_x","previous_sr_jitter_y"})require(finite_value(j,key)==0,"actual current/previous viewport/SR jitter must be0",3);
  require(j.contains("valid_rect")&&j["valid_rect"].is_object(),"full actual valid rect object",3);const auto&v=j["valid_rect"];
  require(uint_value(v,"x")==0&&uint_value(v,"y")==0&&uint_value(v,"width")==1920&&uint_value(v,"height")==1080,"C4 full 1:1 actual valid rect required",3);
  require(bool_value(j,"sr_initial_reset_observed")&&bool_value(j,"sr_history_jitter_zero_from_start"),"C4 actual fresh SR history evidence required",3);
  require(uint_value(j,"sr_initial_source_game_frame")<=source_game_frame,"SR initial source chronology",3);
  require(finite_value(j,"source_frame_ms")>0,"actual source frame_ms",3);(void)bool_value(j,"source_reset");
  input_digest.clear();if(audit_full){input_digest=string_value(j,"input_digest");require(hex16(input_digest)&&string_value(j,"input_digest_algorithm")=="fnv1a64_rgba32f_bytes"&&uint_value(j,"input_digest_bytes")==33177600,"C4 full raw sourceColor digest fields",3);}
  else for(const char*key:{"input_digest","input_digest_bytes","input_digest_algorithm"})require(!j.contains(key),"C4 non-full input must omit FNV/dump fields",3);
  actual_frame_params=j;for(const char*key:{"session","generation","frame","resource_set","source_game_frame","profile_id","width","height","render_width","render_height","mode","op","status","completed_GPU","output_valid","model_valid"})actual_frame_params.erase(key);
  printf("ACTUAL_FRAME source_game_frame=%llu source_reset=%u NR_reset=%u current_previous_viewport_SR_jitter=0,0 hdr=1 exposure=unknown manual_whitepoint=1 mvscale=1 same_texel_only=1\n",(unsigned long long)source_game_frame,unsigned(bool_value(j,"source_reset")),nr_reset);
 }
 void frame_deadline(const char*stage){
  const auto elapsed=GetTickCount64()-frame_start;
  printf("FRAME_PHASE execution_mode=%s stage=%s elapsed_ms=%llu budget_ms=%u failure_upper_bound_not_latency_target=1\n",execution_mode.c_str(),stage,(unsigned long long)elapsed,frame_completion_timeout_ms);
  require(elapsed<frame_completion_timeout_ms,"C4 frame completion deadline expired; invalid/quarantine",3);
  require(GetFileAttributesA((dir+"\\native.abort").c_str())==INVALID_FILE_ATTRIBUTES,"native abort/quarantine; no normal release",3);
 }
 static uint32_t mix32(uint32_t h){h^=h>>16;h*=0x7feb352du;h^=h>>15;h*=0x846ca68bu;h^=h>>16;return h;}
 static std::array<uint32_t,2> signature(const void*data,size_t words){
  const auto*p=static_cast<const uint8_t*>(data);uint32_t x=0,sum=0;
  for(size_t i=0;i<words;i++){uint32_t bits;memcpy(&bits,p+4*i,4);auto h=mix32(bits^(uint32_t(i)*0x9e3779b9u));x^=h;sum+=h;}
  return {x,sum};
 }
 void stamp(unsigned index){if(timestamps)D(vkCmdWriteTimestamp)(cmd,index==0?VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT:VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,timestamps,index);}
 Json gpu_times(){
  Json j{{"available",false},{"status","NOT_AVAILABLE"},{"timestamp_valid_bits",timestamp_bits},{"timestamp_period_ns",properties.limits.timestampPeriod},{"wrap_handling","unsigned delta masked to timestampValidBits"},{"scope","same Helper queue durations; Evaluate recorded commands include dependencies/scheduling, not pure model benchmark"},{"query_readback_bytes",timestamps?query_count*16u:0u},{"query_count",query_count},{"wait_bit",false}};
  const std::vector<const char*> names=diagnostic?std::vector<const char*>{"acquire_poison_ms","encode_ms","evaluate_commands_ms","resolve_ms","validation_ms","readbacks_ms","release_ms"}:std::vector<const char*>{"acquire_poison_ms","encode_ms","evaluate_commands_ms","resolve_ms","release_ms"};
  for(auto name:names){j[name]=nullptr;}j["total_ms"]=nullptr;
  if(!timestamps)return j;
  std::array<std::array<uint64_t,2>,8> q{};
  const auto result=D(vkGetQueryPoolResults)(device,timestamps,0,query_count,query_count*16,q.data(),16,VK_QUERY_RESULT_64_BIT|VK_QUERY_RESULT_WITH_AVAILABILITY_BIT);
  j["vk_result"]=int(result);Json availability=Json::array();bool ready=result==VK_SUCCESS;
  for(unsigned i=0;i<query_count;i++){availability.push_back(q[i][1]!=0);ready=ready&&q[i][1]!=0;}j["query_availability"]=availability;
  if(!ready)return j;
  const uint64_t mask=timestamp_bits==64?UINT64_MAX:((uint64_t(1)<<timestamp_bits)-1);
  for(unsigned i=0;i<names.size();i++)j[names[i]]=double((q[i+1][0]-q[i][0])&mask)*properties.limits.timestampPeriod/1e6;
  j["total_ms"]=double((q[query_count-1][0]-q[0][0])&mask)*properties.limits.timestampPeriod/1e6;
  j["available"]=true;j["status"]="AVAILABLE";return j;
 }
 void frame(){
  input();frame_start=GetTickCount64();require(nr->safe,"prior actual GPU fence before persistent reuse",3);begin();nr->reset_frame_descriptors();
  if(timestamps)D(vkCmdResetQueryPool)(cmd,timestamps,0,query_count);stamp(0);
  for(auto&im:images)barrier(im,VK_IMAGE_LAYOUT_GENERAL,VK_QUEUE_FAMILY_EXTERNAL,family,0,VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
  auto&color=shared_color;auto&depth=shared_depth;auto&motion=shared_motion;auto&output=shared_output;
  nr->transition(color,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);nr->transition(depth,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);nr->transition(motion,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  nr->clear(raw,NAN);nr->clear(output,-1234.f);nr->transition(proxy,VK_IMAGE_LAYOUT_GENERAL);nr->transition(keep,VK_IMAGE_LAYOUT_GENERAL);nr->transition(dummy,VK_IMAGE_LAYOUT_GENERAL);stamp(1);
  DlssNrConstants c{};c.Mode=DlssNrMode_Encode;c.Width=c.GuideWidth=width;c.Height=c.GuideHeight=height;c.WhitePoint=1;c.TransferStrength=c.ColourStrength=1;c.MaxRatio=2;c.Transfer=1;
  nr->dispatch(c,color,color,color,color,proxy,keep,&encode_cb);nr->transition(proxy,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);stamp(2);
  int er=0;double evaluate_cpu_ms=0;
  printf("FRAME_PARAMS session=%s generation=0 frame=%u resource_set=%s source_game_frame=%llu prev_source_game_frame=%llu reset=%u reset_reason=%s audit_kind=%s extent=1920,1080 guides=1920,1080 jitter=0,0 exposure=unknown manual_whitepoint=1 working_scale=1 passes=1 cache=0 compare=0 debug=0 SR=0 FG=0\n",session.c_str(),frame_index,resource_set().c_str(),(unsigned long long)source_game_frame,(unsigned long long)prev_source_game_frame,nr_reset,reset_reason.c_str(),diagnostic?"full":"off");
  frame_deadline("encode_recorded");
  if(!negative){
   using Eval=int(*)(void*,void*,void*,void*,void*,void*,void*,unsigned,unsigned,unsigned,unsigned,int,int,float,int,float,float,float,int,float,float);
   ++evaluates;const auto start=cpu_now_ms();er=get<Eval>(fw,"dlssnr_vk_evaluate")(cmd,feature,params,&proxy.ngx,&depth.ngx,&motion.ngx,&raw.ngx,width,height,width,height,0,int(nr_reset),1,0,1,1,1,1,1,1);
   evaluate_cpu_ms=cpu_now_ms()-start;
   printf("MODEL_EVALUATE frame=%u source_game_frame=%llu reset=%u calls=1 cumulative_calls=%u result=0x%08x CPU_call_ms=%.6f\n",frame_index,(unsigned long long)source_game_frame,nr_reset,evaluates,er,evaluate_cpu_ms);
  }else printf("SKIP_EVALUATE frame=%u evaluate_calls=0 resolve_calls=0 raw_poison=NaN output_poison=-1234\n",frame_index);
  stamp(3);frame_deadline("evaluate_returned");nr->memoryBarrier();
  if(!negative&&er==1){nr->transition(raw,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);nr->transition(keep,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);c.Mode=DlssNrMode_Resolve;nr->dispatch(c,proxy,raw,keep,motion,output,dummy,&resolve_cb);++resolves;}
  stamp(4);if(diagnostic){validator->record(color,raw,output);stamp(5);}
  if(diagnostic){nr->read(raw,true,&raw_read);nr->read(output,false,&out_read);nr->read(color,false,&source_read);stamp(6);}
  nr->transition(color,VK_IMAGE_LAYOUT_GENERAL);nr->transition(depth,VK_IMAGE_LAYOUT_GENERAL);nr->transition(motion,VK_IMAGE_LAYOUT_GENERAL);nr->transition(output,VK_IMAGE_LAYOUT_GENERAL);
  for(auto&im:images)barrier(im,VK_IMAGE_LAYOUT_GENERAL,family,VK_QUEUE_FAMILY_EXTERNAL,VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT,0,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);stamp(diagnostic?7:5);
  const auto wait_start=cpu_now_ms();in_flight=true;nr->submit();in_flight=false;const auto submit_and_wait_ms=cpu_now_ms()-wait_start;const auto gpu_wait_ms=nr->fence_wait_cpu_ms;
  frame_deadline("actual_GPU_fence_complete");require(negative||er==1,"actual model Evaluate failed; no OUTPUT_VALID");
  const auto cpu_stats_start=cpu_now_ms();
  auto e=envelope(frame_index,"complete",negative?"INVALID":"OUTPUT_VALID",!negative);e.update(actual_frame_params);
  e["evaluate_calls"]=negative?0u:1u;e["evaluate_result"]=er;e["resolve_calls"]=negative?0u:1u;e["reset"]=nr_reset;
  e["evaluate_calls_total"]=evaluates;e["resolve_calls_total"]=resolves;
  e.update(validation_fields());
  e["model_acceptance_scope"]=diagnostic?"API_GPU_tuple_and_full_diagnostic_pixels":"API_GPU_active_tuple_only; pixels NOT_CHECKED";
  if(diagnostic){
   auto stats=validator->read();const size_t pixels=size_t(width)*height,rgb=pixels*3,rgba=pixels*4,bytes=rgba*4;
  require(stats[11]==pixels&&stats[12]==0&&stats[15]==0,"C4 actual full-image visited count/reserved layout mismatch");
  require(stats[4]==rgba&&stats[5]==pixels&&stats[7]==input_xor&&stats[8]==input_sum,"C4 actual shared source finite/signature mismatch");
  require(negative?(stats[0]==0&&stats[1]==0&&stats[13]==rgb&&stats[2]==rgba&&stats[3]==0&&stats[14]==rgba):(stats[0]==rgb&&stats[1]==rgb&&stats[13]==0&&stats[2]==rgba&&stats[3]==rgba&&stats[14]==0&&stats[6]==pixels),"C4 every-frame full GPU finite/poison/alpha gate failed");
  e["raw_rgb_finite"]=stats[0];e["raw_rgb_written"]=stats[1];e["raw_rgb_total"]=rgb;e["raw_rgb_nan"]=stats[13];
  e["resolved_rgba_finite"]=stats[2];e["resolved_rgba_written"]=stats[3];e["resolved_rgba_total"]=rgba;e["output_sentinel_count"]=stats[14];
  e["source_rgba_finite"]=stats[4];e["source_alpha_finite"]=stats[5];e["source_alpha_preserved"]=negative?0u:stats[6];e["source_alpha_total"]=pixels;e["visited_pixels"]=stats[11];
  e["signature_algorithm"]="rgba32f_mix32_xor_sum_v1";e["input_signature_xor32"]=stats[7];e["input_signature_sum32"]=stats[8];e["output_signature_xor32"]=stats[9];e["output_signature_sum32"]=stats[10];e["current_input_signature_match"]=true;
  if(audit_full){
   const auto*rv=static_cast<const uint16_t*>(raw_read.mapped);const auto*ov=static_cast<const float*>(out_read.mapped);const auto*sv=static_cast<const float*>(source_read.mapped);std::array<uint32_t,16> cpu{};
   for(size_t p=0;p<pixels;p++)for(unsigned ch=0;ch<4;ch++){const size_t i=4*p+ch;const float r=N::unhalf(rv[i]);cpu[4]+=std::isfinite(sv[i]);cpu[2]+=std::isfinite(ov[i]);cpu[3]+=std::isfinite(ov[i])&&ov[i]!=-1234.f;cpu[14]+=ov[i]==-1234.f;if(ch<3){cpu[0]+=std::isfinite(r);cpu[1]+=std::isfinite(r);cpu[13]+=std::isnan(r);}else{cpu[5]+=std::isfinite(sv[i]);cpu[6]+=memcmp(&sv[i],&ov[i],4)==0;}}
   auto si=signature(source_read.mapped,rgba),oi=signature(out_read.mapped,rgba);cpu[7]=si[0];cpu[8]=si[1];cpu[9]=oi[0];cpu[10]=oi[1];cpu[11]=uint32_t(pixels);
   require(cpu==stats,"C4 independent full CPU readbacks disagree with full GPU counts/signatures");
   const auto id=fnv_bytes(source_read.mapped,bytes),od=fnv_bytes(out_read.mapped,bytes);require(id==input_digest,"C4 full actual shared sourceColor byte digest mismatch");
   e["current_input_digest_match"]=true;e["output_digest_algorithm"]="fnv1a64_rgba32f_bytes";e["output_digest_bytes"]=bytes;e["output_digest"]=od;
   const std::string suffix="-f"+std::to_string(frame_index);dump_bytes(dir+"\\raw"+suffix+".rgba16f",raw_read.mapped,pixels*8);dump_bytes(dir+"\\source"+suffix+".rgba32f",source_read.mapped,bytes);dump_bytes(dir+"\\resolve"+suffix+".rgba32f",out_read.mapped,bytes);
   e["dump_count"]=3;
   printf("FULL_AUDIT frame=%u source_game_frame=%llu bytes=82944000 source_digest=%s output_digest=%s CPU_GPU_counters_signatures_equal=1\n",frame_index,(unsigned long long)source_game_frame,id.c_str(),od.c_str());
  }
  printf("C4_DIAGNOSTIC_CHECK frame=%u raw_finite=%u raw_written=%u out_finite=%u out_written=%u alpha_equal=%u visited=%u GPU_complete=1 audit_kind=full\n",frame_index,stats[0],stats[1],stats[2],stats[3],stats[6],stats[11]);
  }
  e["cpu_stats_readback_ms"]=diagnostic?Json(cpu_now_ms()-cpu_stats_start):Json(nullptr);
  e["evaluate_cpu_call_ms"]=evaluate_cpu_ms;e["gpu_fence_wait_ms"]=gpu_wait_ms;e["gpu_submit_and_wait_ms"]=submit_and_wait_ms;e["control_input_read_ms"]=control_input_read_ms;e["cpu_timing_clock"]="QueryPerformanceCounter monotonic wall clock";
  e["gpu_timestamps"]=gpu_times();e["query_readback_bytes"]=timestamps?query_count*16u:0u;
  e["sentinel"]=-1234;e["nr_exposure"]=1;e["nr_pre_exposure"]=1;e["frame_elapsed_ms"]=GetTickCount64()-frame_start;
  e["helper_pixel_readback_bytes"]=diagnostic?82944000u:0u;e["audit_readback_bytes"]=diagnostic?82944064u:0u;e["gpu_validator_readback_bytes"]=diagnostic?64u:0u;
  frame_deadline("actual_api_GPU_tuple_and_optional_diagnostic_done");
  printf("C4_FRAME_COMPLETE frame=%u actual_Evaluate=%u result=%d actual_resolve=%u GPU_complete=1 execution_mode=%s validation_profile=%s pixel_readback_bytes=%u acceptance_scope=%s\n",frame_index,negative?0u:1u,er,negative?0u:1u,execution_mode.c_str(),validation_profile(),diagnostic?82944000u:0u,diagnostic?"full_diagnostic":"API_GPU_tuple_only");
  const auto publish_start=cpu_now_ms();
  atomic_text(dir+"\\"+name("complete",frame_index),e.dump()+"\n");const auto publish_ms=cpu_now_ms()-publish_start;
  const auto consumed_start=cpu_now_ms();wait_control(frame_index,"consumed","consumed","CONSUMED");
  printf("CONTROL_TIMING frame=%u input_read_ms=%.6f complete_publish_ms=%.6f consumed_wait_ms=%.6f cpu_clock=QueryPerformanceCounter publication_scope=JSON_serialize_and_atomic_write\n",frame_index,control_input_read_ms,publish_ms,cpu_now_ms()-consumed_start);++frames_done;
 }
 void retire_shared(){
  wait_control(frame_count,"native-released","release","NATIVE_RELEASED",false);require(!in_flight&&nr&&nr->safe,"shared retirement requires actual GPU completion");
  free_shared();publish(frame_count,"retired","retired","RETIRED",false,false);wait_control(frame_count,"native-audited","audit","FD_AUDITED",false);
 }

 void init_nr_device(){
  loader=LoadLibraryA("vulkan-1.dll");require(loader,"vulkan-1.dll",3);gipa=reinterpret_cast<PFN_vkGetInstanceProcAddr>(GetProcAddress(loader,"vkGetInstanceProcAddr"));require(gipa,"gipa",3);
  // Match the original successful NR fixture explicit KHR properties2 instance condition.
  uint32_t instance_count=0;api(I(vkEnumerateInstanceExtensionProperties)(nullptr,&instance_count,nullptr),"InstanceExtCount");
  std::vector<VkExtensionProperties> instance_exts(instance_count);api(I(vkEnumerateInstanceExtensionProperties)(nullptr,&instance_count,instance_exts.data()),"InstanceExts");
  const char*instance_ext=VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME;bool properties2_advertised=false;
  for(auto&e:instance_exts)if(!strcmp(e.extensionName,instance_ext))properties2_advertised=true;
  printf("INSTANCE_EXTENSION name=%s advertised=%u enabled=%u original_success_fixture_alignment=1\n",instance_ext,properties2_advertised,properties2_advertised);
  require(properties2_advertised,"KHR properties2 instance extension not advertised",3);
  VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};app.apiVersion=VK_API_VERSION_1_2;app.pApplicationName="Game C4 NR-only hot/diagnostic";VkInstanceCreateInfo ci{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};ci.pApplicationInfo=&app;ci.enabledExtensionCount=1;ci.ppEnabledExtensionNames=&instance_ext;api(I(vkCreateInstance)(&ci,nullptr,&instance),"CreateInstance");
  auto core_properties=gipa(instance,"vkGetPhysicalDeviceProperties2"),khr_properties=gipa(instance,"vkGetPhysicalDeviceProperties2KHR");
  printf("PROPERTIES2_ENTRYPOINT core=%p khr=%p both_present=%u\n",reinterpret_cast<void*>(core_properties),reinterpret_cast<void*>(khr_properties),core_properties&&khr_properties);
  require(core_properties&&khr_properties,"properties2 core/KHR dispatch after explicit extension",3);
  uint32_t n=0;api(I(vkEnumeratePhysicalDevices)(instance,&n,nullptr),"PhysicalCount");std::vector<VkPhysicalDevice> ps(n);api(I(vkEnumeratePhysicalDevices)(instance,&n,ps.data()),"PhysicalDevices");
  for(auto p:ps){VkPhysicalDeviceProperties2 pr{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};pr.pNext=&ids;I(vkGetPhysicalDeviceProperties2)(p,&pr);if(pr.properties.vendorID==0x10de){physical=p;properties=pr.properties;break;}}
  require(physical,"NVIDIA absent",3);I(vkGetPhysicalDeviceMemoryProperties)(physical,&memory);printf("DEVICE name=%s device_uuid=%s driver_uuid=%s driver=%u\n",properties.deviceName,hex(ids.deviceUUID).c_str(),hex(ids.driverUUID).c_str(),properties.driverVersion);
  require(hex(ids.deviceUUID)==expected_device&&hex(ids.driverUUID)==expected_driver,"C4 actual game/helper UUID mismatch",3);
  char luid_hex[17]{};for(unsigned i=0;i<8;i++)snprintf(luid_hex+2*i,3,"%02x",ids.deviceLUID[i]);
  printf("DEVICE_ID_LUID valid=%u luid=%s node_mask=%u host_NGX_requires_validity_unverified=1\n",ids.deviceLUIDValid,luid_hex,ids.deviceNodeMask);
  api(I(vkEnumerateDeviceExtensionProperties)(physical,nullptr,&n,nullptr),"ExtCount");std::vector<VkExtensionProperties> es(n);api(I(vkEnumerateDeviceExtensionProperties)(physical,nullptr,&n,es.data()),"Exts");std::vector<const char*> enable;
  for(const char*s:{VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME,VK_EXT_EXTERNAL_MEMORY_DMA_BUF_EXTENSION_NAME,"VK_NVX_binary_import","VK_NVX_image_view_handle",VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME,VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME}){bool found=false;for(auto&e:es)if(!strcmp(e.extensionName,s))found=true;printf("EXTENSION name=%s advertised=%d enabled=%d\n",s,found,found);require(found,s,3);enable.push_back(s);}
  I(vkGetPhysicalDeviceQueueFamilyProperties)(physical,&n,nullptr);std::vector<VkQueueFamilyProperties> qs(n);I(vkGetPhysicalDeviceQueueFamilyProperties)(physical,&n,qs.data());bool found=false;for(uint32_t i=0;i<n;i++)if(qs[i].queueCount&&(qs[i].queueFlags&VK_QUEUE_COMPUTE_BIT)){family=i;timestamp_bits=qs[i].timestampValidBits;found=true;break;}require(found,"compute queue",3);
  VkPhysicalDeviceBufferDeviceAddressFeatures bda{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES};VkPhysicalDeviceFeatures2 ft{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2};ft.pNext=&bda;I(vkGetPhysicalDeviceFeatures2)(physical,&ft);printf("BDA_FEATURE available=%u\n",bda.bufferDeviceAddress);require(bda.bufferDeviceAddress,"NR bufferDeviceAddress",3);bda.bufferDeviceAddressCaptureReplay=bda.bufferDeviceAddressMultiDevice=VK_FALSE;
  float priority=.5f;VkDeviceQueueCreateInfo qi{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO};qi.queueFamilyIndex=family;qi.queueCount=1;qi.pQueuePriorities=&priority;VkDeviceCreateInfo dc{VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO};dc.pNext=&bda;dc.queueCreateInfoCount=1;dc.pQueueCreateInfos=&qi;dc.enabledExtensionCount=uint32_t(enable.size());dc.ppEnabledExtensionNames=enable.data();api(I(vkCreateDevice)(physical,&dc,nullptr,&device),"CreateDevice");gdpa=I(vkGetDeviceProcAddr);D(vkGetDeviceQueue)(device,family,0,&queue);
  VkCommandPoolCreateInfo cp{VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO};cp.flags=VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;cp.queueFamilyIndex=family;api(D(vkCreateCommandPool)(device,&cp,nullptr,&pool),"CreatePool");VkCommandBufferAllocateInfo ca{VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO};ca.commandPool=pool;ca.level=VK_COMMAND_BUFFER_LEVEL_PRIMARY;ca.commandBufferCount=1;api(D(vkAllocateCommandBuffers)(device,&ca,&cmd),"AllocateCommand");VkFenceCreateInfo fc{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};api(D(vkCreateFence)(device,&fc,nullptr,&fence),"CreateFence");
 }

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
  phase_tick("PE_runtime_loaded");
  auto ir=get<decltype(&NVSDK_NGX_VULKAN_Init_Ext)>(core,"NVSDK_NGX_VULKAN_Init_Ext")(0x24480451ull,wide(dir).c_str(),instance,physical,device,NVSDK_NGX_Version_API,nullptr);
  phase_tick("core_init_returned");
  printf("CORE_VK_INIT result=0x%08x\n",(unsigned)ir);require(ir==NVSDK_NGX_Result_Success,"core init");
  auto caps=get<decltype(&NVSDK_NGX_VULKAN_GetCapabilityParameters)>(core,"NVSDK_NGX_VULKAN_GetCapabilityParameters")(&params);
  printf("VK_CAPABILITY result=0x%08x ptr=%p\n",(unsigned)caps,params);require(caps==NVSDK_NGX_Result_Success&&params,"core caps");
  require(nr_discover_float(params,get<void(*)(void*,const char*,float,int)>(fw,"dlssnr_call_probe_float"),get<void(*)(int)>(fw,"dlssnr_call_set_float_slot")),"safe parameter float ABI");
  phase_tick("caps_safe_parameter_abi");
  int mr=get<int(*)(const wchar_t*,const wchar_t*,void*,void*,void*,int)>(fw,"dlssnr_vk_init")(wide(model_path).c_str(),wide(dir).c_str(),instance,physical,device,0x15);
  printf("MODEL_VK_INIT result=0x%08x\n",mr);require(mr==1,"model init");
  phase_tick("model_init");
  begin();using Create=void*(*)(void*,void*,unsigned,unsigned,int,float,int,float,float,float,int,int);
  feature=get<Create>(fw,"dlssnr_vk_create")(cmd,params,width,height,0,1,0,1,1,1,1,0);
  auto*last=reinterpret_cast<int*>(GetProcAddress(fw,"dlssnr_vk_last_create"));printf("MODEL_VK_CREATE handle=%p result=0x%08x\n",feature,last?*last:0);
  submit();require(feature&&last&&*last==1,"model create result after real GPU completion");
  model_initialized=true;
  phase_tick("feature_create_gpu");
  nr=std::make_unique<N>(instance,physical,device,cmd,queue,gipa);nr->gpu_timeout_ms=gpu_timeout_ms;nr->shader();
  query(VK_FORMAT_R16G16B16A16_SFLOAT,"private_raw97_ACTUAL_MODEL_TARGET",false);
  proxy=nr->image(VK_FORMAT_R32G32B32A32_SFLOAT,false);raw=nr->image(VK_FORMAT_R16G16B16A16_SFLOAT,true);
  keep=nr->image(VK_FORMAT_R32G32B32A32_SFLOAT,true);dummy=nr->image(VK_FORMAT_R32G32B32A32_SFLOAT,true);
  // All private allocations/map/descriptors/pipeline exist BEFORE same-state FD baseline.
  {
   if(diagnostic){
    raw_read=nr->buffer(width*height*8ull,VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    out_read=nr->buffer(width*height*16ull,VK_BUFFER_USAGE_TRANSFER_DST_BIT);
    source_read=nr->buffer(width*height*16ull,VK_BUFFER_USAGE_TRANSFER_DST_BIT);
   }
   encode_cb=nr->buffer(sizeof(DlssNrConstants),VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
   resolve_cb=nr->buffer(sizeof(DlssNrConstants),VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
  }
  if(diagnostic)validator=std::make_unique<C3GpuValidation>(*nr);
  query_count=diagnostic?8u:6u;
  if(timestamp_bits){VkQueryPoolCreateInfo qi{VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO};qi.queryType=VK_QUERY_TYPE_TIMESTAMP;qi.queryCount=query_count;api(D(vkCreateQueryPool)(device,&qi,nullptr,&timestamps),"C4 persistent timestamp pool");}
  printf("GPU_TIMESTAMP_CAPABILITY valid_bits=%u period_ns=%g available=%u\n",timestamp_bits,properties.limits.timestampPeriod,unsigned(timestamps!=nullptr));
  begin();nr->clear(raw,NAN);nr->clear(proxy,0);nr->clear(keep,0);nr->clear(dummy,0);submit();
  printf("C4_PRIVATE_READY production_entry=CSMain descriptor_sets_max=8 pixel_readback_bytes=%u groups=240,135,1 private_raw97_allocated=1 diagnostic_pipeline_allocated=%u hot_omits_diagnostic_allocations=%u GPU_complete=1\n",diagnostic?82944000u:0u,unsigned(validator!=nullptr),unsigned(!diagnostic));
 }

 void shutdown(){
  require(!in_flight&&images.empty()&&nr&&nr->safe,"normal model shutdown requires GPU quiescence and release/audit");
  get<void(*)(void*)>(fw,"dlssnr_vk_release")(feature);feature=nullptr;puts("TEARDOWN feature_release completed");
  validator.reset();if(timestamps){D(vkDestroyQueryPool)(device,timestamps,nullptr);timestamps=nullptr;}
  nr.reset();puts("TEARDOWN private_production_fixture released");
  int sr=get<int(*)(const wchar_t*,void*)>(fw,"nr_poc_vk_shutdown")(wide(model_path).c_str(),device);printf("TEARDOWN model_Shutdown1 result=0x%08x\n",sr);require(sr==1,"model Shutdown1");
  auto dr=get<decltype(&NVSDK_NGX_VULKAN_DestroyParameters)>(core,"NVSDK_NGX_VULKAN_DestroyParameters")(params);printf("TEARDOWN core_DestroyParameters result=0x%08x\n",(unsigned)dr);require(dr==NVSDK_NGX_Result_Success,"DestroyParameters");params=nullptr;
  using CoreShutdown=NVSDK_NGX_Result(NVSDK_CONV*)(void);auto cs=get<CoreShutdown>(core,"NVSDK_NGX_VULKAN_Shutdown")();printf("TEARDOWN core_Shutdown result=0x%08x\n",(unsigned)cs);require(cs==NVSDK_NGX_Result_Success,"public core Shutdown");
  require(FreeLibrary(fw)&&FreeLibrary(core),"PE FreeLibrary");fw=core=nullptr;
  SetLastError(ERROR_SUCCESS);const BOOL nvapi_freed=FreeLibrary(nvapi);const DWORD free_error=GetLastError();
  printf("TEARDOWN NVAPI_FreeLibrary result=%u winerror=%lu after_core_shutdown=1\n",unsigned(nvapi_freed),(unsigned long)free_error);
  require(nvapi_freed,"NVAPI FreeLibrary after core shutdown");nvapi=nullptr;
  SetLastError(ERROR_SUCCESS);const BOOL dxgi_freed=FreeLibrary(dxgi);const DWORD dxgi_free_error=GetLastError();
  printf("TEARDOWN DXGI_FreeLibrary result=%u winerror=%lu after_core_shutdown_and_NVAPI_FreeLibrary=1\n",unsigned(dxgi_freed),(unsigned long)dxgi_free_error);
  require(dxgi_freed,"DXGI FreeLibrary after core shutdown and NVAPI release");dxgi=nullptr;
  D(vkDestroyFence)(device,fence,nullptr);D(vkDestroyCommandPool)(device,pool,nullptr);D(vkDestroyDevice)(device,nullptr);device=nullptr;
  I(vkDestroyInstance)(instance,nullptr);instance=nullptr;require(FreeLibrary(loader),"loader FreeLibrary");loader=nullptr;
  puts("NORMAL_RELEASE NR=1 shared=1 private=1 core=1 device=1 process_exit_not_raw_fd_strategy=1");
 }
};
int main(int argc,char**argv){
 if(argc!=4){fputs("Usage: wine_game_nr_hot.exe resident|skipEvaluate RUN_DIR_WINDOWS SESSION\n",stderr);return 2;}
 GameNrHelper c;c.dir=argv[2];c.session=argv[3];c.mode=argv[1];
 if(!freopen((c.dir+"\\wine-helper.log").c_str(),"w",stdout)||!freopen((c.dir+"\\wine-helper-stderr.log").c_str(),"w",stderr))return 4;setvbuf(stdout,nullptr,_IONBF,0);
 try{
  require((c.mode=="resident"||c.mode=="skipEvaluate")&&safe_token(c.session),"isolated C4 modes/safe unique session only");c.negative=c.mode=="skipEvaluate";
  c.config();c.environment();atomic_text(c.dir+"\\pid-request.json",Json{{"protocol_version",4},{"execution_mode",c.execution_mode},{"session",c.session},{"exe",module(GetModuleHandleA(nullptr))},{"win_pid",GetCurrentProcessId()}}.dump()+"\n");
  auto pt=read_bounded(c.dir,"native.pid",c.control_timeout_ms);char*end=nullptr;auto pid=strtoul(pt.c_str(),&end,10);while(end&&(*end=='\r'||*end=='\n'||*end==' '))++end;require(end&&!*end&&pid>1&&pid<=0x7fffffff,"native-supplied verified actual exporter Linux PID",3);c.linux_pid=uint32_t(pid);
  printf("ACTUAL_LINUX_PID pid=%u authority=native_ancestor_verified session=%s\n",c.linux_pid,c.session.c_str());
  c.cold_start=GetTickCount64();c.init_nr_device();c.phase_tick("device_ready");c.init_model();c.phase_tick("model_private_ready");
  c.baseline();c.exports();for(c.frame_index=0;c.frame_index<c.frame_count;c.frame_index++)c.frame();c.retire_shared();c.shutdown();
  auto result=c.envelope(c.frame_count,"result","PASS");result["frames"]=c.frames_done;result["actual_linux_pid"]=c.linux_pid;
  result["model_init"]=c.model_initialized;result["evaluate_calls"]=c.evaluates;result["resolve_calls"]=c.resolves;result["model_valid"]=!c.negative;result["output_valid"]=!c.negative;
  result.update(c.validation_fields());result["dump_count"]=c.diagnostic?c.frames_done*3u:0u;result["normal_release"]=true;result["pixels_IPC"]=false;result["external_GPU_sync"]="NOT_RUN";atomic_text(c.dir+"\\wine-result.json",result.dump()+"\n");
  printf("PASS C4_HELPER mode=%s frames=%u evaluate_calls=%u resolve_calls=%u NORMAL_EXIT=0\n",c.mode.c_str(),c.frames_done,c.evaluates,c.resolves);return 0;
 }catch(const std::exception&e){
  auto stop=dynamic_cast<const Stop*>(&e);const int code=stop?stop->code:2;
  printf("%s reason=%s exit=%d frames=%u evaluates=%u resolves=%u in_flight=%u normal_release=0 cleanup=PRIVATE_PROCESS_EXIT_ONLY\n",code==3?"BLOCKED":"FAIL",e.what(),code,c.frames_done,c.evaluates,c.resolves,unsigned(c.in_flight));
  try{atomic_text(c.dir+"\\wine-result.json",Json{{"protocol_version",4},{"execution_mode",c.execution_mode},{"status",code==3?"BLOCKED":"FAIL"},{"reason",e.what()},{"exit_code",code},{"session",c.session},{"mode",c.mode},{"source_game_frame",c.source_game_frame},{"frames",c.frames_done},{"model_init",c.model_initialized},{"evaluate_calls",c.evaluates},{"resolve_calls",c.resolves},{"output_valid",false},{"model_valid",false},{"normal_release",false},{"cleanup","PRIVATE_PROCESS_EXIT_NOT_NORMAL_FD_RECLAIM"}}.dump()+"\n");}catch(...){}
  // No destructor frees resources that may still be referenced by either GPU/peer.
  c.validator.release();c.nr.release();return code;
 }
}
