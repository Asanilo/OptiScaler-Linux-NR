// SPDX-License-Identifier: GPL-3.0-or-later
// Protocol6 managed Quality resident. Root's single p0-wire-spec.json is canonical.
#include "playable_transport.h"
#include "playable_control.h"
#include "playable_frame.h"
#include "/home/arinp/Code/dlss5_linux/OptiScaler-Linux-NR/build_out/nr-runtime-poc-20261010/poc/nr_runtime/parameter_abi.h"
#include <optional>
#include <sstream>
#include <cerrno>
using Library=HMODULE;
template<class T>static T get(Library l,const char*n){auto f=reinterpret_cast<T>(GetProcAddress(l,n));require(f,n,3);return f;}
static std::wstring wide(const std::string&s){return std::wstring(s.begin(),s.end());}
static std::string winpath(const std::string&s){std::string p="Z:"+s;std::replace(p.begin(),p.end(),'/','\\');return p;}
static const std::string model_path=winpath("/home/arinp/Code/dlss5_linux/dll/runtime/nvngx_dlssnr.dll");
static const std::string forwarder_path=winpath("/home/arinp/Code/dlss5_linux/OptiScaler-Linux-NR/build_out/nr-runtime-poc-20261010/build_out/poc-runtime-20261010/nvngx.dll_dlssnr.dll");
static const std::string core_path=winpath("/usr/lib64/nvidia/wine/_nvngx.dll");
static const std::string nvapi_path=winpath("/home/arinp/.local/share/Steam/compatibilitytools.d/GE-Proton11-7-x86_64/files/lib/wine/nvapi/x86_64-windows/nvapi64.dll");
static const std::string dxgi_path=winpath("/home/arinp/.local/share/Steam/compatibilitytools.d/GE-Proton11-7-x86_64/files/lib/wine/dxvk/x86_64-windows/dxgi.dll");
static void dump_bytes(const std::string&path,const void*data,size_t bytes){FILE*f=fopen(path.c_str(),"wb");require(f,"diagnostic dump");bool ok=fwrite(data,1,bytes,f)==bytes;bool close=fclose(f)==0;require(ok&&close,"diagnostic write/close");}
struct PlayableHelper:PlayableTransport {
 using N=NrSharedProduction;Library fw{},core{},nvapi{},dxgi{};NVSDK_NGX_Parameter*params{};void*feature{};
 std::unique_ptr<N>nr;std::unique_ptr<PlayableQualityFrame>gpu;
 N::Image shared_color{},shared_depth{},shared_motion{},shared_output{};
 std::string mode,execution_mode,profile=p0_wire::profile_id,expected_device,expected_driver,input_digest,reset_reason;
 uint64_t source_game_frame=0,prev_source_game_frame=0,game_pid=0,game_start_ticks=0;uint32_t timestamp_bits=0;
 unsigned frame_count=0,frame_index=0,nr_reset=1,frames_done=0,evaluates=0,resolves=0,dumps_written=0,feature_create_calls=0,feature_release_calls=0;
 bool diagnostic=false,negative=false,model_initialized=false,feature_created_ever=false,frame_gpu_complete=false,normal_shutdown=false;
 double service_start=0,cold_start=0,frame_start=0,last_peer_check=-1;uint32_t service_wall_seconds=0,control_timeout_ms=60000,cold_init_timeout_ms=120000,frame_completion_timeout_ms=30000,native_soft_deadline_ms=500;
 p0_wire::ControlState managed;Json actual_frame_params=Json::object(),immutable_profile=Json::object(),guide_inputs=Json::object(),last_diagnostics=Json::object(),last_complete=Json::object();
 uint32_t input_xor=0,input_sum=0;double ready_since=0;std::string terminal_reason;
 static double cpu_now_ms(){return c4_cpu_now_ms();}
 const char*validation_profile()const{return diagnostic?"p0_diagnostic":"p0_hot_path";}
 Json validation_fields()const{if(diagnostic)return Json{{"validation_profile","p0_diagnostic"},{"audit_kind","full"},{"dump_count",0u}};return Json{{"validation_profile","p0_hot_path"},{"audit_kind","off"},{"numerical_validation","NOT_CHECKED"},{"pixel_validation","NOT_RUN"},{"signature","NOT_RUN"},{"dump_count",0u}};}
 static std::optional<std::string>try_text(const std::string&path){
  const DWORD a=GetFileAttributesA(path.c_str());if(a==INVALID_FILE_ATTRIBUTES){auto e=GetLastError();require(e==ERROR_FILE_NOT_FOUND||e==ERROR_PATH_NOT_FOUND,"control path unreadable",3);return {};}
  require(!(a&(FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT)),"regular non-reparse control file",3);FILE*f=fopen(path.c_str(),"rb");if(!f){if(errno==ENOENT)return {};throw Stop(3,"control fopen failed");}
  std::string text;char buf[4096];size_t n;while((n=fread(buf,1,sizeof buf,f))){text.append(buf,n);if(text.size()>65536){fclose(f);throw Stop(2,"control64KiB bound");}}const bool ok=!ferror(f),closed=fclose(f)==0;require(ok&&closed,"control read/close");return text;
 }
 static std::pair<char,uint64_t>proc_identity(uint64_t pid){
  auto s=try_text("Z:\\proc\\"+std::to_string(pid)+"\\stat");require(s.has_value(),"numeric native peer proc missing",3);const auto p=s->rfind(')');require(p!=std::string::npos&&p+2<s->size(),"peer proc format",3);std::istringstream in(s->substr(p+2));std::string v;char state=0;uint64_t ticks=0;
  for(unsigned field=3;field<=22;field++){require(bool(in>>v),"peer proc22 fields",3);if(field==3){require(v.size()==1,"peer state",3);state=v[0];}if(field==22){require(!v.empty()&&std::all_of(v.begin(),v.end(),[](char c){return c>='0'&&c<='9';}),"peer start_ticks digits",3);size_t pos=0;ticks=std::stoull(v,&pos);require(pos==v.size()&&ticks>0,"peer start ticks",3);}}
  return{state,ticks};
 }
 void check_peer(bool force=false){const auto now=cpu_now_ms();if(!game_pid||(!force&&now-last_peer_check<250))return;auto p=proc_identity(game_pid);require(p.second==game_start_ticks&&p.first!='Z'&&p.first!='X'&&p.first!='x',"native peer PID/start_ticks changed/zombie",3);last_peer_check=now;}
 void service_check(){
  const auto now=cpu_now_ms();require(service_start>0&&now-service_start<1000.0*service_wall_seconds,"finite service wall expired; abnormal hold not normal release",3);
  require(!try_text(dir+"\\native.abort").has_value(),"native abort/quarantine; no normal release",3);check_peer();
 }
 void phase_tick(const char*stage){service_check();const double elapsed=cpu_now_ms()-cold_start;printf("COLD_PHASE stage=%s elapsed_ms=%.6f budget_ms=120000 synchronous_API_cancel=NOT_AVAILABLE\n",stage,elapsed);require(std::isfinite(elapsed)&&elapsed>=0&&elapsed<120000,"cold readiness deadline; HOLD",3);}
 void frame_deadline(const char*stage){service_check();double t=cpu_now_ms()-frame_start;require(std::isfinite(t)&&t>=0&&t<30000,"active frame30s expired; HOLD not idle timeout",3);printf("ACTIVE_FRAME phase=%s elapsed_QPC_ms=%.6f budget_ms=30000\n",stage,t);}
 Json envelope(uint64_t f,const char*op,const char*status,bool valid=false)const{
  Json j{{"protocol_version",6u},{"transport_layout",p0_wire::layout},{"execution_mode",execution_mode},{"session",session},{"generation",0u},{"frame",f},{"resource_set",resource_set()},{"source_game_frame",source_game_frame},{"source_frame_valid",source_game_frame!=0},{"profile_id",profile},{"width",1920u},{"height",1080u},{"render_width",1280u},{"render_height",720u},{"quality_profile",immutable_profile},{"mode",mode},{"status",status},{"op",op},{"completed_GPU",true},{"output_valid",valid},{"model_valid",valid},{"history_version",managed.history},{"params_version",1u},{"finalized_count",managed.finalized},{"accepted_count",managed.accepted},{"discarded_count",managed.discarded},{"parameter_application",playable_parameter_application()}};j.update(validation_fields());return j;
 }
 static void check_layout(const Json&j){require(string_value(j,"transport_layout")==p0_wire::layout,"explicit frame-buckets layout",3);}
 void common_tuple(const Json&j,uint64_t f,bool source=true){
  check_layout(j);require(uint_value(j,"protocol_version")==6&&string_value(j,"execution_mode")==execution_mode&&string_value(j,"profile_id")==profile,"protocol6/profile/execution exact",3);
  require(j.contains("quality_profile")&&p0_wire::exact_profile(j.at("quality_profile"))&&j.at("quality_profile")==immutable_profile,"experimental immutable profile exact",3);
  require(string_value(j,"session")==session&&uint_value(j,"generation")==0&&uint_value(j,"frame")==f&&string_value(j,"resource_set")==resource_set(),"current session/gen/frame/resource",3);
  require(string_value(j,"mode")==mode&&uint_value(j,"width")==1920&&uint_value(j,"height")==1080&&uint_value(j,"render_width")==1280&&uint_value(j,"render_height")==720,"Quality actual role sizes/mode",3);
  require(string_value(j,"validation_profile")==validation_profile()&&string_value(j,"audit_kind")== (diagnostic?"full":"off"),"exact validation scope",3);
  if(source)require(uint_value(j,"source_game_frame")==source_game_frame,"actual source ID",3);if(j.contains("source_frame_valid"))require(bool_value(j,"source_frame_valid")== (uint_value(j,"source_game_frame")!=0),"actual source valid flag",3);
  if(j.contains("parameter_application"))require(j.at("parameter_application")==playable_parameter_application(),"truthful parameter application",3);
  if(!diagnostic)require(string_value(j,"numerical_validation")=="NOT_CHECKED"&&string_value(j,"pixel_validation")=="NOT_RUN"&&string_value(j,"signature")=="NOT_RUN"&&uint_value(j,"dump_count")==0,"hot required scope",3);
 }
 void validate_tuple(const Json&j,uint64_t f,const char*op,const char*status,bool source=true){common_tuple(j,f,source);require(string_value(j,"op")==op&&string_value(j,"status")==status&&bool_value(j,"completed_GPU")&&!bool_value(j,"output_valid")&&!bool_value(j,"model_valid"),"control op/status/GPU invalid",3);}
 std::string frame_dir(uint64_t f)const{return p0_wire::frame_dir(dir,f,frame_count,'\\');}
 std::string frame_path(uint64_t f,const std::string&file)const{return p0_wire::frame_path(dir,f,frame_count,file,'\\');}
 Json wait_root(const std::string&filename){const auto start=cpu_now_ms();while(cpu_now_ms()-start<60000){service_check();if(auto text=try_text(dir+"\\"+filename))return parse_control(*text);Sleep(5);}throw Stop(3,"identity/control60s missing "+filename);}
 void wait_control(uint64_t f,const char*file,const char*op,const char*status){auto j=wait_root(name(file,f,false));validate_tuple(j,f,op,status);if(file==std::string("native-released")||file==std::string("native-audited")){require(uint_value(j,"finalized_count")==managed.finalized&&uint_value(j,"accepted_count")==managed.accepted&&uint_value(j,"discarded_count")==managed.discarded&&bool_value(j,"source_frame_valid")== (managed.finalized!=0),"actual terminal N/count/source proof",3);}}
 void publish_terminal(uint64_t f,const char*file,const char*op,const char*status){auto j=envelope(f,op,status);j["completed_count"]=managed.finalized;j["terminal_reason"]=terminal_reason;atomic_text(dir+"\\"+name(file,f,false),j.dump()+"\n");}
 static bool hex32(const std::string&s){return s.size()==32&&std::all_of(s.begin(),s.end(),[](char c){return(c>='0'&&c<='9')||(c>='a'&&c<='f');});}
 static bool hex16(const std::string&s){return s.size()==16&&std::all_of(s.begin(),s.end(),[](char c){return(c>='0'&&c<='9')||(c>='a'&&c<='f');});}
 void verify_buckets(){auto directory=[](const std::string&p){auto a=GetFileAttributesA(p.c_str());require(a!=INVALID_FILE_ATTRIBUTES&&(a&FILE_ATTRIBUTE_DIRECTORY)&&!(a&FILE_ATTRIBUTE_REPARSE_POINT),"native precreated non-reparse frame dir",3);};directory(dir);directory(dir+"\\frames");for(uint64_t b=0;b<p0_wire::bucket_count(frame_count);b++)directory(frame_dir(b*64));}
 void config(){auto j=parse_control(read_bounded(dir,"game-config.json",10000));p0_wire::config(j,session,mode);execution_mode=string_value(j,"execution_mode");diagnostic=execution_mode=="diagnostic";frame_count=unsigned(uint_value(j,"frame_count"));immutable_profile=j.at("quality_profile");service_wall_seconds=unsigned(uint_value(j,"service_wall_seconds"));service_start=cpu_now_ms();native_soft_deadline_ms=unsigned(uint_value(j,"native_soft_deadline_ms"));expected_device=string_value(j,"device_uuid");expected_driver=string_value(j,"driver_uuid");require(hex32(expected_device)&&hex32(expected_driver),"actual device/driver UUID syntax",3);width=1920;height=1080;managed.session=session;managed.execution=execution_mode;managed.limit=frame_count;verify_buckets();puts("CONFIG protocol6 Quality1080/guides720 single_slot=1 EXPERIMENTAL_scale1_phase_none_UNVERIFIED runtime_application_truthful=1");}
 void peer_identity(){auto j=wait_root("native-exporter-identity.json");require(uint_value(j,"protocol_version")==6&&string_value(j,"execution_mode")==execution_mode&&string_value(j,"session")==session&&uint_value(j,"exporter_pid")==linux_pid,"verified native identity tuple",3);require(bool_value(j,"descendant_game")&&bool_value(j,"descendant_launcher")&&bool_value(j,"request_exe_match")&&bool_value(j,"unique_run_cmd")&&bool_value(j,"exact_pe_maps"),"verified native ancestor/PE authority",3);game_pid=uint_value(j,"game_pid");game_start_ticks=uint_value(j,"game_start_ticks");require(game_pid>1&&game_pid<=0x7fffffff&&game_start_ticks>0,"numeric actual game PID/start_ticks",3);check_peer(true);printf("NATIVE_PEER pid=%llu start_ticks=%llu same_numeric_proc_nonZombie=1 private_API=0\n",(unsigned long long)game_pid,(unsigned long long)game_start_ticks);}
 void environment(){for(const char*k:{"DXVK_ENABLE_NVAPI","WINEDLLPATH","WINEDLLOVERRIDES","NVIDIA_WINE_DLL_DIR","WINEDEBUG","DXVK_NVAPI_LOG_LEVEL"})printf("CHILD_ENV %s=%s\n",k,getenv(k)?getenv(k):"<unset>");const char*ge="/home/arinp/.local/share/Steam/compatibilitytools.d/GE-Proton11-7-x86_64/files/lib/";require(getenv("DXVK_ENABLE_NVAPI")&&std::string(getenv("DXVK_ENABLE_NVAPI"))=="1","NVAPI enable",3);require(getenv("WINEDLLPATH")&&std::string(getenv("WINEDLLPATH"))==std::string(ge)+"vkd3d:"+ge+"wine","exact installed DLL path",3);require(getenv("WINEDLLOVERRIDES")&&std::string(getenv("WINEDLLOVERRIDES"))=="mscoree,mshtml=;nvapi64=n;dxgi=n","exact native override",3);require(getenv("NVIDIA_WINE_DLL_DIR")&&std::string(getenv("NVIDIA_WINE_DLL_DIR"))=="/usr/lib64/nvidia/wine","exact host NGX",3);}
 void baseline(){auto j=envelope(0,"baseline","BASELINE");j["model_init"]=model_initialized;j["feature_created"]=feature_created_ever;j["feature_create_calls"]=feature_create_calls;j["creation_GPU_complete"]=!in_flight&&serial>0;j["cold_init_timeout_ms"]=120000;j["control_timeout_ms"]=60000;j["gpu_timeout_ms"]=10000;j["frame_completion_timeout_ms"]=30000;atomic_text(dir+"\\"+name("baseline",0,false),j.dump()+"\n");wait_control(0,"native-baseline","baseline_audit","BASELINE_AUDITED");}
 void exports(){create_images();shared_color=N::wrap(images[0].image,images[0].view,images[0].format,false,images[0].width,images[0].height);shared_depth=N::wrap(images[1].image,images[1].view,images[1].format,false,images[1].width,images[1].height);shared_motion=N::wrap(images[2].image,images[2].view,images[2].format,false,images[2].width,images[2].height);shared_output=N::wrap(images[3].image,images[3].view,images[3].format,true,images[3].width,images[3].height);initial_release();auto j=parse_control(metadata());j.update(envelope(0,"metadata","READY"));j["max_frames"]=frame_count;j["frame_count"]=frame_count;j["history_version"]=1;j["params_version"]=1;j["cold_init_timeout_ms"]=120000;j["control_timeout_ms"]=60000;j["gpu_timeout_ms"]=10000;j["frame_completion_timeout_ms"]=30000;j["manual_white_point"]=1;j["passthrough"]=false;j["service_wall_seconds"]=service_wall_seconds;j["native_soft_deadline_ms"]=native_soft_deadline_ms;atomic_text(dir+"\\meta-g0.json",j.dump()+"\n");wait_control(0,"imported","imported","IMPORTED");retire_fds();auto f=envelope(0,"fd_retired","FD_RETIRED");f["closed_fd"]=Json::parse(closed_fds);atomic_text(dir+"\\"+name("fd-retired",0,false),f.dump()+"\n");ready_since=cpu_now_ms();}
 bool managed_control(){
  auto text=try_text(dir+"\\"+name("control-request",0,false));if(!text)return false;auto j=parse_control(*text);require(!in_flight&&nr&&nr->safe,"control ACK requires actual GPU quiescence",3);auto d=managed.apply(j);
  if(d==p0_wire::ControlState::Applied){atomic_text(dir+"\\"+name("control-ack",0,false),managed.last_ack.dump()+"\n");ready_since=cpu_now_ms();if(managed.stopping)terminal_reason=string_value(j,"reason");printf("CONTROL_ACK epoch=%llu action=%s finalized=%llu accepted=%llu discarded=%llu source_valid=%u history=%llu actual_GPU_quiescence=1\n",(unsigned long long)managed.epoch,string_value(j,"action").c_str(),(unsigned long long)managed.finalized,(unsigned long long)managed.accepted,(unsigned long long)managed.discarded,unsigned(managed.finalized!=0),(unsigned long long)managed.history);}
  // An identical accepted request already has its atomically published ACK; no repeat action/IO flood.
  return d==p0_wire::ControlState::Applied;
 }
 std::optional<Json>next_input(){for(;;){service_check();managed_control();if(managed.stopping)return {};
   if(!managed.idle&&managed.finalized<frame_count){if(auto t=try_text(frame_path(managed.finalized,name("input",managed.finalized)))){require(!managed.discarded,"late-discarded session cannot accept another source; drain required",3);return parse_control(*t);}}
   if(!managed.idle&&cpu_now_ms()-ready_since>=60000)throw Stop(3,"READY/control60s without input/control; not an idle frame error");Sleep(managed.idle?5:1);
  }}
 void validate_input(const Json&j){frame_index=unsigned(managed.finalized);common_tuple(j,frame_index,false);validate_tuple(j,frame_index,"input","INPUT_READY",false);prev_source_game_frame=managed.last_source;source_game_frame=uint_value(j,"source_game_frame");require(source_game_frame>prev_source_game_frame,"strict actual source monotonic",3);
  reset_reason=p0_wire::reset_reason(frame_index,bool_value(j,"source_reset"),managed.resume_pending,source_game_frame!=prev_source_game_frame+1);nr_reset=reset_reason=="continuous"?0:1;
  require(uint_value(j,"prev_source_game_frame")==prev_source_game_frame&&uint_value(j,"history_version")==managed.history&&uint_value(j,"params_version")==1&&uint_value(j,"nr_reset")==nr_reset&&string_value(j,"reset_reason")==reset_reason,"actual reset/history precedence",3);
  require(bool_value(j,"sr_history_jitter_enabled_from_start")&&bool_value(j,"sr_initial_reset_observed")&&!bool_value(j,"nr_jitter_parameter_present"),"actual j1 SR history/no NR setter",3);require(uint_value(j,"sr_initial_source_game_frame")<=source_game_frame&&finite_value(j,"source_frame_ms")>0,"actual SR chronology/frame duration",3);
  for(const char*k:{"viewport_jitter_x","viewport_jitter_y","previous_viewport_jitter_x","previous_viewport_jitter_y","sr_jitter_x","sr_jitter_y","previous_sr_jitter_x","previous_sr_jitter_y","sr_mvec_scale_x","sr_mvec_scale_y"})(void)finite_value(j,k);
  for(const char*k:{"nr_jitter_x","nr_jitter_y","mv_scale_x","mv_scale_y","sr_history_jitter_zero_from_start"})require(!j.contains(k),"old/ambiguous jitter MV fields rejected",3);
  require(bool_value(j,"hdr")&&!bool_value(j,"depth_inverted")&&!bool_value(j,"passthrough")&&finite_value(j,"manual_white_point")==1&&!bool_value(j,"exposure_valid")&&string_value(j,"exposure_source")=="unknown","actual HDR/forwardZ/manual exposure profile",3);
  auto rect=[](const Json&r,uint64_t w,uint64_t h){return r.is_object()&&r.size()==4&&uint_value(r,"x")==0&&uint_value(r,"y")==0&&uint_value(r,"width")==w&&uint_value(r,"height")==h;};require(rect(j.at("valid_rect"),1920,1080),"full output rect",3);
  require(j.at("input_descriptors").is_object()&&j.at("input_descriptors").size()==3,"source descriptor roles",3);for(const auto&role:std::vector<std::string>{"color","depth","mv"}){const auto&d=j.at("input_descriptors").at(role);bool guide=role!="color";require(d.is_object()&&d.size()==7&&uint_value(d,"format")==unsigned(role=="color"?97:role=="depth"?130:83)&&uint_value(d,"aspect")==unsigned(role=="depth"?2:1)&&uint_value(d,"width")==unsigned(guide?1280:1920)&&uint_value(d,"height")==unsigned(guide?720:1080)&&uint_value(d,"layout")==1&&uint_value(d,"usage")<=UINT32_MAX&&rect(d.at("valid_rect"),guide?1280:1920,guide?720:1080),"actual source format/extent/aspect",3);if(diagnostic)require(uint_value(d,"usage")&VK_IMAGE_USAGE_TRANSFER_SRC_BIT,"diagnostic source lawful TransferSrc",3);}
  if(!diagnostic)for(const char*k:{"raw_rgb_finite","raw_rgb_written","resolved_rgba_finite","resolved_rgba_written","source_alpha_preserved","signature_algorithm","input_signature_xor32","input_signature_sum32","input_digest","guide_input_digests"})require(!j.contains(k),"hot must not fabricate pixel/signature/readback claims",3);
  if(diagnostic){input_digest=string_value(j,"input_digest");require(hex16(input_digest)&&string_value(j,"input_digest_algorithm")=="fnv1a64_rgba32f_bytes"&&uint_value(j,"input_digest_bytes")==33177600,"diagnostic source digest",3);require(string_value(j,"signature_algorithm")=="rgba32f_mix32_xor_sum_v1","signature",3);auto x=uint_value(j,"input_signature_xor32"),s=uint_value(j,"input_signature_sum32");require(x<=UINT32_MAX&&s<=UINT32_MAX,"uint32 signature",3);input_xor=uint32_t(x);input_sum=uint32_t(s);guide_inputs=j.at("guide_input_digests");require(guide_inputs.is_object()&&guide_inputs.size()==2,"guide digest roles",3);for(const auto&role:std::vector<std::string>{"depth","mv"}){const auto&d=guide_inputs.at(role);require(d.is_object()&&d.size()==3&&string_value(d,"algorithm")== (role=="depth"?"fnv1a64_r32f_bytes":"fnv1a64_rg32f_bytes")&&uint_value(d,"bytes")==uint64_t(role=="depth"?3686400:7372800)&&hex16(string_value(d,"digest")),"guide diagnostic bytes/digest",3);}}
  actual_frame_params=j;for(const char*k:{"session","generation","frame","resource_set","source_game_frame","profile_id","width","height","render_width","render_height","mode","op","status","completed_GPU","output_valid","model_valid"})actual_frame_params.erase(k);last_diagnostics=Json::object();managed.start();frame_start=cpu_now_ms();frame_gpu_complete=false;
 }
 static uint32_t mix32(uint32_t h){h^=h>>16;h*=0x7feb352du;h^=h>>15;h*=0x846ca68bu;h^=h>>16;return h;}
 static std::array<uint32_t,2>signature(const void*data,size_t words){const auto*p=static_cast<const uint8_t*>(data);uint32_t x=0,sum=0;for(size_t i=0;i<words;i++){uint32_t bits;memcpy(&bits,p+4*i,4);auto v=mix32(bits^(uint32_t(i)*0x9e3779b9u));x^=v;sum+=v;}return{x,sum};}
 Json diagnostic_result(){
  const auto&rb=gpu->diagnostic_buffers();const auto stats=gpu->diagnostic_counters();std::array<uint32_t,16>cpu{};uint64_t negative_source=0;
  const auto*raw16=static_cast<const uint16_t*>(rb.raw.mapped);const auto*sv=static_cast<const float*>(rb.source.mapped);const auto*ov=static_cast<const float*>(rb.output.mapped);
  for(size_t p=0;p<2073600;p++)for(unsigned c=0;c<4;c++){auto i=4*p+c;cpu[4]+=std::isfinite(sv[i]);cpu[2]+=std::isfinite(ov[i]);cpu[3]+=std::isfinite(ov[i])&&ov[i]!=-1234.f;cpu[14]+=ov[i]==-1234.f;if(c<3){const auto rv=N::unhalf(raw16[i]);cpu[0]+=std::isfinite(rv);cpu[1]+=std::isfinite(rv);cpu[13]+=std::isnan(rv);negative_source+=std::isfinite(sv[i])&&sv[i]<0;}else{cpu[5]+=std::isfinite(sv[i]);cpu[6]+=memcmp(&sv[i],&ov[i],4)==0;}}
  auto si=signature(rb.source.mapped,8294400),oi=signature(rb.output.mapped,8294400);cpu[7]=si[0];cpu[8]=si[1];cpu[9]=oi[0];cpu[10]=oi[1];cpu[11]=2073600;
  last_diagnostics["measured_pixel_counts"]=stats;require(cpu==stats,"GPU64B agrees with independent CPU actual readbacks",3);
  require(stats[11]==2073600&&stats[12]==0&&stats[15]==0&&stats[4]==8294400&&stats[5]==2073600&&stats[7]==input_xor&&stats[8]==input_sum,"current source full stats/signature",3);
  require(negative?(stats[0]==0&&stats[1]==0&&stats[13]==6220800&&stats[2]==8294400&&stats[3]==0&&stats[14]==8294400):(stats[0]==6220800&&stats[1]==6220800&&stats[13]==0&&stats[2]==8294400&&stats[3]==8294400&&stats[14]==0&&stats[6]==2073600),"actual raw poison/resolved/alpha checks",3);
  const auto id=fnv_bytes(rb.source.mapped,33177600),od=fnv_bytes(rb.output.mapped,33177600);require(id==input_digest,"current actual source FNV",3);
  const auto*d=static_cast<const float*>(rb.depth.mapped);const auto*m=static_cast<const float*>(rb.motion.mapped);uint64_t df=0,dz=0,mf=0;for(size_t i=0;i<921600;i++){df+=std::isfinite(d[i]);dz+=std::isfinite(d[i])&&d[i]>=0&&d[i]<=1;}for(size_t i=0;i<1843200;i++)mf+=std::isfinite(m[i]);
  auto dh=fnv_bytes(d,3686400),mh=fnv_bytes(m,7372800);require(df==921600&&dz==921600&&mf==1843200&&dh==string_value(guide_inputs.at("depth"),"digest")&&mh==string_value(guide_inputs.at("mv"),"digest"),"actual720 guide finite/range/digest",3);
  const auto suffix="-f"+std::to_string(frame_index);dump_bytes(frame_path(frame_index,"raw"+suffix+".rgba16f"),rb.raw.mapped,16588800);dump_bytes(frame_path(frame_index,"source"+suffix+".rgba32f"),rb.source.mapped,33177600);dump_bytes(frame_path(frame_index,"resolve"+suffix+".rgba32f"),rb.output.mapped,33177600);dump_bytes(frame_path(frame_index,"depth"+suffix+".r32f"),rb.depth.mapped,3686400);dump_bytes(frame_path(frame_index,"mv"+suffix+".rg32f"),rb.motion.mapped,7372800);dumps_written+=5;
  return Json{{"dump_count",5u},{"pixel_counter_source","GPU64B_crosschecked_CPU_full_actual_readback"},{"raw_rgb_finite",stats[0]},{"raw_rgb_written",stats[1]},{"raw_rgb_total",6220800u},{"raw_rgb_nan",stats[13]},{"resolved_rgba_finite",stats[2]},{"resolved_rgba_written",stats[3]},{"resolved_rgba_total",8294400u},{"output_sentinel_count",stats[14]},{"source_rgba_finite",stats[4]},{"source_alpha_finite",stats[5]},{"source_alpha_preserved",negative?0u:stats[6]},{"source_alpha_total",2073600u},{"visited_pixels",stats[11]},{"source_rgb_negative",negative_source},{"source_rgba_nonfinite",8294400u-stats[4]},{"signature_algorithm","rgba32f_mix32_xor_sum_v1"},{"input_signature_xor32",stats[7]},{"input_signature_sum32",stats[8]},{"output_signature_xor32",stats[9]},{"output_signature_sum32",stats[10]},{"current_input_signature_match",true},{"current_input_digest_match",true},{"output_digest_algorithm","fnv1a64_rgba32f_bytes"},{"output_digest_bytes",33177600u},{"output_digest",od},{"guide_input_digests",guide_inputs},{"guide_validation",Json{{"depth",{{"algorithm","fnv1a64_r32f_bytes"},{"bytes",3686400u},{"digest",dh},{"samples",921600u},{"finite",df},{"device_z_in_0_1",dz},{"source_digest_match",true}}},{"mv",{{"algorithm","fnv1a64_rg32f_bytes"},{"bytes",7372800u},{"digest",mh},{"components",1843200u},{"finite",mf},{"source_digest_match",true}}},{"scope","same_texel_transport_only_NR_phase_UNVERIFIED"}}}};
 }
 void exact_frame_finalization(const Json&j){common_tuple(j,frame_index);require(bool_value(j,"completed_GPU")&&!bool_value(j,"output_valid")&&!bool_value(j,"model_valid"),"consumer invalidity and actual last-use proof",3);for(const char*k:{"prev_source_game_frame","history_version","params_version","nr_reset","reset_reason"})require(j.contains(k)&&j.at(k)==actual_frame_params.at(k),"current finalization history/params/reset",3);}
 void await_finalization(){const auto start=cpu_now_ms();for(;;){service_check();require(cpu_now_ms()-start<60000,"current consumed/discard60s missing; HOLD not normal release",3);
   auto consumed=try_text(frame_path(frame_index,name("consumed",frame_index))),discard=try_text(frame_path(frame_index,name("discard",frame_index)));require(!(consumed&&discard),"conflicting consumed/discard for current source",3);
   if(consumed){auto j=parse_control(*consumed);exact_frame_finalization(j);require(string_value(j,"op")=="consumed"&&string_value(j,"status")=="CONSUMED","normal current consumed",3);const bool accepted=bool_value(j,"accepted_for_merge");require(!accepted||(!negative&&last_complete.at("output_valid")==true&&last_complete.at("model_valid")==true),"accepted only valid actual NR; skip false",3);require(!in_flight&&nr->safe&&frame_gpu_complete,"current own GPU complete before finalized count",3);managed.finalize(source_game_frame,accepted,false);break;}
   if(discard){auto j=parse_control(*discard);exact_frame_finalization(j);require(string_value(j,"kind")=="discard"&&string_value(j,"state")=="DISCARDED"&&string_value(j,"reason")=="native_deadline_expired"&&bool_value(j,"no_sample_no_merge"),"proven dedicated late discard only",3);if(j.contains("accepted_for_merge"))require(!bool_value(j,"accepted_for_merge"),"discard cannot claim merge acceptance",3);require(!in_flight&&nr->safe&&frame_gpu_complete,"real Helper complete fence required for discard",3);managed.finalize(source_game_frame,false,true);puts("REAL_LATE_DISCARD finalized_once=1 accepted_for_merge=0 no_sample_no_merge=1 drain_required=1");break;}
   Sleep(1);
  }
  frames_done=unsigned(managed.finalized);ready_since=cpu_now_ms();
 }
 void frame(const Json&input){
  validate_input(input);require(nr->safe&&!in_flight,"prior real GPU fence before reuse",3);begin();gpu->begin(nr_reset!=0,negative,diagnostic);
  for(auto&im:images)barrier(im,VK_IMAGE_LAYOUT_GENERAL,VK_QUEUE_FAMILY_EXTERNAL,family,0,VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
  gpu->record(shared_color,shared_depth,shared_motion,shared_output,feature,params,get<PlayableQualityFrame::Evaluate>(fw,"dlssnr_vk_evaluate"));
  frame_deadline("production_records_returned");
  for(auto&im:images)barrier(im,VK_IMAGE_LAYOUT_GENERAL,family,VK_QUEUE_FAMILY_EXTERNAL,VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT,0,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);gpu->finish_recording();
  const auto wait_start=cpu_now_ms();in_flight=true;nr->submit();in_flight=false;const auto submit_wait=cpu_now_ms()-wait_start;auto applied=gpu->completed();frame_gpu_complete=true;frame_deadline("actual_current_GPU_fence");evaluates+=unsigned(applied.evaluate_calls);resolves+=unsigned(applied.resolve_calls);
  require(negative||(applied.evaluate_calls==1&&applied.evaluate_result==1&&applied.resolve_calls==1),"real NR API/result/resolve; no valid output on failure",3);
  Json e=envelope(frame_index,"complete",negative?"INVALID":"OUTPUT_VALID",!negative);e.update(actual_frame_params);e.update(validation_fields());e["parameter_application"]=playable_parameter_application();e["model_init"]=model_initialized;e["feature_created"]=feature_created_ever;e["evaluate_calls"]=applied.evaluate_calls;e["evaluate_result"]=applied.evaluate_result;e["resolve_calls"]=applied.resolve_calls;e["evaluate_calls_total"]=evaluates;e["resolve_calls_total"]=resolves;e["reset"]=nr_reset;e["evaluate_cpu_call_ms"]=applied.evaluate_CPU_call_ms;e["gpu_fence_wait_ms"]=nr->fence_wait_cpu_ms;e["gpu_submit_and_wait_ms"]=submit_wait;e["GPU_fence_serial"]=nr->serial;e["gpu_timestamps"]=gpu->gpu_times();e["query_readback_bytes"]=e["gpu_timestamps"]["query_readback_bytes"];e["native_soft_deadline_ms"]=native_soft_deadline_ms;
  const auto stats_start=cpu_now_ms();if(diagnostic)e.update(diagnostic_result());e["cpu_stats_readback_ms"]=diagnostic?Json(cpu_now_ms()-stats_start):Json(nullptr);
  e["model_acceptance_scope"]=diagnostic?"API_GPU_tuple_and_full_diagnostic_pixels_phase_UNVERIFIED":"API_GPU_active_tuple_only; pixels NOT_CHECKED; phase_UNVERIFIED";e["helper_pixel_readback_bytes"]=gpu->pixel_readback_bytes();e["gpu_validator_readback_bytes"]=gpu->validator_readback_bytes();e["audit_readback_bytes"]=gpu->pixel_readback_bytes()+gpu->validator_readback_bytes();e["sentinel"]=-1234;e["frame_elapsed_ms"]=cpu_now_ms()-frame_start;e["frame_elapsed_clock"]="QueryPerformanceCounter";e["frame_elapsed_scope"]="input accepted through current completion snapshot; excludes publish/consumer wait";
  frame_deadline("completion_snapshot_and_optional_diagnostics");last_complete=e;
  atomic_text(frame_path(frame_index,name("complete",frame_index)),e.dump()+"\n");printf("P0_COMPLETE session=%s frame=%u source=%llu history=%llu reset=%u Eval=%llu result=%d resolve=%llu GPU_complete=1 mode=%s pixel_bytes=%llu parameter_application=HOST_DECLARATIONS_NOT_SENT\n",session.c_str(),frame_index,(unsigned long long)source_game_frame,(unsigned long long)managed.history,nr_reset,(unsigned long long)applied.evaluate_calls,applied.evaluate_result,(unsigned long long)applied.resolve_calls,execution_mode.c_str(),(unsigned long long)gpu->pixel_readback_bytes());
  await_finalization();
 }
 void retire_shared(){require(managed.stopping&&!managed.active&&!in_flight&&nr&&nr->safe,"normal managed retirement requires stopped finalized GPU-safe state",3);source_game_frame=managed.last_source;frame_index=unsigned(managed.finalized);wait_control(managed.finalized,"native-released","release","NATIVE_RELEASED");free_shared();publish_terminal(managed.finalized,"retired","retired","RETIRED");wait_control(managed.finalized,"native-audited","audit","FD_AUDITED");}

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
  ++feature_create_calls;feature=get<Create>(fw,"dlssnr_vk_create")(cmd,params,width,height,0,1,0,1,1,1,1,0);
  auto*last=reinterpret_cast<int*>(GetProcAddress(fw,"dlssnr_vk_last_create"));printf("MODEL_VK_CREATE handle=%p result=0x%08x\n",feature,last?*last:0);
  submit();require(feature&&last&&*last==1,"model create result after real GPU completion");
  model_initialized=true;feature_created_ever=true;
  phase_tick("feature_create_gpu");
  nr=std::make_unique<N>(instance,physical,device,cmd,queue,gipa);nr->gpu_timeout_ms=gpu_timeout_ms;nr->shader();
  query(VK_FORMAT_R16G16B16A16_SFLOAT,"private_raw97_ACTUAL_MODEL_TARGET",false,width,height);
  gpu=std::make_unique<PlayableQualityFrame>(*nr,diagnostic,timestamp_bits,properties.limits.timestampPeriod);
  phase_tick("persistent_production_and_optional_diagnostic_ready");
 }
 void shutdown(){
  require(!in_flight&&images.empty()&&nr&&nr->safe,"normal model shutdown requires GPU quiescence and release/audit");
  get<void(*)(void*)>(fw,"dlssnr_vk_release")(feature);feature=nullptr;++feature_release_calls;puts("TEARDOWN feature_release completed");
  gpu.reset();
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
  normal_shutdown=true;puts("NORMAL_RELEASE NR=1 shared=1 private=1 core=1 device=1 source_count_actual=1 process_exit_not_raw_fd_strategy=1");
 }
};
int main(int argc,char**argv){
 if(argc!=4){fputs("Usage: wine_game_nr_playable.exe resident|skipEvaluate RUN_DIR_WINDOWS SESSION\n",stderr);return 2;}
 PlayableHelper c;c.dir=argv[2];c.session=argv[3];c.mode=argv[1];c.negative=c.mode=="skipEvaluate";
 if(!freopen((c.dir+"\\wine-helper.log").c_str(),"wb",stdout)||!freopen((c.dir+"\\wine-helper-stderr.log").c_str(),"wb",stderr))return 4;setvbuf(stdout,nullptr,_IONBF,0);
 try{
  require((c.mode=="resident"||c.mode=="skipEvaluate")&&safe_token(c.session),"P0 isolated CLI/session");c.config();c.environment();
  auto request=c.envelope(0,"pid_request","PID_REQUEST");request["exe"]=module(GetModuleHandleA(nullptr));request["win_pid"]=GetCurrentProcessId();atomic_text(c.dir+"\\pid-request.json",request.dump()+"\n");
  auto pt=read_bounded(c.dir,"native.pid",60000);char*end=nullptr;auto pid=strtoul(pt.c_str(),&end,10);while(end&&(*end=='\r'||*end=='\n'||*end==' '))++end;require(end&&!*end&&pid>1&&pid<=0x7fffffff,"verified actual exporter LinuxPID",3);c.linux_pid=uint32_t(pid);c.peer_identity();
  c.cold_start=c.cpu_now_ms();c.init_nr_device();c.phase_tick("device_ready");c.init_model();c.phase_tick("all_private_ready");c.baseline();c.exports();
  while(auto input=c.next_input())c.frame(*input);c.retire_shared();c.shutdown();
  auto result=c.envelope(c.managed.finalized,"result","PASS");result["frames"]=c.managed.finalized;result["completed_count"]=c.managed.finalized;result["actual_linux_pid"]=c.linux_pid;result["model_init"]=c.model_initialized;result["feature_created"]=c.feature_created_ever;result["feature_create_calls"]=c.feature_create_calls;result["feature_release_calls"]=c.feature_release_calls;result["evaluate_calls"]=c.evaluates;result["resolve_calls"]=c.resolves;result["evaluate_calls_total"]=c.evaluates;result["resolve_calls_total"]=c.resolves;result["dump_count"]=c.dumps_written;result["normal_release"]=true;result["normal_shutdown"]=c.normal_shutdown;result["terminal_reason"]=c.terminal_reason;result["pixels_IPC"]=false;result["external_GPU_sync"]="NOT_RUN";result["NR_phase"]="UNVERIFIED";result["native_present_count"]="REQUIRES_NATIVE_LEDGER_NOT_INFERRED";atomic_text(c.dir+"\\wine-result.json",result.dump()+"\n");
  printf("PASS P0 finalized=%llu accepted=%llu discarded=%llu evaluates=%u resolves=%u create=%u release=%u normal_exit=0 actual_terminal_source_valid=%u\n",(unsigned long long)c.managed.finalized,(unsigned long long)c.managed.accepted,(unsigned long long)c.managed.discarded,c.evaluates,c.resolves,c.feature_create_calls,c.feature_release_calls,unsigned(c.managed.finalized!=0));return 0;
 }catch(const std::exception&e){auto stop=dynamic_cast<const Stop*>(&e);int code=stop?stop->code:2;printf("P0_%s reason=%s exit=%d finalized=%llu in_flight=%u normal_release=0 HOLD=1\n",code==3?"BLOCKED":"FAIL",e.what(),code,(unsigned long long)c.managed.finalized,unsigned(c.in_flight));
  try{auto result=c.envelope(c.managed.finalized,"result",code==3?"BLOCKED":"FAIL");result["completed_GPU"]=c.frame_gpu_complete;result["reason"]=e.what();result["exit_code"]=code;result["normal_release"]=false;result["diagnostic_failure_details"]=c.last_diagnostics;result["cleanup"]="PRIVATE_PROCESS_EXIT_NOT_NORMAL_RESOURCE_OR_GPU_COMPLETION_PROOF";atomic_text(c.dir+"\\wine-result.json",result.dump()+"\n");}catch(...){}
  c.gpu.release();c.nr.release();return code;
 }
}
