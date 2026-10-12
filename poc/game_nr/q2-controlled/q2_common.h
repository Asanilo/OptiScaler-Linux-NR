#pragma once
// SPDX-License-Identifier: GPL-3.0-or-later
// Standalone controlled fixture utilities, no Q1 wire/IPC or Wine private API.
#include "/home/arinp/Code/nrbridge-design/game-poc-20261011/opti-worktree/poc/game_nr/c2-baseline/fixture_util.h"
#include "nvsdk_ngx_vk.h"
#include "/home/arinp/Code/dlss5_linux/OptiScaler-Linux-NR/build_out/nr-runtime-poc-20261010/poc/nr_runtime/parameter_abi.h"
#include <json.hpp>
#include <set>
#include <memory>
#include <limits>
using Json=nlohmann::json;
static void need(bool v,const char*msg){require(v,msg);}
static void check(VkResult r,const char*msg){api(r,msg);}
template<class T>static T get(HMODULE m,const char*n){auto p=reinterpret_cast<T>(GetProcAddress(m,n));require(p,n,3);return p;}
template<class T>static T proc(PFN_vkGetInstanceProcAddr g,VkInstance i,const char*n){auto p=reinterpret_cast<T>(g(i,n));require(p,n,3);return p;}
static std::wstring wide(const std::string&s){return std::wstring(s.begin(),s.end());}
static std::string winpath(const std::string&s){std::string p="Z:"+s;std::replace(p.begin(),p.end(),'/','\\');return p;}
static double c4_cpu_now_ms(){static const double f=[](){LARGE_INTEGER v{};require(QueryPerformanceFrequency(&v)&&v.QuadPart>0,"QPC frequency");return double(v.QuadPart);}();LARGE_INTEGER v{};require(QueryPerformanceCounter(&v),"QPC counter");return double(v.QuadPart)*1000.0/f;}
static std::string file_read(const std::string&path,size_t expected,size_t limit){
 FILE*f=fopen(path.c_str(),"rb");require(f,("read "+path).c_str(),3);
 if(_fseeki64(f,0,SEEK_END)!=0){fclose(f);throw Stop(2,"file seek");}const auto n=_ftelli64(f);
 if(n<0||uint64_t(n)>limit||(expected&&uint64_t(n)!=expected)||_fseeki64(f,0,SEEK_SET)!=0){fclose(f);throw Stop(2,"file byte size mismatch");}
 std::string s(size_t(n),'\0');const bool ok=fread(s.data(),1,s.size(),f)==s.size()&&!ferror(f);const bool closed=fclose(f)==0;require(ok&&closed,"read complete/close");return s;
}
static void dump_bytes(const std::string&path,const void*data,size_t bytes){FILE*f=fopen(path.c_str(),"wb");require(f,"dump fopen");const bool wrote=fwrite(data,1,bytes,f)==bytes;const bool closed=fclose(f)==0;require(wrote&&closed,"dump write/close");}
static Json strict_json(const std::string&s){std::vector<std::set<std::string>> stack;auto cb=[&](int,Json::parse_event_t e,Json&v){if(e==Json::parse_event_t::object_start)stack.emplace_back();else if(e==Json::parse_event_t::key){require(!stack.empty()&&stack.back().insert(v.get<std::string>()).second,"duplicate JSON key");}else if(e==Json::parse_event_t::object_end){require(!stack.empty(),"JSON stack");stack.pop_back();}return true;};auto j=Json::parse(s,cb);require(j.is_object(),"JSON object");return j;}
static uint64_t uint_value(const Json&j,const char*k){require(j.contains(k)&&j[k].is_number_unsigned(),k);return j[k].get<uint64_t>();}
static std::string string_value(const Json&j,const char*k){require(j.contains(k)&&j[k].is_string(),k);return j[k].get<std::string>();}
static float finite_value(const Json&j,const char*k){require(j.contains(k)&&j[k].is_number(),k);float v=j[k].get<float>();require(std::isfinite(v),k);return v;}
static bool bool_value(const Json&j,const char*k){require(j.contains(k)&&j[k].is_boolean(),k);return j[k].get<bool>();}
static bool hex64(const std::string&s){return s.size()==64&&std::all_of(s.begin(),s.end(),[](char c){return(c>='0'&&c<='9')||(c>='a'&&c<='f');});}
static bool safe_name(const std::string&s){return !s.empty()&&s.size()<=63&&std::all_of(s.begin(),s.end(),[](char c){return(c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-'||c=='_';});}
static const std::string model_path=winpath("/home/arinp/Code/dlss5_linux/dll/runtime/nvngx_dlssnr.dll");
static const std::string forwarder_path=winpath("/home/arinp/Code/dlss5_linux/OptiScaler-Linux-NR/build_out/nr-runtime-poc-20261010/build_out/poc-runtime-20261010/nvngx.dll_dlssnr.dll");
static const std::string core_path=winpath("/usr/lib64/nvidia/wine/_nvngx.dll");
static const std::string nvapi_path=winpath("/home/arinp/.local/share/Steam/compatibilitytools.d/GE-Proton11-7-x86_64/files/lib/wine/nvapi/x86_64-windows/nvapi64.dll");
static const std::string dxgi_path=winpath("/home/arinp/.local/share/Steam/compatibilitytools.d/GE-Proton11-7-x86_64/files/lib/wine/dxvk/x86_64-windows/dxgi.dll");
