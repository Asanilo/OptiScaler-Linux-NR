#pragma once
// SPDX-License-Identifier: GPL-3.0-or-later
// B's sole q2-known-translation-v1 schema. Portable validator; no model/oracle asset reads.
#include <json.hpp>
#include <array>
#include <algorithm>
#include <vector>
#include <cstdint>
#include <cmath>
#include <set>
#include <string>
#include <stdexcept>
namespace q2 {
using J=nlohmann::json;
inline void need(bool b,const char*m){if(!b)throw std::runtime_error(m);}
inline uint64_t u(const J&j,const char*k){need(j.contains(k)&&j[k].is_number_unsigned(),k);return j[k].get<uint64_t>();}
inline std::string s(const J&j,const char*k){need(j.contains(k)&&j[k].is_string(),k);return j[k].get<std::string>();}
inline J parse(const std::string&s){std::vector<std::set<std::string>> stack;auto cb=[&](int,J::parse_event_t e,J&v){if(e==J::parse_event_t::object_start)stack.emplace_back();else if(e==J::parse_event_t::key)need(!stack.empty()&&stack.back().insert(v.get<std::string>()).second,"duplicate key");else if(e==J::parse_event_t::object_end)stack.pop_back();return true;};auto j=J::parse(s,cb);need(j.is_object(),"case object");return j;}
inline bool lowerhex(const std::string&s,size_t n){return s.size()==n&&std::all_of(s.begin(),s.end(),[](char c){return (c>='0'&&c<='9')||(c>='a'&&c<='f');});}
inline const std::array<const char*,5> branch_ids={"primary","repeat_primary","zero_mv","reverse_mv","reset_second"};
inline const std::string root="/home/arinp/Code/nrbridge-design/game-poc-20261011/bbport/runs/q2-controlled-assets-v1/assets/";
inline J model_parameters(){return J{{"working_scale",1},{"manual_white_point",1.0},{"passthrough",false},{"scene_linear",true},{"transfer_strength",1.0},{"colour_strength",1.0},{"transfer",1},{"max_ratio",2.0},{"frame_ms",1000.0/60.0},{"exposure",1.0},{"pre_exposure",1.0},{"sr",false},{"fg",false}};}
inline void asset(const J&a,unsigned w,unsigned h,unsigned format,unsigned channels){
 need(a.is_object(),"asset object");const auto path=s(a,"path");need(path.rfind(root,0)==0&&path.find("..") == std::string::npos&&path.find('\\')==std::string::npos,"asset root/path");
 const auto basename=path.substr(root.size());need(!basename.empty()&&basename.find('/')==std::string::npos&&std::all_of(basename.begin(),basename.end(),[](char c){return(c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='.'||c=='-'||c=='_';}),"asset basename");
 need(s(a,"windows_path")=="Z:"+path,"asset Windows mapping");need(lowerhex(s(a,"sha256"),64),"asset SHA256 syntax");need(s(a,"dtype")=="<f4","asset dtype");need(u(a,"format")==format&&u(a,"channels")==channels,"asset format/channels");need(a.at("extent")==J::array({w,h}),"asset extent");need(u(a,"bytes")==uint64_t(w)*h*channels*4,"asset bytes");
}
inline void validate(const J&j){
 need(s(j,"schema")=="q2-known-translation-v1","schema");const auto id=s(j,"case_id");
 const std::set<std::string> ids={"ref1080-s1","ref1080-s15","g720-raw-s1","g720-raw-s15","g720-corrected-s1","g720-corrected-s15"};need(ids.count(id),"case_id");
 unsigned w=id.rfind("ref1080",0)==0?1920:1280,h=w==1920?1080:720;
 const std::string phase=id.find("raw")!=std::string::npos?"raw":"corrected";double scale=id.size()>=3&&id.substr(id.size()-3)=="s15"?1.5:1.0;
 need(j.at("model_extent")==J::array({1920,1080})&&j.at("guide_extent")==J::array({w,h}),"model/guide extent");need(s(j,"phase")==phase,"case phase");need(j.at("nr_mvec_scale")==J::array({scale,scale}),"case MV scale");
 auto p=j.at("model_parameters");need(p==model_parameters(),"exact model_parameters");for(const char*k:{"passthrough","scene_linear","sr","fg"})need(p[k].is_boolean(),"model bool type");for(const char*k:{"working_scale","transfer"})u(p,k);
 need(j.at("budgets_ms")==J{{"gpu_fence",10000},{"cold",120000},{"outer",300000}},"fixed budgets");for(const char*k:{"gpu_fence","cold","outer"})u(j.at("budgets_ms"),k);
 need(s(j,"plan_sha256")=="12fcd962a4fdb09543994cbc0a0b21bb8ea3e1b85f8b4f7c6daeb44e3f5abb66","plan SHA");
 need(j.at("translation_output_pixels")==J::array({12,-6})&&j.at("known_color_output_phase")==J::array({0,0}),"physical scene metadata");need(j.at("jitter_render_reference_pixels")==J::array({J::array({.25,-1.0/3.0}),J::array({-.25,2.0/9.0})}),"reference jitter metadata");
 need(s(j,"asset_input_scope")=="CPU_generated_canonical_files_then_same_process_GPU_upload_not_shared_transport","asset scope");
 const auto&bs=j.at("branches");need(bs.is_array()&&bs.size()==5,"exact five branches");for(unsigned b=0;b<5;b++){
  const auto&br=bs[b];need(s(br,"branch_id")==branch_ids[b],"ordered branch_id");need(br.at("fresh_feature").is_boolean()&&br.at("fresh_feature").get<bool>(),"fresh feature");const auto&fs=br.at("frames");need(fs.is_array()&&fs.size()==2,"exact two frames");
  for(unsigned f=0;f<2;f++){const auto&v=fs[f];need(u(v,"frame")==f&&u(v,"reset")==((f==0||b==4)?1:0),"frame/reset");asset(v.at("source"),1920,1080,109,4);asset(v.at("depth"),w,h,100,1);asset(v.at("mv"),w,h,103,2);}
 }
 // Validate control identity relationships; only source/depth/mv may be opened.
 for(unsigned b=1;b<5;b++)for(unsigned f=0;f<2;f++){
  need(bs[b]["frames"][f]["source"]==bs[0]["frames"][f]["source"]&&bs[b]["frames"][f]["depth"]==bs[0]["frames"][f]["depth"],"fixed Color/Depth across branches");
  if(b==1||b==4)need(bs[b]["frames"][f]["mv"]==bs[0]["frames"][f]["mv"],"repeat/reset primary MV");
 }
 need(j.contains("oracle")&&j["oracle"].is_object(),"oracle metadata exists, never loaded");
}
}
