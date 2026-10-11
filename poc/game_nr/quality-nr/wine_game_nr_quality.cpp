// SPDX-License-Identifier: GPL-3.0-or-later
// Quality protocol5: raw720 guides / full1080 model. Phase and units UNVERIFIED.
#include "quality_game_transport.h"
#include "quality_game_control.h"
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
#include "quality_production_adapter.h"
#include "gpu_validation.h"
#include "frame_paths.h"
static const std::string model_path=winpath("/home/arinp/Code/dlss5_linux/dll/runtime/nvngx_dlssnr.dll");
static const std::string forwarder_path=winpath("/home/arinp/Code/dlss5_linux/OptiScaler-Linux-NR/build_out/nr-runtime-poc-20261010/build_out/poc-runtime-20261010/nvngx.dll_dlssnr.dll");
static const std::string core_path=winpath("/usr/lib64/nvidia/wine/_nvngx.dll");
static const std::string nvapi_path=winpath("/home/arinp/.local/share/Steam/compatibilitytools.d/GE-Proton11-7-x86_64/files/lib/wine/nvapi/x86_64-windows/nvapi64.dll");
static const std::string dxgi_path=winpath("/home/arinp/.local/share/Steam/compatibilitytools.d/GE-Proton11-7-x86_64/files/lib/wine/dxvk/x86_64-windows/dxgi.dll");
static std::string digest_bytes(const void*data,size_t bytes){uint64_t h=0xcbf29ce484222325ull;const auto*p=static_cast<const uint8_t*>(data);for(size_t i=0;i<bytes;i++){h^=p[i];h*=0x100000001b3ull;}char out[17];snprintf(out,sizeof out,"%016llx",(unsigned long long)h);return out;}
static void dump_bytes(const std::string&path,const void*data,size_t bytes){FILE*f=fopen(path.c_str(),"wb");require(f,"local evidence dump");bool ok=fwrite(data,1,bytes,f)==bytes;ok=(fclose(f)==0)&&ok;require(ok,"local evidence write");}

struct QualityHelper:QualityTransport {
 using N=NrSharedProduction;
 Library fw=nullptr,core=nullptr,nvapi=nullptr,dxgi=nullptr;
 NVSDK_NGX_Parameter*params=nullptr;void*feature=nullptr;
 std::unique_ptr<N> nr;std::unique_ptr<C3GpuValidation> validator;VkQueryPool timestamps{};uint32_t timestamp_bits=0;N::Image proxy{},raw{},keep{},dummy{},shared_color{},shared_depth{},shared_motion{},shared_output{};
 N::Buffer raw_read{},out_read{},source_read{},depth_read{},mv_read{},encode_cb{},resolve_cb{};
 std::string mode,execution_mode,profile="quality-hdr-direct720-raw-renderpx-v1",expected_device,expected_driver,input_digest;
 uint64_t source_game_frame=0,prev_source_game_frame=0;double cold_start=0,frame_start=0;
 unsigned frame_count=0,frame_index=0,nr_reset=1,query_count=0;bool diagnostic=false,audit_full=false;double control_input_read_ms=0;std::string diagnostic_case,reset_reason;uint32_t input_xor=0,input_sum=0;
 uint32_t control_timeout_ms=60000,cold_init_timeout_ms=120000,frame_completion_timeout_ms=30000;
 unsigned evaluates=0,resolves=0,frames_done=0;bool negative=false,no_op=false,model_initialized=false,feature_created_ever=false,frame_gpu_complete=false;unsigned copies=0,dumps_written=0;
 Json actual_frame_params=Json::object(),immutable_profile=Json::object(),guide_inputs=Json::object(),last_diagnostics=Json::object();
 static double cpu_now_ms(){return c4_cpu_now_ms();}
 const char*validation_profile()const{return no_op?"quality_noop_full":"quality_nr_full";}
 Json validation_fields()const{return Json{{"validation_profile",validation_profile()},{"audit_kind","full"},{"dump_count",0u}};}
 void phase_tick(const char*stage){
  require(GetFileAttributesA((dir+"\\native.abort").c_str())==INVALID_FILE_ATTRIBUTES,"native abort/quarantine; no normal release",3);
  const double elapsed=cpu_now_ms()-cold_start;
  printf("COLD_PHASE stage=%s elapsed_ms=%.6f budget_ms=%u clock=QueryPerformanceCounter synchronous_API_cancel=NOT_AVAILABLE\n",stage,elapsed,cold_init_timeout_ms);
  require(std::isfinite(elapsed)&&elapsed>=0&&elapsed<cold_init_timeout_ms,"cold readiness expired; hold resources",3);
 }
 Json envelope(uint64_t frame,const char*op,const char*status,bool valid=false)const{
  Json j{{"protocol_version",5u},{"transport_layout",quality_paths::layout},{"execution_mode",execution_mode},{"session",session},{"generation",generation},{"frame",frame},{"resource_set",resource_set()},
   {"source_game_frame",source_game_frame},{"profile_id",profile},{"width",1920u},{"height",1080u},{"render_width",1280u},{"render_height",720u},{"quality_profile",immutable_profile},
   {"mode",mode},{"status",status},{"op",op},{"completed_GPU",true},{"output_valid",valid},{"model_valid",valid&&!no_op}};
  j.update(validation_fields());return j;
 }
 static void check_layout(const Json&j){require(quality_paths::layout_matches(j),"required transport_layout frame-buckets-64-v1 mismatch; flat peer rejected",3);}
 std::string frame_dir(uint64_t f)const{return quality_paths::frame_dir(dir,f,frame_count,'\\');}
 std::string frame_path(uint64_t f,const std::string&basename)const{return quality_paths::frame_path(dir,f,frame_count,basename,'\\');}
 void verify_precreated_buckets()const{
  const auto directory=[](const std::string&p){const auto a=GetFileAttributesA(p.c_str());require(a!=INVALID_FILE_ATTRIBUTES&&(a&FILE_ATTRIBUTE_DIRECTORY)&&!(a&FILE_ATTRIBUTE_REPARSE_POINT),"native-precreated private non-reparse frame directory required; no flat fallback",3);};
  directory(dir);directory(dir+"\\frames");
  for(uint64_t i=0;i<quality_paths::bucket_count(frame_count);++i)directory(frame_dir(i*64));
  printf("TRANSPORT_LAYOUT value=%s precreated_buckets=%llu no_directory_creation=1 no_delete_archive=1 native_resolved_private_paths=REQUIRED\n",quality_paths::layout,(unsigned long long)quality_paths::bucket_count(frame_count));
 }
 void report_population(const char*stage,uint64_t f)const{
  const auto entries=[](const std::string&p){WIN32_FIND_DATAA data{};HANDLE h=FindFirstFileA((p+"\\*").c_str(),&data);if(h==INVALID_HANDLE_VALUE){const auto err=GetLastError();const auto a=GetFileAttributesA(p.c_str());require(err==ERROR_FILE_NOT_FOUND&&a!=INVALID_FILE_ATTRIBUTES&&(a&FILE_ATTRIBUTE_DIRECTORY),"directory population snapshot failed",3);return uint64_t(0);}uint64_t n=0;do{if(strcmp(data.cFileName,".")&&strcmp(data.cFileName,".."))++n;}while(FindNextFileA(h,&data));const auto err=GetLastError();const bool close=FindClose(h)!=0;require(err==ERROR_NO_MORE_FILES&&close,"directory population snapshot incomplete",3);return n;};
  printf("CONTROL_DIRECTORY_POPULATION stage=%s frame=%llu root_entries=%llu leaf_entries=%llu hot_record_class_bound=320 total_entries_include_tmp_dump_fault=1 scope=boundary_snapshot_not_peak_not_leak_audit\n",stage,(unsigned long long)f,(unsigned long long)entries(dir),(unsigned long long)entries(frame_dir(f)));
 }
 void publish(uint64_t f,const char*file,const char*op,const char*status,bool valid=false,bool numbered=true){
  atomic_text(dir+"\\"+name(file,f,numbered),envelope(f,op,status,valid).dump()+"\n");
 }
 Json read_control(const std::string&filename){return parse_control(read_bounded(dir,filename,control_timeout_ms));}
 struct ActiveControlTiming {uint64_t poll_count=0,sleep_calls=0;double sleep_wall_ms=0,elapsed_ms=0;};
 ActiveControlTiming input_poll{},consumed_poll{};
 Json read_active_control(uint64_t frame,const std::string&filename,ActiveControlTiming&timing){
  // C4 frame input/consumed only. Frozen setup/shared-header reader stays at Sleep(5).
  timing={};const auto start_tick=GetTickCount64();const auto start_cpu=cpu_now_ms();
  while(GetTickCount64()-start_tick<control_timeout_ms){
   if(GetFileAttributesA((dir+"\\native.abort").c_str())!=INVALID_FILE_ATTRIBUTES)throw Stop(3,"native abort/quarantine; no normal release");
   ++timing.poll_count;FILE*f=fopen(frame_path(frame,filename).c_str(),"rb");
   if(f){
    std::string text;char b[4096];size_t n;
    while((n=fread(b,1,sizeof b,f))){text.append(b,n);if(text.size()>65536){fclose(f);throw Stop(2,"control exceeds bounded bytes");}}
    const bool ok=!ferror(f);fclose(f);require(ok,"read control");
    auto j=parse_control(text);timing.elapsed_ms=cpu_now_ms()-start_cpu;return j;
   }
   ++timing.sleep_calls;const auto sleep_start=cpu_now_ms();Sleep(1);timing.sleep_wall_ms+=cpu_now_ms()-sleep_start;
  }
  throw Stop(3,"bounded control timeout "+filename+"; hold resources/no normal release");
 }
 void validate_tuple(const Json&j,uint64_t f,const char*op,const char*status,bool match_source=true){
  check_layout(j);
  require(uint_value(j,"protocol_version")==5&&string_value(j,"execution_mode")=="diagnostic","protocol5 diagnostic only; old peers rejected",3);
  require(j.contains("quality_profile")&&quality_wire::matches(j.at("quality_profile"))&&j.at("quality_profile")==immutable_profile,"exact immutable quality_profile echo",3);
  require(string_value(j,"session")==session&&uint_value(j,"generation")==0&&uint_value(j,"frame")==f&&string_value(j,"resource_set")==resource_set(),"exact active tuple",3);
  require(string_value(j,"profile_id")==profile&&string_value(j,"mode")==mode&&uint_value(j,"width")==1920&&uint_value(j,"height")==1080&&uint_value(j,"render_width")==1280&&uint_value(j,"render_height")==720,"actual Color/model1080 guides720 only",3);
  require(string_value(j,"validation_profile")==validation_profile()&&string_value(j,"audit_kind")=="full","mode-specific full diagnostic scope",3);
  require(string_value(j,"op")==op&&string_value(j,"status")==status&&bool_value(j,"completed_GPU")&&!bool_value(j,"output_valid")&&!bool_value(j,"model_valid"),"control op/status/GPU/validity",3);
  if(match_source)require(uint_value(j,"source_game_frame")==source_game_frame,"source mismatch",3);
  if(std::string(op)=="consumed")for(const char*key:{"prev_source_game_frame","history_version","params_version","nr_reset","reset_reason","audit_kind"})require(j.contains(key)&&j.at(key)==actual_frame_params.at(key),"consumed actual history/reset mismatch",3);
  printf("CONTROL_ACCEPT frame=%llu source_game_frame=%llu mode=%s op=%s status=%s exact_tuple=1 protocol=5\n",(unsigned long long)f,(unsigned long long)uint_value(j,"source_game_frame"),mode.c_str(),op,status);
 }
 void wait_control(uint64_t f,const char*file,const char*op,const char*status,bool numbered=true){validate_tuple(read_control(name(file,f,numbered)),f,op,status);}
 static bool hex32(const std::string&v){return v.size()==32&&std::all_of(v.begin(),v.end(),[](char c){return(c>='0'&&c<='9')||(c>='a'&&c<='f');});}
 static bool hex16(const std::string&v){return v.size()==16&&std::all_of(v.begin(),v.end(),[](char c){return(c>='0'&&c<='9')||(c>='a'&&c<='f');});}
 void config(){
  auto j=parse_control(read_bounded(dir,"game-config.json",10000));check_layout(j);
  require(uint_value(j,"protocol_version")==5&&string_value(j,"execution_mode")=="diagnostic","explicit protocol5 diagnostic before allocation",3);
  execution_mode="diagnostic";diagnostic=true;audit_full=true;
  require(j.contains("quality_profile")&&quality_wire::matches(j.at("quality_profile")),"immutable profile exact keys/types/constants",3);immutable_profile=j.at("quality_profile");
  require(string_value(j,"session")==session&&string_value(j,"mode")==mode&&uint_value(j,"generation")==0&&string_value(j,"profile_id")==profile,"exact mode/profile/session/gen0",3);
  require(uint_value(j,"width")==1920&&uint_value(j,"height")==1080&&uint_value(j,"render_width")==1280&&uint_value(j,"render_height")==720,"Quality actual extents",3);
  const auto count=uint_value(j,"frame_count");require(count==1||count==4||count==120||count==1800,"uint64 finite count before narrowing",3);
  require(uint_value(j,"max_frames")==count,"exact frame_count/max_frames",3);
  diagnostic_case=string_value(j,"diagnostic_case");
  require(quality_wire::mode_count_matches(mode,count,diagnostic_case),"only authorized noop1/single1/skip1or4/resident4; no hot120",3);
  frame_count=unsigned(count);require(string_value(j,"validation_profile")==validation_profile()&&string_value(j,"audit_kind")=="full","config diagnostic mode scope",3);
  require(uint_value(j,"history_version")==1&&uint_value(j,"params_version")==1,"immutable history/params epoch",3);
  require(uint_value(j,"cold_init_timeout_ms")==120000&&uint_value(j,"control_timeout_ms")==60000&&uint_value(j,"gpu_timeout_ms")==10000&&uint_value(j,"frame_completion_timeout_ms")==30000,"fixed finite budgets",3);
  expected_device=string_value(j,"device_uuid");expected_driver=string_value(j,"driver_uuid");require(hex32(expected_device)&&hex32(expected_driver),"actual GPU/driver UUID syntax",3);
  verify_precreated_buckets();width=N::W;height=N::H;
  printf("CONFIG QUALITY_HELPER protocol5 session=%s mode=%s extent=1920,1080 guides=1280,720 frames=%u profile=%s phase_correction=none NRphase=UNVERIFIED NRscale=production_candidate_UNVERIFIED SR=0 FG=0 CPU_PIXEL_IPC=0\n",session.c_str(),mode.c_str(),frame_count,profile.c_str());
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
  auto j=envelope(0,"baseline","BASELINE");j.update(validation_fields());j["model_init"]=model_initialized;j["feature_created"]=feature_created_ever;
  j["cold_init_timeout_ms"]=cold_init_timeout_ms;j["control_timeout_ms"]=control_timeout_ms;j["gpu_timeout_ms"]=gpu_timeout_ms;j["frame_completion_timeout_ms"]=frame_completion_timeout_ms;
  atomic_text(dir+"\\"+name("baseline",0,false),j.dump()+"\n");wait_control(0,"native-baseline","baseline_audit","BASELINE_AUDITED",false);
 }
 void exports(){
  create_images();shared_color=N::wrap(images[0].image,images[0].view,images[0].format,false,images[0].width,images[0].height);
  shared_depth=N::wrap(images[1].image,images[1].view,images[1].format,false,images[1].width,images[1].height);
  shared_motion=N::wrap(images[2].image,images[2].view,images[2].format,false,images[2].width,images[2].height);
  shared_output=N::wrap(images[3].image,images[3].view,images[3].format,true,images[3].width,images[3].height);
  initial_release();auto j=parse_control(metadata());j.update(envelope(0,"metadata","READY"));
  j["max_frames"]=frame_count;j["frame_count"]=frame_count;j["history_version"]=1;j["params_version"]=1;j["cold_init_timeout_ms"]=cold_init_timeout_ms;j["control_timeout_ms"]=control_timeout_ms;j["gpu_timeout_ms"]=gpu_timeout_ms;j["frame_completion_timeout_ms"]=frame_completion_timeout_ms;
  j["manual_white_point"]=1;j["passthrough"]=false;j["validation_profile"]=validation_profile();atomic_text(dir+"\\meta-g0.json",j.dump()+"\n");
  wait_control(0,"imported","imported","IMPORTED",false);retire_fds();auto fd=envelope(0,"fd_retired","FD_RETIRED");fd["closed_fd"]=Json::parse(closed_fds);atomic_text(dir+"\\"+name("fd-retired",0,false),fd.dump()+"\n");
 }
 void input(){
  const auto start=cpu_now_ms();auto j=read_active_control(frame_index,name("input",frame_index),input_poll);control_input_read_ms=cpu_now_ms()-start;
  validate_tuple(j,frame_index,"input","INPUT_READY",false);
  prev_source_game_frame=source_game_frame;source_game_frame=uint_value(j,"source_game_frame");require(source_game_frame>prev_source_game_frame,"duplicate/backwards source",3);
  const bool sr_reset=bool_value(j,"source_reset");nr_reset=(frame_index==0||sr_reset||source_game_frame!=prev_source_game_frame+1)?1u:0u;
  reset_reason=frame_index==0?"first":sr_reset?"sr_reset":source_game_frame!=prev_source_game_frame+1?"source_gap":"continuous";
  require(uint_value(j,"prev_source_game_frame")==prev_source_game_frame&&uint_value(j,"history_version")==1&&uint_value(j,"params_version")==1&&uint_value(j,"nr_reset")==nr_reset&&string_value(j,"reset_reason")==reset_reason,"actual reset/history echo",3);
  if(j.contains("reset"))require(uint_value(j,"reset")==nr_reset,"reset request mismatch",3);
  require(bool_value(j,"sr_history_jitter_enabled_from_start")&&bool_value(j,"sr_initial_reset_observed")&&!bool_value(j,"nr_jitter_parameter_present"),"actual j1 SR history, no NR jitter setter",3);
  require(uint_value(j,"sr_initial_source_game_frame")<=source_game_frame&&finite_value(j,"source_frame_ms")>0,"actual SR chronology/timing",3);
  for(const char*key:{"viewport_jitter_x","viewport_jitter_y","previous_viewport_jitter_x","previous_viewport_jitter_y","sr_jitter_x","sr_jitter_y","previous_sr_jitter_x","previous_sr_jitter_y","sr_mvec_scale_x","sr_mvec_scale_y"})(void)finite_value(j,key);
  for(const char*key:{"nr_jitter_x","nr_jitter_y","mv_scale_x","mv_scale_y","sr_history_jitter_zero_from_start"})require(!j.contains(key),"old j0/ambiguous NR fields rejected",3);
  if(j.contains("hdr"))require(bool_value(j,"hdr"),"HDR Run only",3);
  if(j.contains("depth_inverted"))require(!bool_value(j,"depth_inverted"),"actual forward device-Z",3);
  if(j.contains("passthrough"))require(!bool_value(j,"passthrough"),"HDR encode not passthrough",3);
  if(j.contains("manual_white_point"))require(finite_value(j,"manual_white_point")==1,"manualWP1",3);
  if(j.contains("exposure_source"))require(string_value(j,"exposure_source")=="unknown","real exposure unknown",3);
  if(j.contains("exposure_valid"))require(!bool_value(j,"exposure_valid"),"not measured game exposure",3);
  auto rect=[](const Json&r,uint64_t w,uint64_t h){return r.is_object()&&r.size()==4&&uint_value(r,"x")==0&&uint_value(r,"y")==0&&uint_value(r,"width")==w&&uint_value(r,"height")==h;};
  require(j.contains("valid_rect")&&rect(j.at("valid_rect"),1920,1080),"full output valid rect",3);
  require(j.contains("input_descriptors")&&j.at("input_descriptors").is_object()&&j.at("input_descriptors").size()==3,"exact source descriptor roles",3);
  for(const auto&role:std::vector<std::string>{"color","depth","mv"}){
   const auto&d=j.at("input_descriptors").at(role);bool guide=role!="color";const unsigned fmt=role=="color"?97:role=="depth"?130:83;
   require(d.is_object()&&d.size()==7&&uint_value(d,"format")==fmt&&uint_value(d,"aspect")==unsigned(role=="depth"?2:1)&&uint_value(d,"width")==unsigned(guide?1280:1920)&&uint_value(d,"height")==unsigned(guide?720:1080)&&uint_value(d,"layout")==1&&(uint_value(d,"usage")&VK_IMAGE_USAGE_TRANSFER_SRC_BIT)&&uint_value(d,"usage")<=UINT32_MAX&&rect(d.at("valid_rect"),guide?1280:1920,guide?720:1080),"actual source format/aspect/extent/TRANSFER_SRC descriptor",3);
  }
  input_digest=string_value(j,"input_digest");require(hex16(input_digest)&&string_value(j,"input_digest_algorithm")=="fnv1a64_rgba32f_bytes"&&uint_value(j,"input_digest_bytes")==33177600,"actual post-SR source byte digest",3);
  require(string_value(j,"signature_algorithm")=="rgba32f_mix32_xor_sum_v1","actual source signature algorithm",3);
  const auto x=uint_value(j,"input_signature_xor32"),sum=uint_value(j,"input_signature_sum32");require(x<=UINT32_MAX&&sum<=UINT32_MAX,"actual uint32 source signature",3);input_xor=uint32_t(x);input_sum=uint32_t(sum);
  require(j.contains("guide_input_digests")&&j.at("guide_input_digests").is_object()&&j.at("guide_input_digests").size()==2,"exact guide digest roles",3);guide_inputs=j.at("guide_input_digests");
  for(const auto&role:std::vector<std::string>{"depth","mv"}){const auto&d=guide_inputs.at(role);require(d.is_object()&&d.size()==3&&string_value(d,"algorithm")== (role=="depth"?"fnv1a64_r32f_bytes":"fnv1a64_rg32f_bytes")&&uint_value(d,"bytes")==uint64_t(role=="depth"?3686400:7372800)&&hex16(string_value(d,"digest")),"guide exact algorithm/bytes/hex digest",3);}
  actual_frame_params=j;for(const char*key:{"session","generation","frame","resource_set","source_game_frame","profile_id","width","height","render_width","render_height","mode","op","status","completed_GPU","output_valid","model_valid"})actual_frame_params.erase(key);
  last_diagnostics=Json::object();
  printf("ACTUAL_FRAME source=%llu prev=%llu nr_reset_request=%u reason=%s guides=1280,720 storedMV=renderpixels_previous_minus_current phase_correction=none NRscale=1_candidate NRphase=UNVERIFIED actual_jitter=%s\n",(unsigned long long)source_game_frame,(unsigned long long)prev_source_game_frame,nr_reset,reset_reason.c_str(),Json{{"viewport_x",j.at("viewport_jitter_x")},{"viewport_y",j.at("viewport_jitter_y")},{"SR_x",j.at("sr_jitter_x")},{"SR_y",j.at("sr_jitter_y")}}.dump().c_str());
 }
 void frame_deadline(const char*stage){
  const auto elapsed=cpu_now_ms()-frame_start;
  require(std::isfinite(elapsed)&&elapsed>=0,"C4 monotonic frame clock invalid; quarantine",3);
  printf("FRAME_PHASE execution_mode=%s stage=%s elapsed_ms=%.6f budget_ms=%u cpu_clock=QueryPerformanceCounter failure_upper_bound_not_latency_target=1\n",execution_mode.c_str(),stage,elapsed,frame_completion_timeout_ms);
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
  input();frame_gpu_complete=false;frame_start=cpu_now_ms();require(nr->safe,"prior real fence before reuse",3);begin();if(!no_op)nr->reset_frame_descriptors();
  if(timestamps)D(vkCmdResetQueryPool)(cmd,timestamps,0,query_count);stamp(0);
  for(auto&im:images)barrier(im,VK_IMAGE_LAYOUT_GENERAL,VK_QUEUE_FAMILY_EXTERNAL,family,0,VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
  auto&color=shared_color;auto&depth=shared_depth;auto&motion=shared_motion;auto&output=shared_output;
  int er=0;double evaluate_cpu_ms=0;
  if(no_op){
   nr->transition(color,VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);nr->transition(output,VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
   VkImageCopy copy{};copy.srcSubresource=copy.dstSubresource={VK_IMAGE_ASPECT_COLOR_BIT,0,0,1};copy.extent={1920,1080,1};
   D(vkCmdCopyImage)(cmd,color.handle,color.layout,output.handle,output.layout,1,&copy);++copies;nr->memoryBarrier();
   puts("NOOP_GPU_COPY Color109_to_independent_Output109 extent=1920,1080 model_init=0 feature_create=0 evaluate=0 resolve=0");
  }else{
   nr->transition(color,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);nr->transition(depth,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);nr->transition(motion,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
   nr->clear(raw,NAN);nr->clear(output,-1234.f);nr->transition(proxy,VK_IMAGE_LAYOUT_GENERAL);nr->transition(keep,VK_IMAGE_LAYOUT_GENERAL);nr->transition(dummy,VK_IMAGE_LAYOUT_GENERAL);stamp(1);
   DlssNrConstants c{};c.Mode=DlssNrMode_Encode;c.Width=1920;c.Height=1080;c.GuideWidth=1280;c.GuideHeight=720;c.WhitePoint=1;c.TransferStrength=c.ColourStrength=1;c.MaxRatio=2;c.Transfer=1;
   nr->dispatch(c,color,color,color,color,proxy,keep,&encode_cb);nr->transition(proxy,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);stamp(2);
   printf("FRAME_PARAMS session=%s frame=%u source_game_frame=%llu reset=%u reset_reason=%s model=1920,1080 guides=1280,720 NRscale=1_production_candidate phase_correction=none exposure=unknown WhitePoint=1 WorkingScale=1 passes=1 cache=0 compare=0 debug=0 SR=0 FG=0\n",session.c_str(),frame_index,(unsigned long long)source_game_frame,nr_reset,reset_reason.c_str());
   frame_deadline("encode_recorded");
   if(!negative){
    using Eval=int(*)(void*,void*,void*,void*,void*,void*,void*,unsigned,unsigned,unsigned,unsigned,int,int,float,int,float,float,float,int,float,float);
    ++evaluates;const auto start=cpu_now_ms();er=get<Eval>(fw,"dlssnr_vk_evaluate")(cmd,feature,params,&proxy.ngx,&depth.ngx,&motion.ngx,&raw.ngx,1920,1080,1280,720,0,int(nr_reset),1,0,1,1,1,1,finite_value(immutable_profile,"nr_mvec_scale_x"),finite_value(immutable_profile,"nr_mvec_scale_y"));
    evaluate_cpu_ms=cpu_now_ms()-start;
    printf("MODEL_EVALUATE frame=%u source_game_frame=%llu reset=%u calls=1 cumulative_calls=%u result=0x%08x model=1920,1080 guides=1280,720 NRscale=1_candidate CPU_call_ms=%.6f\n",frame_index,(unsigned long long)source_game_frame,nr_reset,evaluates,er,evaluate_cpu_ms);
   }else printf("SKIP_EVALUATE frame=%u evaluate_calls=0 resolve_calls=0 raw_poison=NaN output_poison=-1234 requested_reset=%u NOT_APPLIED\n",frame_index,nr_reset);
   stamp(3);frame_deadline("evaluate_returned");nr->memoryBarrier();
   if(!negative&&er==1){nr->transition(raw,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);nr->transition(keep,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);c.Mode=DlssNrMode_Resolve;nr->dispatch(c,proxy,raw,keep,motion,output,dummy,&resolve_cb);++resolves;}
   stamp(4);validator->record(color,raw,output);stamp(5);nr->read(raw,true,&raw_read);
  }
  nr->read(output,false,&out_read);nr->read(color,false,&source_read);
  nr->read_guide(depth,depth_read);nr->read_guide(motion,mv_read);stamp(6);
  nr->transition(color,VK_IMAGE_LAYOUT_GENERAL);nr->transition(depth,VK_IMAGE_LAYOUT_GENERAL);nr->transition(motion,VK_IMAGE_LAYOUT_GENERAL);nr->transition(output,VK_IMAGE_LAYOUT_GENERAL);
  for(auto&im:images)barrier(im,VK_IMAGE_LAYOUT_GENERAL,family,VK_QUEUE_FAMILY_EXTERNAL,VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT,0,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);stamp(7);
  const auto wait_start=cpu_now_ms();in_flight=true;nr->submit();in_flight=false;frame_gpu_complete=true;const auto submit_and_wait_ms=cpu_now_ms()-wait_start;const auto gpu_wait_ms=nr->fence_wait_cpu_ms;
  frame_deadline("actual_GPU_fence_complete");require(no_op||negative||er==1,"real Evaluate failed; no OUTPUT_VALID");
  const auto cpu_stats_start=cpu_now_ms();const size_t pixels=2073600,rgb=6220800,rgba=8294400,bytes=33177600;
  const auto*rv=static_cast<const uint16_t*>(raw_read.mapped);const auto*ov=static_cast<const float*>(out_read.mapped);const auto*sv=static_cast<const float*>(source_read.mapped);
  std::array<uint32_t,16> cpu{};size_t source_negative=0;
  for(size_t p=0;p<pixels;p++)for(unsigned ch=0;ch<4;ch++){
   const size_t i=4*p+ch;cpu[4]+=std::isfinite(sv[i]);cpu[2]+=std::isfinite(ov[i]);cpu[3]+=std::isfinite(ov[i])&&ov[i]!=-1234.f;cpu[14]+=ov[i]==-1234.f;
   if(ch<3){source_negative+=std::isfinite(sv[i])&&sv[i]<0;if(!no_op){const float r=N::unhalf(rv[i]);cpu[0]+=std::isfinite(r);cpu[1]+=std::isfinite(r);cpu[13]+=std::isnan(r);}}
   else{cpu[5]+=std::isfinite(sv[i]);cpu[6]+=memcmp(&sv[i],&ov[i],4)==0;}
  }
  auto si=signature(source_read.mapped,rgba),oi=signature(out_read.mapped,rgba);cpu[7]=si[0];cpu[8]=si[1];cpu[9]=oi[0];cpu[10]=oi[1];cpu[11]=uint32_t(pixels);
  const auto stats=no_op?cpu:validator->read();last_diagnostics["measured_pixel_counts"]=stats;
  if(!no_op)require(cpu==stats,"independent CPU readbacks disagree with actual GPU64B counts/signatures");
  require(stats[11]==pixels&&stats[12]==0&&stats[15]==0&&stats[4]==rgba&&stats[5]==pixels&&stats[7]==input_xor&&stats[8]==input_sum,"actual full source finite/signature gate");
  if(no_op)require(stats[2]==rgba&&stats[6]==pixels&&memcmp(source_read.mapped,out_read.mapped,bytes)==0,"actual noop Color/Output bit equality and finite/alpha");
  else require(negative?(stats[0]==0&&stats[1]==0&&stats[13]==rgb&&stats[2]==rgba&&stats[3]==0&&stats[14]==rgba):(stats[0]==rgb&&stats[1]==rgb&&stats[13]==0&&stats[2]==rgba&&stats[3]==rgba&&stats[14]==0&&stats[6]==pixels),"actual raw/output finite/written/poison/alpha gate");
  const auto id=fnv_bytes(source_read.mapped,bytes),od=fnv_bytes(out_read.mapped,bytes);last_diagnostics["source_digest"]=id;last_diagnostics["output_digest"]=od;
  require(id==input_digest,"actual shared sourceColor raw-byte digest mismatch");
  const std::string suffix="-f"+std::to_string(frame_index);
  dump_bytes(frame_path(frame_index,"depth"+suffix+".r32f"),depth_read.mapped,3686400);
  dump_bytes(frame_path(frame_index,"mv"+suffix+".rg32f"),mv_read.mapped,7372800);
  const auto guide_validation=inspect_guides();
  if(!no_op)dump_bytes(frame_path(frame_index,"raw"+suffix+".rgba16f"),raw_read.mapped,pixels*8);
  dump_bytes(frame_path(frame_index,"source"+suffix+".rgba32f"),source_read.mapped,bytes);dump_bytes(frame_path(frame_index,"resolve"+suffix+".rgba32f"),out_read.mapped,bytes);
  dumps_written+=no_op?4u:5u;
  auto e=envelope(frame_index,"complete",no_op?"NOOP_VALID":negative?"INVALID":"OUTPUT_VALID",!negative);e.update(actual_frame_params);
  e["model_init"]=model_initialized;e["feature_created"]=feature_created_ever;e["copy_calls"]=no_op?1u:0u;
  e["evaluate_calls"]=no_op||negative?0u:1u;e["evaluate_result"]=unsigned(er);e["resolve_calls"]=no_op||negative?0u:1u;e["reset"]=nr_reset;
  e["evaluate_calls_total"]=evaluates;e["resolve_calls_total"]=resolves;
  e.update(validation_fields());e["dump_count"]=no_op?4u:5u;
  e["model_acceptance_scope"]=no_op?"GPU_copy_and_full_diagnostic_pixels_phase_UNVERIFIED":"API_GPU_tuple_and_full_diagnostic_pixels_phase_UNVERIFIED";
  if(no_op)e["model_validation"]="NOT_RUN";
  e["pixel_counter_source"]=no_op?"CPU_full_actual_GPU_readback":"GPU64B_crosschecked_CPU_full_actual_readback";
  if(!no_op){e["raw_rgb_finite"]=stats[0];e["raw_rgb_written"]=stats[1];e["raw_rgb_total"]=rgb;e["raw_rgb_nan"]=stats[13];}
  e["resolved_rgba_finite"]=stats[2];e["resolved_rgba_written"]=stats[3];e["resolved_rgba_total"]=rgba;e["output_sentinel_count"]=stats[14];
  e["source_rgba_finite"]=stats[4];e["source_alpha_finite"]=stats[5];e["source_alpha_preserved"]=negative?0u:stats[6];e["source_alpha_total"]=pixels;e["visited_pixels"]=stats[11];e["source_rgb_negative"]=source_negative;e["source_rgba_nonfinite"]=rgba-stats[4];
  e["signature_algorithm"]="rgba32f_mix32_xor_sum_v1";e["input_signature_xor32"]=stats[7];e["input_signature_sum32"]=stats[8];e["output_signature_xor32"]=stats[9];e["output_signature_sum32"]=stats[10];e["current_input_signature_match"]=true;e["current_input_digest_match"]=true;
  e["output_digest_algorithm"]="fnv1a64_rgba32f_bytes";e["output_digest_bytes"]=bytes;e["output_digest"]=od;
  e["guide_input_digests"]=guide_inputs;e["guide_validation"]=guide_validation;
  e["cpu_stats_readback_ms"]=cpu_now_ms()-cpu_stats_start;e["evaluate_cpu_call_ms"]=evaluate_cpu_ms;e["gpu_fence_wait_ms"]=gpu_wait_ms;e["gpu_submit_and_wait_ms"]=submit_and_wait_ms;
  e["control_input_read_ms"]=control_input_read_ms;e["control_input_poll_count"]=input_poll.poll_count;e["control_input_sleep_calls"]=input_poll.sleep_calls;e["control_input_poll_interval_ms"]=1;e["control_input_sleep_wall_ms"]=input_poll.sleep_wall_ms;e["control_input_wait_scope"]="active input wait/read/parse includes producer work; not isolated IPC";
  e["gpu_timestamps"]=gpu_times();e["query_readback_bytes"]=timestamps?query_count*16u:0u;
  e["sentinel"]=-1234;e["nr_exposure"]=1;e["nr_pre_exposure"]=1;e["frame_elapsed_ms"]=cpu_now_ms()-frame_start;e["frame_elapsed_clock"]="QueryPerformanceCounter";e["frame_elapsed_scope"]="input accepted through completion snapshot; excludes publication and consumed wait";
  e["helper_pixel_readback_bytes"]=no_op?77414400u:94003200u;e["guide_readback_bytes"]=11059200u;e["gpu_validator_readback_bytes"]=no_op?0u:64u;e["audit_readback_bytes"]=no_op?77414400u:94003264u;
  frame_deadline("real_GPU_full_pixels_guides_and_dumps_complete");
  printf("QUALITY_COMPLETE frame=%u source=%llu copy=%u Evaluate=%u result=%d resolve=%u GPU_complete=1 source_digest=%s output_digest=%s guide_depth_digest=%s guide_mv_digest=%s pixel_bytes=%u dump_count=%u phase_UNVERIFIED=1\n",frame_index,(unsigned long long)source_game_frame,no_op?1u:0u,no_op||negative?0u:1u,er,no_op||negative?0u:1u,id.c_str(),od.c_str(),string_value(guide_validation.at("depth"),"digest").c_str(),string_value(guide_validation.at("mv"),"digest").c_str(),no_op?77414400u:94003200u,no_op?4u:5u);
  const auto publish_start=cpu_now_ms();atomic_text(frame_path(frame_index,name("complete",frame_index)),e.dump()+"\n");const auto publish_ms=cpu_now_ms()-publish_start;
  const auto consumed_start=cpu_now_ms();validate_tuple(read_active_control(frame_index,name("consumed",frame_index),consumed_poll),frame_index,"consumed","CONSUMED");
  printf("CONTROL_TIMING frame=%u input_read_ms=%.6f complete_publish_ms=%.6f consumed_wait_ms=%.6f cpu_clock=QueryPerformanceCounter active_poll_interval_ms=1 input_poll_count=%llu input_sleep_calls=%llu consumed_poll_count=%llu consumed_sleep_calls=%llu\n",frame_index,control_input_read_ms,publish_ms,cpu_now_ms()-consumed_start,(unsigned long long)input_poll.poll_count,(unsigned long long)input_poll.sleep_calls,(unsigned long long)consumed_poll.poll_count,(unsigned long long)consumed_poll.sleep_calls);++frames_done;
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
  VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};app.apiVersion=VK_API_VERSION_1_2;app.pApplicationName="Quality5 NR-only / model-free noop";VkInstanceCreateInfo ci{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};ci.pApplicationInfo=&app;ci.enabledExtensionCount=1;ci.ppEnabledExtensionNames=&instance_ext;api(I(vkCreateInstance)(&ci,nullptr,&instance),"CreateInstance");
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
  model_initialized=true;feature_created_ever=true;
  phase_tick("feature_create_gpu");
  nr=std::make_unique<N>(instance,physical,device,cmd,queue,gipa);nr->gpu_timeout_ms=gpu_timeout_ms;nr->shader();
  query(VK_FORMAT_R16G16B16A16_SFLOAT,"private_raw97_ACTUAL_MODEL_TARGET",false,width,height);
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

 void init_quality_private(){
  if(no_op){
   nr=std::make_unique<N>(instance,physical,device,cmd,queue,gipa);nr->gpu_timeout_ms=gpu_timeout_ms;nr->create_noop_fence();
   source_read=nr->buffer(33177600,VK_BUFFER_USAGE_TRANSFER_DST_BIT);out_read=nr->buffer(33177600,VK_BUFFER_USAGE_TRANSFER_DST_BIT);
   begin();submit();puts("NOOP_PRIVATE_READY model_libraries_loaded=0 model_init=0 feature_create=0 raw_allocations=0 shader_algorithm_calls=0 GPU_complete=1");
  }else init_model();
  depth_read=nr->buffer(3686400,VK_BUFFER_USAGE_TRANSFER_DST_BIT);mv_read=nr->buffer(7372800,VK_BUFFER_USAGE_TRANSFER_DST_BIT);
  printf("QUALITY_PRIVATE_READY guide_readback_bytes=11059200 helper_pixel_readback_bytes=%u all_before_same_state_baseline=1\n",no_op?77414400u:94003200u);
 }
 Json inspect_guides(){
  size_t df=0,dz=0,mf=0;const auto*d=static_cast<const float*>(depth_read.mapped);const auto*m=static_cast<const float*>(mv_read.mapped);
  for(size_t i=0;i<921600;i++){df+=std::isfinite(d[i]);dz+=std::isfinite(d[i])&&d[i]>=0&&d[i]<=1;}
  for(size_t i=0;i<1843200;i++)mf+=std::isfinite(m[i]);
  const auto dh=fnv_bytes(d,3686400),mh=fnv_bytes(m,7372800);bool dm=dh==string_value(guide_inputs.at("depth"),"digest"),mm=mh==string_value(guide_inputs.at("mv"),"digest");
  Json g{{"depth",Json{{"algorithm","fnv1a64_r32f_bytes"},{"bytes",3686400u},{"digest",dh},{"samples",921600u},{"finite",df},{"device_z_in_0_1",dz},{"source_digest_match",dm}}},{"mv",Json{{"algorithm","fnv1a64_rg32f_bytes"},{"bytes",7372800u},{"digest",mh},{"components",1843200u},{"finite",mf},{"source_digest_match",mm}}},{"scope","same_texel_transport_only_NR_phase_UNVERIFIED"}};
  last_diagnostics["guide_validation"]=g;
  require(df==921600&&dz==921600&&mf==1843200&&dm&&mm,"actual acquired720 guide finite/range/current byte digest failed");return g;
 }
 void shutdown(){
  if(no_op){require(!in_flight&&images.empty()&&nr&&nr->safe,"noop GPU/peer quiescence");nr.reset();shutdown_transport();return;}
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
 if(argc!=4){fputs("Usage: wine_game_nr_quality.exe noop|single|skipEvaluate|resident RUN_DIR_WINDOWS SESSION\n",stderr);return 2;}
 QualityHelper c;c.dir=argv[2];c.session=argv[3];c.mode=argv[1];c.no_op=c.mode=="noop";c.negative=c.mode=="skipEvaluate";
 if(!freopen((c.dir+"\\wine-helper.log").c_str(),"w",stdout)||!freopen((c.dir+"\\wine-helper-stderr.log").c_str(),"w",stderr))return 4;setvbuf(stdout,nullptr,_IONBF,0);
 try{
  require((c.mode=="noop"||c.mode=="single"||c.mode=="resident"||c.mode=="skipEvaluate")&&safe_token(c.session),"Quality isolated modes/safe unique session");
  c.config();c.environment();atomic_text(c.dir+"\\pid-request.json",Json{{"protocol_version",5u},{"transport_layout",quality_paths::layout},{"execution_mode",c.execution_mode},{"profile_id",c.profile},{"quality_profile",c.immutable_profile},{"session",c.session},{"exe",module(GetModuleHandleA(nullptr))},{"win_pid",GetCurrentProcessId()}}.dump()+"\n");
  auto pt=read_bounded(c.dir,"native.pid",c.control_timeout_ms);char*end=nullptr;auto pid=strtoul(pt.c_str(),&end,10);while(end&&(*end=='\r'||*end=='\n'||*end==' '))++end;require(end&&!*end&&pid>1&&pid<=0x7fffffff,"native verified actual exporter Unix PID",3);c.linux_pid=uint32_t(pid);
  printf("ACTUAL_LINUX_PID pid=%u authority=native_ancestor_verified session=%s\n",c.linux_pid,c.session.c_str());
  c.cold_start=c.cpu_now_ms();c.init_nr_device();c.phase_tick("device_ready");c.init_quality_private();c.phase_tick("all_private_ready");
  c.baseline();c.exports();for(c.frame_index=0;c.frame_index<c.frame_count;c.frame_index++){
   if(c.frame_index==0||c.frame_index==63||c.frame_index==64||c.frame_index==127||c.frame_index==128||c.frame_index+1==c.frame_count)c.report_population("before_frame",c.frame_index);
   c.frame();if(c.frame_index==0||c.frame_index==63||c.frame_index==64||c.frame_index==127||c.frame_index==128||c.frame_index+1==c.frame_count)c.report_population("after_consumed",c.frame_index);
  }
  c.retire_shared();c.shutdown();
  auto result=c.envelope(c.frame_count,"result","PASS");result["frames"]=c.frames_done;result["actual_linux_pid"]=c.linux_pid;
  result["model_init"]=c.model_initialized;result["feature_created"]=c.feature_created_ever;result["copy_calls"]=c.copies;result["evaluate_calls"]=c.evaluates;result["resolve_calls"]=c.resolves;
  result["evaluate_calls_total"]=c.evaluates;result["resolve_calls_total"]=c.resolves;result["dump_count"]=c.dumps_written;
  result["normal_release"]=true;result["pixels_IPC"]=false;result["external_GPU_sync"]="NOT_RUN";result["NR_phase"]="UNVERIFIED";
  atomic_text(c.dir+"\\wine-result.json",result.dump()+"\n");
  printf("PASS QUALITY_HELPER mode=%s frames=%u copies=%u evaluates=%u resolves=%u NORMAL_EXIT=0 phase_UNVERIFIED=1\n",c.mode.c_str(),c.frames_done,c.copies,c.evaluates,c.resolves);return 0;
 }catch(const std::exception&e){
  auto stop=dynamic_cast<const Stop*>(&e);const int code=stop?stop->code:2;
  printf("%s reason=%s exit=%d frames=%u evaluates=%u resolves=%u in_flight=%u normal_release=0 cleanup=PRIVATE_PROCESS_EXIT_ONLY\n",code==3?"BLOCKED":"FAIL",e.what(),code,c.frames_done,c.evaluates,c.resolves,unsigned(c.in_flight));
  try{
   auto result=c.envelope(c.frame_index,"result",code==3?"BLOCKED":"FAIL");result["completed_GPU"]=c.frame_gpu_complete;result["reason"]=e.what();result["exit_code"]=code;result["frames"]=c.frames_done;result["model_init"]=c.model_initialized;result["feature_created"]=c.feature_created_ever;result["copy_calls"]=c.copies;result["evaluate_calls"]=c.evaluates;result["resolve_calls"]=c.resolves;result["normal_release"]=false;result["cleanup"]="PRIVATE_PROCESS_EXIT_NOT_NORMAL_FD_RECLAIM";result["diagnostic_failure_details"]=c.last_diagnostics;
   atomic_text(c.dir+"\\wine-result.json",result.dump()+"\n");
  }catch(...){}
  c.validator.release();c.nr.release();return code;
 }
}
