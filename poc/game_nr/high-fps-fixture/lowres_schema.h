#pragma once
// SPDX-License-Identifier: GPL-3.0-or-later
#include <json.hpp>
#include <set>
#include <cmath>
#include <stdexcept>
#include <string>
#include <vector>
namespace lowres {
using J=nlohmann::json;
inline void need(bool v,const char*m){if(!v)throw std::invalid_argument(m);}
inline J parse(const std::string&s){need(s.size()<=65536,"case64KiB");std::vector<std::set<std::string>>ks;auto cb=[&](int,J::parse_event_t e,J&v){if(e==J::parse_event_t::object_start)ks.emplace_back();else if(e==J::parse_event_t::key)need(!ks.empty()&&ks.back().insert(v.get<std::string>()).second,"duplicate key");else if(e==J::parse_event_t::object_end)ks.pop_back();else if(e==J::parse_event_t::value&&v.is_number_float())need(std::isfinite(v.get<double>()),"nonfinite");return true;};auto j=J::parse(s,cb);need(j.is_object(),"case object");return j;}
inline uint64_t u(const J&j,const char*k){need(j.contains(k)&&j.at(k).is_number_unsigned(),k);return j.at(k).get<uint64_t>();}
inline bool hex(const std::string&s){return s.size()==64&&s.find_first_not_of("0123456789abcdef")==std::string::npos;}
inline J application(){return{{"frame_ms","HOST_FIXTURE_DECLARATION_NOT_SENT_TO_NR_API"},{"exposure","HOST_FIXTURE_DECLARATION_NOT_SENT_TO_NR_API"},{"pre_exposure","HOST_FIXTURE_DECLARATION_NOT_SENT_TO_NR_API"},{"runtime_default_values","UNVERIFIED"}};}
inline J parameters(){return{{"style",0u},{"intensity",1.0},{"white_point",1.0},{"passthrough",false},{"mvec_scale",{1.0,1.0}},{"local_structure",1.0},{"local_tone",1.0},{"skin_structure",1.0},{"use_auto_mask",1u},{"preset",0u}};}
inline void validate(const J&j){
 need(j.size()==19&&j.at("schema")=="nr-highfps-lowres-cost-v1"&&j.at("case_id")=="matched-p1-current-runtime-ref1080-small720-skip720","canonical case identity");
 need(j.at("source_extent")==J::array({1920,1080})&&j.at("guide_extent")==J::array({1280,720})&&j.at("output_extent")==J::array({1920,1080}),"full source/keep/output; guide720");
 const J stages=J::array({J{{"name","reference1080"},{"model_extent",{1920,1080}},{"evaluate",true},{"feature_generation",1u}},J{{"name","lowres720"},{"model_extent",{1280,720}},{"evaluate",true},{"feature_generation",2u}},J{{"name","skip720"},{"model_extent",{1280,720}},{"evaluate",false},{"feature_generation",2u}}});need(j.at("stages")==stages,"reference FIRST then lowres then negative (no bypass)");
 need(u(j,"repetitions")==1&&u(j,"frames_per_stage")==4,"bounded12 frames/8 Eval no hidden repetitions");
 need(j.at("parameters")==parameters()&&u(j.at("parameters"),"style")==0&&u(j.at("parameters"),"preset")==0&&j.at("parameters").at("passthrough").is_boolean(),"fixed real parameters exact types");
 need(j.at("parameter_application")==application()&&j.at("model_reset_policy")=="RESET1_EVERY_REFRESH; independent edit history not implemented in this fixture","truthful application/modelReset");
 need(j.at("MVecScale_status")=="EXPERIMENTAL_UNVERIFIED"&&j.at("NR_phase_status")=="UNVERIFIED"&&j.at("k_output_status")=="NOT_IDENTIFIED","no semantic acceptance");
 need(j.at("budgets_ms")==J{{"cold",120000u},{"frame",30000u},{"fence",10000u},{"outer",300000u}},"fixed budgets");
 need(j.at("runtime_driver_required")==J::array({615,78,8})&&j.at("expected_device_uuid")=="8734fac9db30c279a340fe117ace6365","current runtime/device");
 need(hex(j.at("input_manifest_sha256").get<std::string>()),"inputmanifest SHA");
 for(const char*k:{"source_extent","guide_extent","output_extent","runtime_driver_required"})for(const auto&v:j.at(k))need(v.is_number_unsigned(),"extent/driver unsigned values");
 for(const char*k:{"cold","frame","fence","outer"})(void)u(j.at("budgets_ms"),k);
 for(const auto&st:j.at("stages")){(void)u(st,"feature_generation");need(st.at("evaluate").is_boolean(),"stage bool");for(const auto&v:st.at("model_extent"))need(v.is_number_unsigned(),"model extent unsigned");}
 const auto&fs=j.at("frames");need(fs.is_array()&&fs.size()==4,"four exact captured inputs");uint64_t previous=0;
 const std::string root="/home/arinp/Code/nrbridge-design/game-poc-20261011/bbport/runs/p0-game-p1-quality-wait-diag4-20261012/frames/bucket-000000/";
 for(unsigned f=0;f<4;f++){const auto&v=fs[f];need(v.size()==6&&u(v,"ordinal")==f&&v.at("source_reset").is_boolean(),"input ordinal/types");const auto source=u(v,"source_game_frame");need(source==18645u+f&&(!f||source==previous+1),"same-session consecutive actual source");previous=source;
  const auto& jit=v.at("jitter");need(jit.is_object()&&jit.size()==8,"actual eight jitter fields");for(const char*k:{"viewport_jitter_x","viewport_jitter_y","previous_viewport_jitter_x","previous_viewport_jitter_y","sr_jitter_x","sr_jitter_y","previous_sr_jitter_x","previous_sr_jitter_y"})need(jit.contains(k)&&jit.at(k).is_number()&&std::isfinite(jit.at(k).get<double>()),"actual finite jitter no setter");
  need(v.at("assets").is_object()&&v.at("assets").size()==3,"three source asset roles");
  for(const char*role:{"source","depth","mv"}){const auto&a=v.at("assets").at(role);bool full=std::string(role)=="source",motion=std::string(role)=="mv";unsigned channels=full?4:motion?2:1;unsigned width=full?1920:1280,height=full?1080:720;std::string name=full?"native-source-f"+std::to_string(f)+".rgba32f":motion?"native-mv-f"+std::to_string(f)+".rg32f":"native-depth-f"+std::to_string(f)+".r32f";
   need(a.size()==8&&u(a,"bytes")==uint64_t(width)*height*channels*4&&u(a,"format")==unsigned(full?109:motion?103:100)&&u(a,"channels")==channels&&a.at("extent")==J::array({width,height})&&a.at("dtype")=="float32_le","asset actual CI/bytes/format");
   const std::string path=root+name;std::string win="Z:"+path;for(char&c:win)if(c=='/')c='\\';need(a.at("path")==path&&a.at("windows_path")==win&&hex(a.at("sha256").get<std::string>()),"exact read-only captured asset path/hash");
  }
  const auto&control=v.at("original_control");need(control.size()==2&&control.at("path")==root+"input-p1-quality-wait-diag4-20261012-g0-f"+std::to_string(f)+".json"&&hex(control.at("sha256").get<std::string>()),"same original control pinned");
 }
}
}
