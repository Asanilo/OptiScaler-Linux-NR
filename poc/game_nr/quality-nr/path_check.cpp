// SPDX-License-Identifier: GPL-3.0-or-later
// Native CPU-only resolver/file-placement check. Does not link Wine/Vulkan/NR.
#include "frame_paths.h"
#include <json.hpp>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>
#include <set>
#include <array>
#include <algorithm>
using Json=nlohmann::json;namespace fs=std::filesystem;
static void must(bool b,const char*m){if(!b)throw std::runtime_error(m);}
static size_t entries(const fs::path&p){return size_t(std::distance(fs::directory_iterator(p),fs::directory_iterator{}));}
int main(int argc,char**argv){
 try{
  must(argc==2,"provide a fresh private output directory");const fs::path root=fs::absolute(argv[1]);must(!fs::exists(root),"refuse an existing path");fs::create_directories(root);
  Json result{{"status","PASS_NO_GPU_ROUTING"},{"scope","native CPU resolver + synthetic files only; not Wine API/runtime atomic validation"},{"GPU_execution",false},{"Wine_execution",false},{"model_calls",0},{"transport_layout",quality_paths::layout},{"cases",Json::array()}};
  must(quality_paths::layout_matches(Json{{"transport_layout",quality_paths::layout}}),"exact layout accept");
  for(const Json&j:{Json::object(),Json{{"transport_layout","flat"}},Json{{"transport_layout",4}},Json{{"transport_layout",nullptr}},Json::array()})must(!quality_paths::layout_matches(j),"missing/type/mismatch layout must reject");
  result["layout_mismatch_rejection"]="PASS_SHARED_PREDICATE";
  const std::vector<uint64_t> boundaries{0,63,64,127,128};
  for(uint64_t n:{1ull,4ull,120ull,1800ull}){
   const auto run=root/("count"+std::to_string(n));fs::create_directory(run);fs::create_directory(run/"frames");
   for(uint64_t b=0;b<quality_paths::bucket_count(n);++b)fs::create_directory(quality_paths::frame_dir(run.string(),b*64,n,'/'));
   const auto realroot=fs::canonical(run);auto inspect=boundaries;inspect.push_back(n-1);std::set<uint64_t> unique;
   // Populate an entire first leaf for the declared 5*64 bound (4/120 respect count).
   for(uint64_t f=0;f<std::min(n,uint64_t(64));++f)unique.insert(f);
   for(auto f:inspect)if(f<n)unique.insert(f);
   Json rows=Json::array();
   for(auto f:unique){
    const auto dir=fs::path(quality_paths::frame_dir(run.string(),f,n,'/'));must(fs::canonical(dir).parent_path().parent_path()==realroot,"native private resolved leaf escape");
    for(const std::string kind:{"input","complete","consumed","native-frame","native-merge-timing"}){
     const auto name=kind+"-probe-g0-f"+std::to_string(f)+".json";
     const auto file=fs::path(quality_paths::frame_path(run.string(),f,n,name,'/'));const fs::path tmp=file.string()+".tmp";
     must(file.parent_path()==tmp.parent_path(),"atomic tmp outside leaf");
     Json envelope{{"protocol_version",5},{"transport_layout",quality_paths::layout},{"session","probe"},{"generation",0},{"frame",f},{"resource_set","probe-g0"},{"scope","synthetic routing only"}};
     {std::ofstream out(tmp);out<<envelope.dump()<<'\n';out.close();must(bool(out),"synthetic file write failed");}
     fs::rename(tmp,file);std::ifstream in(file);Json actual;in>>actual;must(actual==envelope&&!fs::exists(tmp),"synthetic CPU rename/readback changed tuple");
     if(std::find(inspect.begin(),inspect.end(),f)!=inspect.end())rows.push_back(Json{{"frame",f},{"kind",kind},{"basename",name},{"relative",fs::relative(file,run).string()},{"windows_relative",quality_paths::frame_relative(f,n,'\\')+"\\"+name},{"temp_same_leaf",true},{"tuple_match",true}});
    }
   }
   // The stale flat candidate exists but is never searched or accepted.
   const uint64_t last=n-1;const auto basename="input-probe-g0-f"+std::to_string(last)+".json";
   {std::ofstream out(run/basename);out<<"stale flat\n";}
   must(quality_paths::frame_path(run.string(),last,n,basename,'/')!=(run/basename).string(),"flat fallback" );
   const auto sentinel=run/"native.abort";{std::ofstream out(sentinel);out<<"CPU-only sentinel\n";}
   must(sentinel.parent_path()==run,"abort leaves root");
   const auto terminal=run/"native-released-probe-g0.json";{std::ofstream out(terminal);out<<Json{{"frame",n},{"transport_layout",quality_paths::layout}}.dump();}
   bool rejected=false;try{(void)quality_paths::frame_dir(run.string(),n,n,'/');}catch(const std::invalid_argument&){rejected=true;}must(rejected,"terminal frame_count incorrectly routed as frame");
   for(const std::string&bad:std::vector<std::string>{"../escape","sub/file","sub\\file","Z:escape","..",std::string("bad\0file",8)}){rejected=false;try{(void)quality_paths::frame_path(run.string(),0,n,bad,'/');}catch(const std::invalid_argument&){rejected=true;}must(rejected,"unsafe basename accepted");}
   Json counts=Json::array();for(uint64_t b=0;b<quality_paths::bucket_count(n);++b){const auto dir=quality_paths::frame_dir(run.string(),b*64,n,'/');const auto c=entries(dir);must(c<=320,"hot record class count exceeded320");if(c)counts.push_back(Json{{"bucket",b},{"actual_entries",c}});}
   result["cases"].push_back(Json{{"frame_count",n},{"bucket_count",quality_paths::bucket_count(n)},{"root_entries",entries(run)},{"leaf_counts",counts},{"boundary_checks",rows},{"no_flat_fallback",true},{"terminal_frame_in_root",true},{"no_delete_or_archive",true}});
  }
  for(uint64_t n:std::array<uint64_t,4>{0,128,18001,UINT64_MAX}){bool rejected=false;try{(void)quality_paths::bucket_count(n);}catch(const std::invalid_argument&){rejected=true;}must(rejected,"invalid/unbounded frame count accepted");}
  std::cout<<result.dump(2)<<'\n';return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 2;}
}
