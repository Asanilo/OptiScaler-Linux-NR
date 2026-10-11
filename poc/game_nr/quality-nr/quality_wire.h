#pragma once
// SPDX-License-Identifier: GPL-3.0-or-later
// Portable, strict protocol5 parsing/profile check; no Wine/GPU/model calls.
#include <json.hpp>
#include <map>
#include <set>
#include <string>
#include <cmath>
#include <stdexcept>
namespace quality_wire {
using Json=nlohmann::json;
inline Json parse(const std::string&s){
 if(s.size()>65536)throw std::invalid_argument("control JSON exceeds64KiB");
 std::map<int,std::set<std::string>> keys;
 auto cb=[&](int depth,Json::parse_event_t e,Json&v){
  if(e==Json::parse_event_t::object_start)keys[depth+1].clear();
  if(e==Json::parse_event_t::key&&!keys[depth].insert(v.get<std::string>()).second)throw std::invalid_argument("duplicate JSON key at any object depth");
  if(e==Json::parse_event_t::object_end)keys.erase(depth+1);
  return true;
 };
 Json j=Json::parse(s,cb);if(!j.is_object())throw std::invalid_argument("control must be object");return j;
}
inline Json profile(){return parse(R"({"color_width":1920,"color_height":1080,"guide_width":1280,"guide_height":720,"model_width":1920,"model_height":1080,"output_width":1920,"output_height":1080,"working_scale":1.0,"color_domain":"scene_hdr","passthrough":false,"white_point":1.0,"exposure_source":"manual_unknown_game_exposure","exposure_valid":false,"stored_mv_units":"render_pixels_previous_minus_current","mv_values_jittered":false,"guide_sampling":"raw_jittered_raster_same_texel","phase_correction":"none","nr_phase_status":"UNVERIFIED","k_output_status":"NOT_IDENTIFIED","nr_mvec_scale_x":1.0,"nr_mvec_scale_y":1.0,"nr_mvec_scale_status":"production_candidate_UNVERIFIED"})");}
inline bool mode_count_matches(const std::string&m,uint64_t n,const std::string&c){
 return (m=="noop"&&n==1&&c=="noop")||(m=="single"&&n==1&&c=="single")||(m=="skipEvaluate"&&(n==1||n==4)&&c=="skipEvaluate")||(m=="resident"&&n==4&&c=="smoke");
}
inline bool matches(const Json&j){
 const auto expected=profile();if(!j.is_object()||j.size()!=expected.size())return false;
 for(auto it=expected.begin();it!=expected.end();++it){
  if(!j.contains(it.key()))return false;
  const auto&v=j.at(it.key());const auto&e=it.value();
  if(e.is_boolean()){if(!v.is_boolean()||v!=e)return false;}
  else if(e.is_string()){if(!v.is_string()||v!=e)return false;}
  else if(e.is_number_unsigned()){if(!v.is_number_unsigned()||v!=e)return false;}
  else if(!v.is_number()||!std::isfinite(v.get<double>())||v!=e)return false;
 }
 return true;
}
}
