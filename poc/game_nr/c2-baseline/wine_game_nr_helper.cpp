// SPDX-License-Identifier: GPL-3.0-or-later
// Isolated C2 adapter of frozen successful G3; production algorithm/SPIR-V unchanged.
#include "c2_game_transport.h"
#include "c2_game_control.h"
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
#include "c2_production_adapter.h"
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
 std::unique_ptr<N> nr;N::Image proxy{},raw{},keep{},dummy{},shared_color{},shared_depth{},shared_motion{},shared_output{};
 N::Buffer raw_read{},out_read{},source_read{},encode_cb{},resolve_cb{};
 std::string mode,profile="hdr-run-dlaa-1to1-j0-manual-wp1-v1",expected_device,expected_driver,input_digest;
 uint64_t source_game_frame=0,cold_start=0,frame_start=0;
 uint32_t control_timeout_ms=60000,cold_init_timeout_ms=120000,frame_completion_timeout_ms=30000;
 unsigned evaluates=0,resolves=0,frames_done=0;bool negative=false,model_initialized=false;
 Json actual_frame_params=Json::object();
 void phase_tick(const char*stage){
  require(GetFileAttributesA((dir+"\\native.abort").c_str())==INVALID_FILE_ATTRIBUTES,"native abort/quarantine; no normal release",3);
  const auto elapsed=GetTickCount64()-cold_start;
  printf("COLD_PHASE stage=%s elapsed_ms=%llu budget_ms=%u synchronous_API_cancel=NOT_AVAILABLE\n",stage,(unsigned long long)elapsed,cold_init_timeout_ms);
  require(elapsed<cold_init_timeout_ms,"cold model readiness deadline expired; hold resources",3);
 }
 Json envelope(uint64_t frame,const char*op,const char*status,bool valid=false)const{
  return Json{{"session",session},{"generation",generation},{"frame",frame},{"resource_set",resource_set()},
   {"source_game_frame",source_game_frame},{"profile_id",profile},{"width",width},{"height",height},
   {"render_width",width},{"render_height",height},{"mode",mode},{"status",status},{"op",op},
   {"completed_GPU",true},{"output_valid",valid},{"model_valid",valid}};
 }
 void publish(uint64_t f,const char*file,const char*op,const char*status,bool valid=false,bool numbered=true){
  atomic_text(dir+"\\"+name(file,f,numbered),envelope(f,op,status,valid).dump()+"\n");
 }
 Json read_control(const std::string&filename){return parse_control(read_bounded(dir,filename,control_timeout_ms));}
 void validate_tuple(const Json&j,uint64_t f,const char*op,const char*status,bool match_source=true){
  require(string_value(j,"session")==session&&uint_value(j,"generation")==0&&uint_value(j,"frame")==f&&string_value(j,"resource_set")==resource_set(),"C2 exact session/generation/frame/resource_set mismatch",3);
  require(string_value(j,"profile_id")==profile&&string_value(j,"mode")==mode&&uint_value(j,"width")==1920&&uint_value(j,"height")==1080&&uint_value(j,"render_width")==1920&&uint_value(j,"render_height")==1080,"C2 profile/mode/extents mismatch",3);
  require(string_value(j,"op")==op&&string_value(j,"status")==status&&bool_value(j,"completed_GPU")&&!bool_value(j,"output_valid")&&!bool_value(j,"model_valid"),"C2 control op/status/GPU/validity mismatch",3);
  if(match_source)require(uint_value(j,"source_game_frame")==source_game_frame,"C2 source frame mismatch",3);
  printf("CONTROL_ACCEPT frame=%llu source_game_frame=%llu mode=%s op=%s status=%s exact_tuple=1\n",(unsigned long long)f,(unsigned long long)uint_value(j,"source_game_frame"),mode.c_str(),op,status);
 }
 void wait_control(uint64_t f,const char*file,const char*op,const char*status,bool numbered=true){validate_tuple(read_control(name(file,f,numbered)),f,op,status);}
 static bool hex32(const std::string&v){return v.size()==32&&std::all_of(v.begin(),v.end(),[](char c){return(c>='0'&&c<='9')||(c>='a'&&c<='f');});}
 static bool hex16(const std::string&v){return v.size()==16&&std::all_of(v.begin(),v.end(),[](char c){return(c>='0'&&c<='9')||(c>='a'&&c<='f');});}
 void config(){
  auto j=parse_control(read_bounded(dir,"game-config.json",10000));
  require(string_value(j,"session")==session&&string_value(j,"mode")==mode&&uint_value(j,"generation")==0&&string_value(j,"profile_id")==profile,"only exact C2 mode/profile/session/gen0",3);
  require(uint_value(j,"width")==1920&&uint_value(j,"height")==1080&&uint_value(j,"render_width")==1920&&uint_value(j,"render_height")==1080&&uint_value(j,"max_frames")==1,"C2 actual HDR Run1080 1:1 single frame required",3);
  require(uint_value(j,"cold_init_timeout_ms")==120000&&uint_value(j,"control_timeout_ms")==60000&&uint_value(j,"gpu_timeout_ms")==10000&&uint_value(j,"frame_completion_timeout_ms")==30000,"C2 fixed finite diagnostic budgets required",3);
  require(finite_value(j,"manual_white_point")==1&&!bool_value(j,"passthrough"),"C2 explicit manual whitepoint1/passthrough0",3);
  expected_device=string_value(j,"device_uuid");expected_driver=string_value(j,"driver_uuid");require(hex32(expected_device)&&hex32(expected_driver),"actual captured device/driver UUID syntax",3);
  width=N::W;height=N::H;
  printf("CONFIG C2_SINGLE session=%s mode=%s profile=%s extent=%ux%u frames=1 groups=240,135,1 single_slot=1 cold_ms=120000 control_ms=60000 gpu_ms=10000 completion_ms=30000 NR_ONLY=1 SR=0 FG=0 CPU_PIXEL_IPC=0\n",session.c_str(),mode.c_str(),profile.c_str(),width,height);
 }
 void environment(){
  for(const char*key:{"DXVK_ENABLE_NVAPI","WINEDLLPATH","WINEDLLOVERRIDES","NVIDIA_WINE_DLL_DIR","WINEDEBUG","DXVK_NVAPI_LOG_LEVEL"})printf("CHILD_ENV %s=%s\n",key,getenv(key)?getenv(key):"<unset>");
  const char*ge="/home/arinp/.local/share/Steam/compatibilitytools.d/GE-Proton11-7-x86_64/files/lib/";
  const std::string dllpath=std::string(ge)+"vkd3d:"+ge+"wine";
  require(getenv("DXVK_ENABLE_NVAPI")&&std::string(getenv("DXVK_ENABLE_NVAPI"))=="1","C2 fixture DXVK_ENABLE_NVAPI1",3);
  require(getenv("WINEDLLPATH")&&std::string(getenv("WINEDLLPATH"))==dllpath,"C2 exact installed GE WINEDLLPATH only",3);
  require(getenv("WINEDLLOVERRIDES")&&std::string(getenv("WINEDLLOVERRIDES"))=="mscoree,mshtml=;nvapi64=n;dxgi=n","C2 exact native override",3);
  require(getenv("NVIDIA_WINE_DLL_DIR")&&std::string(getenv("NVIDIA_WINE_DLL_DIR"))=="/usr/lib64/nvidia/wine","C2 exact host NGX directory",3);
  puts("RUNTIME_POLICY pinned_PE_only=1 DLL_copy=0 private_NVAPI_calls=0 own_private_prefix=REQUIRED no_SR_DualFeature_enlarger_FG=1");
 }
 void baseline(){
  auto j=envelope(0,"baseline","BASELINE");j["model_init"]=model_initialized;j["model_feature_created"]=feature!=nullptr;
  j["cold_init_timeout_ms"]=cold_init_timeout_ms;j["control_timeout_ms"]=control_timeout_ms;j["gpu_timeout_ms"]=gpu_timeout_ms;j["frame_completion_timeout_ms"]=frame_completion_timeout_ms;
  atomic_text(dir+"\\"+name("baseline",0,false),j.dump()+"\n");wait_control(0,"native-baseline","baseline_audit","BASELINE_AUDITED",false);
 }
 void exports(){
  create_images();shared_color=N::wrap(images[0].image,images[0].view,images[0].format,false);
  shared_depth=N::wrap(images[1].image,images[1].view,images[1].format,false);
  shared_motion=N::wrap(images[2].image,images[2].view,images[2].format,false);
  shared_output=N::wrap(images[3].image,images[3].view,images[3].format,true);
  initial_release();auto j=parse_control(metadata());j.update(envelope(0,"metadata","READY"));
  j["max_frames"]=1;j["cold_init_timeout_ms"]=cold_init_timeout_ms;j["control_timeout_ms"]=control_timeout_ms;j["gpu_timeout_ms"]=gpu_timeout_ms;j["frame_completion_timeout_ms"]=frame_completion_timeout_ms;
  j["manual_white_point"]=1;j["passthrough"]=false;atomic_text(dir+"\\meta-g0.json",j.dump()+"\n");
  wait_control(0,"imported","imported","IMPORTED",false);retire_fds();auto fd=envelope(0,"fd_retired","FD_RETIRED");fd["closed_fd"]=Json::parse(closed_fds);atomic_text(dir+"\\"+name("fd-retired",0,false),fd.dump()+"\n");
 }
 void input(){
  auto j=read_control(name("input",0));validate_tuple(j,0,"input","INPUT_READY",false);
  source_game_frame=uint_value(j,"source_game_frame");require(source_game_frame>0,"actual selected source frame must be positive",3);
  require(bool_value(j,"hdr")&&!bool_value(j,"exposure_valid")&&string_value(j,"exposure_source")=="unknown","C2 HDR and explicit unknown real exposure required",3);
  require(!bool_value(j,"depth_inverted")&&finite_value(j,"mv_scale_x")==1&&finite_value(j,"mv_scale_y")==1&&uint_value(j,"nr_reset")==1,"C2 real forward depth/MVscale1/new NR reset1",3);
  require(finite_value(j,"nr_jitter_x")==0&&finite_value(j,"nr_jitter_y")==0&&finite_value(j,"manual_white_point")==1&&!bool_value(j,"passthrough"),"C2 model manual params required",3);
  for(const char*key:{"viewport_jitter_x","viewport_jitter_y","previous_viewport_jitter_x","previous_viewport_jitter_y","sr_jitter_x","sr_jitter_y","previous_sr_jitter_x","previous_sr_jitter_y"})require(finite_value(j,key)==0,"actual current/previous viewport/SR jitter must be0",3);
  require(j.contains("valid_rect")&&j["valid_rect"].is_object(),"full actual valid rect object",3);const auto&v=j["valid_rect"];
  require(uint_value(v,"x")==0&&uint_value(v,"y")==0&&uint_value(v,"width")==1920&&uint_value(v,"height")==1080,"C2 full 1:1 actual valid rect required",3);
  require(bool_value(j,"sr_initial_reset_observed")&&bool_value(j,"sr_history_jitter_zero_from_start"),"C2 actual fresh SR history evidence required",3);
  require(uint_value(j,"sr_initial_source_game_frame")<=source_game_frame,"SR initial source chronology",3);
  require(finite_value(j,"source_frame_ms")>0,"actual source frame_ms",3);(void)bool_value(j,"source_reset");
  input_digest=string_value(j,"input_digest");require(hex16(input_digest)&&string_value(j,"input_digest_algorithm")=="fnv1a64_rgba32f_bytes"&&uint_value(j,"input_digest_bytes")==33177600,"C2 raw input digest fields",3);
  actual_frame_params=j;for(const char*key:{"session","generation","frame","resource_set","source_game_frame","profile_id","width","height","render_width","render_height","mode","op","status","completed_GPU","output_valid","model_valid"})actual_frame_params.erase(key);
  printf("ACTUAL_FRAME source_game_frame=%llu source_reset=%u NR_reset=1 current_previous_viewport_SR_jitter=0,0 hdr=1 exposure=unknown manual_whitepoint=1 mvscale=1 same_texel_only=1\n",(unsigned long long)source_game_frame,unsigned(bool_value(j,"source_reset")));
 }
 void frame_deadline(const char*stage){
  const auto elapsed=GetTickCount64()-frame_start;
  printf("FRAME_PHASE stage=%s elapsed_ms=%llu budget_ms=%u diagnostic_only=1\n",stage,(unsigned long long)elapsed,frame_completion_timeout_ms);
  require(elapsed<frame_completion_timeout_ms,"C2 frame completion deadline expired; invalid/quarantine",3);
  require(GetFileAttributesA((dir+"\\native.abort").c_str())==INVALID_FILE_ATTRIBUTES,"native abort/quarantine; no normal release",3);
 }
 void frame(){
  input();frame_start=GetTickCount64();begin();
  for(auto&im:images)barrier(im,VK_IMAGE_LAYOUT_GENERAL,VK_QUEUE_FAMILY_EXTERNAL,family,0,VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT,VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
  auto&color=shared_color;auto&depth=shared_depth;auto&motion=shared_motion;auto&output=shared_output;
  nr->transition(color,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);nr->transition(depth,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);nr->transition(motion,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  nr->clear(raw,NAN);nr->clear(output,-1234.f);nr->transition(proxy,VK_IMAGE_LAYOUT_GENERAL);nr->transition(keep,VK_IMAGE_LAYOUT_GENERAL);nr->transition(dummy,VK_IMAGE_LAYOUT_GENERAL);
  DlssNrConstants c{};c.Mode=DlssNrMode_Encode;c.Width=c.GuideWidth=width;c.Height=c.GuideHeight=height;c.WhitePoint=1;c.TransferStrength=c.ColourStrength=1;c.MaxRatio=2;c.Transfer=1;
  nr->dispatch(c,color,color,color,color,proxy,keep,&encode_cb);nr->transition(proxy,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
  int er=0;constexpr int reset=1;
  printf("FRAME_PARAMS session=%s generation=0 frame=0 resource_set=%s source_game_frame=%llu width=1920 height=1080 guide_width=1920 guide_height=1080 groups=240,135,1 reset=1 depth_inverted=0 mvscale=1,1 jitter=0,0 nr_exposure=1 nr_pre_exposure=1 exposure_source=unknown whitepoint=1 passthrough=0 scene_linear=1 working_scale=1 passes=1 transfer_strength=1 colour_strength=1 max_ratio=2 transfer=1 cache=0 compare=0 debug=0 SR=0 FG=0\n",session.c_str(),resource_set().c_str(),(unsigned long long)source_game_frame);
  frame_deadline("encode_recorded");
  if(!negative){
   using Eval=int(*)(void*,void*,void*,void*,void*,void*,void*,unsigned,unsigned,unsigned,unsigned,int,int,float,int,float,float,float,int,float,float);
   ++evaluates;const auto start=GetTickCount64();er=get<Eval>(fw,"dlssnr_vk_evaluate")(cmd,feature,params,&proxy.ngx,&depth.ngx,&motion.ngx,&raw.ngx,width,height,width,height,0,reset,1,0,1,1,1,1,1,1);
   printf("MODEL_EVALUATE frame=0 source_game_frame=%llu reset=1 calls=%u result=0x%08x CPU_call_ms=%llu\n",(unsigned long long)source_game_frame,evaluates,er,(unsigned long long)(GetTickCount64()-start));
  }else puts("SKIP_EVALUATE frame=0 evaluate_calls=0 resolve_calls=0 raw_poison=NaN output_poison=-1234");
  frame_deadline("evaluate_returned");nr->memoryBarrier();
  if(!negative&&er==1){nr->transition(raw,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);nr->transition(keep,VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);c.Mode=DlssNrMode_Resolve;nr->dispatch(c,proxy,raw,keep,motion,output,dummy,&resolve_cb);++resolves;}
  auto rb=nr->read(raw,true,&raw_read),ob=nr->read(output,false,&out_read),sb=nr->read(color,false,&source_read);
  nr->transition(color,VK_IMAGE_LAYOUT_GENERAL);nr->transition(depth,VK_IMAGE_LAYOUT_GENERAL);nr->transition(motion,VK_IMAGE_LAYOUT_GENERAL);nr->transition(output,VK_IMAGE_LAYOUT_GENERAL);
  for(auto&im:images)barrier(im,VK_IMAGE_LAYOUT_GENERAL,family,VK_QUEUE_FAMILY_EXTERNAL,VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT,0,VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT);
  in_flight=true;nr->submit();in_flight=false;frame_deadline("actual_GPU_fence_readback_complete");require(negative||er==1,"actual model Evaluate failed; no OUTPUT_VALID");
  const size_t pixels=size_t(width)*height,rgb=pixels*3,rgba=pixels*4,bytes=rgba*4;
  size_t raw_finite=0,raw_written=0,out_finite=0,out_written=0,alpha_finite=0,alpha_preserved=0,source_finite=0;
  const auto*rv=static_cast<const uint16_t*>(rb.mapped);const auto*ov=static_cast<const float*>(ob.mapped);const auto*sv=static_cast<const float*>(sb.mapped);
  for(size_t p=0;p<pixels;p++)for(unsigned ch=0;ch<4;ch++){
   const size_t index=4*p+ch;const float rawv=N::unhalf(rv[index]);source_finite+=std::isfinite(sv[index]);
   if(ch<3){raw_finite+=std::isfinite(rawv);raw_written+=std::isfinite(rawv);}
   out_finite+=std::isfinite(ov[index]);out_written+=ov[index]!=-1234.f;
   if(ch==3){alpha_finite+=std::isfinite(sv[index]);alpha_preserved+=memcmp(&sv[index],&ov[index],sizeof(float))==0;}
  }
  const auto actual_input_digest=fnv_bytes(sb.mapped,bytes);const bool input_match=actual_input_digest==input_digest;
  printf("CURRENT_INPUT_SOURCE_COLOR frame=0 source_game_frame=%llu bytes=%zu expected_digest=%s actual_digest=%s digest_match=%u finite=%zu total=%zu raw_bytes=1 keep_equality_NOT_REQUIRED=1\n",(unsigned long long)source_game_frame,bytes,input_digest.c_str(),actual_input_digest.c_str(),unsigned(input_match),source_finite,rgba);
  require(input_match&&source_finite==rgba,"actual shared sourceColor raw input mismatch/nonfinite");
  printf("C2_READBACK frame=0 raw_rgb_finite=%zu raw_rgb_written=%zu raw_rgb_total=%zu resolved_rgba_finite=%zu resolved_rgba_written=%zu resolved_rgba_total=%zu source_alpha_finite=%zu source_alpha_preserved=%zu source_alpha_total=%zu negative=%u actual_GPU_complete=1\n",raw_finite,raw_written,rgb,out_finite,out_written,rgba,alpha_finite,alpha_preserved,pixels,unsigned(negative));
  require(negative?(raw_finite==0&&raw_written==0&&out_finite==rgba&&out_written==0):(raw_finite==rgb&&raw_written==rgb&&out_finite==rgba&&out_written==rgba&&alpha_finite==pixels&&alpha_preserved==pixels),"actual raw/resolve sentinel/finite/sourcealpha verification failed");
  const auto dg=fnv_bytes(ob.mapped,bytes);printf("OUTPUT_DIGEST frame=0 algorithm=fnv1a64_rgba32f_bytes bytes=%zu digest=%s transport_consistency_only=1\n",bytes,dg.c_str());
  // Local artifacts are audit only; neither process consumes the other's CPU pixel files.
  dump_bytes(dir+"\\raw-f0.rgba16f",rb.mapped,pixels*8);dump_bytes(dir+"\\source-f0.rgba32f",sb.mapped,bytes);dump_bytes(dir+"\\resolve-f0.rgba32f",ob.mapped,bytes);
  frame_deadline("stats_digests_local_dumps_done");
  auto e=envelope(0,"complete",negative?"INVALID":"OUTPUT_VALID",!negative);e.update(actual_frame_params);
  e["evaluate_calls"]=evaluates;e["evaluate_result"]=er;e["resolve_calls"]=resolves;e["reset"]=1;
  e["raw_rgb_finite"]=raw_finite;e["raw_rgb_written"]=raw_written;e["raw_rgb_total"]=rgb;
  e["resolved_rgba_finite"]=out_finite;e["resolved_rgba_written"]=out_written;e["resolved_rgba_total"]=rgba;
  e["source_alpha_finite"]=alpha_finite;e["source_alpha_preserved"]=negative?0:alpha_preserved;e["source_alpha_total"]=pixels;
  e["current_input_digest_match"]=input_match;e["output_digest_algorithm"]="fnv1a64_rgba32f_bytes";e["output_digest_bytes"]=bytes;e["output_digest"]=dg;e["sentinel"]=-1234;
  e["nr_exposure"]=1;e["nr_pre_exposure"]=1;e["frame_elapsed_ms"]=GetTickCount64()-frame_start;
  atomic_text(dir+"\\"+name("complete",0),e.dump()+"\n");wait_control(0,"consumed","consumed","CONSUMED");frames_done=1;
 }
 void retire_shared(){
  wait_control(1,"native-released","release","NATIVE_RELEASED",false);require(!in_flight&&nr&&nr->safe,"shared retirement requires actual GPU completion");
  free_shared();publish(1,"retired","retired","RETIRED",false,false);wait_control(1,"native-audited","audit","FD_AUDITED",false);
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
  VkApplicationInfo app{VK_STRUCTURE_TYPE_APPLICATION_INFO};app.apiVersion=VK_API_VERSION_1_2;app.pApplicationName="Game C2 single NR-only";VkInstanceCreateInfo ci{VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};ci.pApplicationInfo=&app;ci.enabledExtensionCount=1;ci.ppEnabledExtensionNames=&instance_ext;api(I(vkCreateInstance)(&ci,nullptr,&instance),"CreateInstance");
  auto core_properties=gipa(instance,"vkGetPhysicalDeviceProperties2"),khr_properties=gipa(instance,"vkGetPhysicalDeviceProperties2KHR");
  printf("PROPERTIES2_ENTRYPOINT core=%p khr=%p both_present=%u\n",reinterpret_cast<void*>(core_properties),reinterpret_cast<void*>(khr_properties),core_properties&&khr_properties);
  require(core_properties&&khr_properties,"properties2 core/KHR dispatch after explicit extension",3);
  uint32_t n=0;api(I(vkEnumeratePhysicalDevices)(instance,&n,nullptr),"PhysicalCount");std::vector<VkPhysicalDevice> ps(n);api(I(vkEnumeratePhysicalDevices)(instance,&n,ps.data()),"PhysicalDevices");
  for(auto p:ps){VkPhysicalDeviceProperties2 pr{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_PROPERTIES_2};pr.pNext=&ids;I(vkGetPhysicalDeviceProperties2)(p,&pr);if(pr.properties.vendorID==0x10de){physical=p;properties=pr.properties;break;}}
  require(physical,"NVIDIA absent",3);I(vkGetPhysicalDeviceMemoryProperties)(physical,&memory);printf("DEVICE name=%s device_uuid=%s driver_uuid=%s driver=%u\n",properties.deviceName,hex(ids.deviceUUID).c_str(),hex(ids.driverUUID).c_str(),properties.driverVersion);
  require(hex(ids.deviceUUID)==expected_device&&hex(ids.driverUUID)==expected_driver,"C2 actual game/helper UUID mismatch",3);
  char luid_hex[17]{};for(unsigned i=0;i<8;i++)snprintf(luid_hex+2*i,3,"%02x",ids.deviceLUID[i]);
  printf("DEVICE_ID_LUID valid=%u luid=%s node_mask=%u host_NGX_requires_validity_unverified=1\n",ids.deviceLUIDValid,luid_hex,ids.deviceNodeMask);
  api(I(vkEnumerateDeviceExtensionProperties)(physical,nullptr,&n,nullptr),"ExtCount");std::vector<VkExtensionProperties> es(n);api(I(vkEnumerateDeviceExtensionProperties)(physical,nullptr,&n,es.data()),"Exts");std::vector<const char*> enable;
  for(const char*s:{VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME,VK_EXT_EXTERNAL_MEMORY_DMA_BUF_EXTENSION_NAME,"VK_NVX_binary_import","VK_NVX_image_view_handle",VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME,VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME}){bool found=false;for(auto&e:es)if(!strcmp(e.extensionName,s))found=true;printf("EXTENSION name=%s advertised=%d enabled=%d\n",s,found,found);require(found,s,3);enable.push_back(s);}
  I(vkGetPhysicalDeviceQueueFamilyProperties)(physical,&n,nullptr);std::vector<VkQueueFamilyProperties> qs(n);I(vkGetPhysicalDeviceQueueFamilyProperties)(physical,&n,qs.data());bool found=false;for(uint32_t i=0;i<n;i++)if(qs[i].queueCount&&(qs[i].queueFlags&VK_QUEUE_COMPUTE_BIT)){family=i;found=true;break;}require(found,"compute queue",3);
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
   raw_read=nr->buffer(width*height*8ull,VK_BUFFER_USAGE_TRANSFER_DST_BIT);
   out_read=nr->buffer(width*height*16ull,VK_BUFFER_USAGE_TRANSFER_DST_BIT);
   source_read=nr->buffer(width*height*16ull,VK_BUFFER_USAGE_TRANSFER_DST_BIT);
   encode_cb=nr->buffer(sizeof(DlssNrConstants),VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
   resolve_cb=nr->buffer(sizeof(DlssNrConstants),VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT);
  }
  begin();nr->clear(raw,NAN);nr->clear(proxy,0);nr->clear(keep,0);nr->clear(dummy,0);submit();
  puts("C2_PRIVATE_READY production_entry=CSMain descriptor_sets_max=8 readback_bytes=82944000 groups=240,135,1 private_raw97_allocated=1 local_readbacks_preallocated=1 GPU_complete=1");
 }

 void shutdown(){
  require(!in_flight&&images.empty()&&nr&&nr->safe,"normal model shutdown requires GPU quiescence and release/audit");
  get<void(*)(void*)>(fw,"dlssnr_vk_release")(feature);feature=nullptr;puts("TEARDOWN feature_release completed");
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
 if(argc!=4){fputs("Usage: wine_game_nr_helper.exe single|skipEvaluate RUN_DIR_WINDOWS SESSION\n",stderr);return 2;}
 GameNrHelper c;c.dir=argv[2];c.session=argv[3];c.mode=argv[1];
 if(!freopen((c.dir+"\\wine-helper.log").c_str(),"w",stdout)||!freopen((c.dir+"\\wine-helper-stderr.log").c_str(),"w",stderr))return 4;setvbuf(stdout,nullptr,_IONBF,0);
 try{
  require((c.mode=="single"||c.mode=="skipEvaluate")&&safe_token(c.session),"isolated C2 modes/safe unique session only");c.negative=c.mode=="skipEvaluate";
  c.config();c.environment();atomic_text(c.dir+"\\pid-request.json",Json{{"session",c.session},{"exe",module(GetModuleHandleA(nullptr))},{"win_pid",GetCurrentProcessId()}}.dump()+"\n");
  auto pt=read_bounded(c.dir,"native.pid",c.control_timeout_ms);char*end=nullptr;auto pid=strtoul(pt.c_str(),&end,10);while(end&&(*end=='\r'||*end=='\n'||*end==' '))++end;require(end&&!*end&&pid>1&&pid<=0x7fffffff,"native-supplied verified actual exporter Linux PID",3);c.linux_pid=uint32_t(pid);
  printf("ACTUAL_LINUX_PID pid=%u authority=native_ancestor_verified session=%s\n",c.linux_pid,c.session.c_str());
  c.cold_start=GetTickCount64();c.init_nr_device();c.phase_tick("device_ready");c.init_model();c.phase_tick("model_private_ready");
  c.baseline();c.exports();c.frame();c.retire_shared();c.shutdown();
  auto result=c.envelope(1,"result","PASS");result["frames"]=c.frames_done;result["actual_linux_pid"]=c.linux_pid;
  result["model_init"]=c.model_initialized;result["evaluate_calls"]=c.evaluates;result["resolve_calls"]=c.resolves;result["model_valid"]=!c.negative;result["output_valid"]=!c.negative;
  result["normal_release"]=true;result["pixels_IPC"]=false;result["external_GPU_sync"]="NOT_RUN";atomic_text(c.dir+"\\wine-result.json",result.dump()+"\n");
  printf("PASS C2_HELPER mode=%s frames=%u evaluate_calls=%u resolve_calls=%u NORMAL_EXIT=0\n",c.mode.c_str(),c.frames_done,c.evaluates,c.resolves);return 0;
 }catch(const std::exception&e){
  auto stop=dynamic_cast<const Stop*>(&e);const int code=stop?stop->code:2;
  printf("%s reason=%s exit=%d frames=%u evaluates=%u resolves=%u in_flight=%u normal_release=0 cleanup=PRIVATE_PROCESS_EXIT_ONLY\n",code==3?"BLOCKED":"FAIL",e.what(),code,c.frames_done,c.evaluates,c.resolves,unsigned(c.in_flight));
  try{atomic_text(c.dir+"\\wine-result.json",Json{{"status",code==3?"BLOCKED":"FAIL"},{"reason",e.what()},{"exit_code",code},{"session",c.session},{"mode",c.mode},{"source_game_frame",c.source_game_frame},{"frames",c.frames_done},{"model_init",c.model_initialized},{"evaluate_calls",c.evaluates},{"resolve_calls",c.resolves},{"output_valid",false},{"model_valid",false},{"normal_release",false},{"cleanup","PRIVATE_PROCESS_EXIT_NOT_NORMAL_FD_RECLAIM"}}.dump()+"\n");}catch(...){}
  // No destructor frees resources that may still be referenced by either GPU/peer.
  c.nr.release();return code;
 }
}
