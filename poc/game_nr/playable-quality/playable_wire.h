#pragma once
// SPDX-License-Identifier: GPL-3.0-or-later
// Implements Root's single frozen p0-wire-spec.json; generated canonical literal is build-pinned.
#include <json.hpp>
#include <set>
#include <string>
#include <stdexcept>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include "out/p0_spec.h"
namespace p0_wire {
using J=nlohmann::json;
inline void need(bool b,const char*m){if(!b)throw std::invalid_argument(m);}
inline uint64_t u(const J&j,const char*k){need(j.contains(k)&&j.at(k).is_number_unsigned(),k);return j.at(k).get<uint64_t>();}
inline std::string s(const J&j,const char*k){need(j.contains(k)&&j.at(k).is_string(),k);return j.at(k).get<std::string>();}
inline bool b(const J&j,const char*k){need(j.contains(k)&&j.at(k).is_boolean(),k);return j.at(k).get<bool>();}
inline J parse(const std::string&text){std::vector<std::set<std::string>>keys;auto cb=[&](int,J::parse_event_t e,J&v){if(e==J::parse_event_t::object_start)keys.emplace_back();else if(e==J::parse_event_t::key)need(!keys.empty()&&keys.back().insert(v.get<std::string>()).second,"duplicate JSON key");else if(e==J::parse_event_t::object_end)keys.pop_back();if(e==J::parse_event_t::value&&v.is_number_float())need(std::isfinite(v.get<double>()),"nonfinite JSON number");return true;};auto j=J::parse(text,cb);need(j.is_object(),"JSON control object");return j;}
inline const J&spec(){static const J j=parse(p0_spec_json);return j;}
inline const std::string profile_id=spec().at("profile_id").get<std::string>();
inline constexpr const char*layout="frame-buckets-64-v1";
inline bool exact_profile(const J&j){const auto&e=spec().at("quality_profile");if(!j.is_object()||j.size()!=e.size())return false;for(auto it=e.begin();it!=e.end();++it){if(!j.contains(it.key()))return false;const auto&v=j.at(it.key());if(it.value().is_boolean()){if(!v.is_boolean()||v!=it.value())return false;}else if(it.value().is_string()){if(!v.is_string()||v!=it.value())return false;}else if(it.value().is_number_unsigned()){if(!v.is_number_unsigned()||v!=it.value())return false;}else if(!v.is_number()||!std::isfinite(v.get<double>())||v!=it.value())return false;}return true;}
inline bool allowed_count(uint64_t n){return n==4||n==120||n==1800||n==18000;}
inline uint64_t bucket_count(uint64_t n){need(allowed_count(n),"declared finite limit");return(n+63)/64;}
inline std::string frame_dir(const std::string&root,uint64_t f,uint64_t n,char sep){need(allowed_count(n)&&f<n&&(sep=='/'||sep=='\\'),"current frame within limit");char name[32]{};snprintf(name,sizeof name,"bucket-%06llu",(unsigned long long)(f/64));return root+sep+"frames"+sep+name;}
inline std::string frame_path(const std::string&root,uint64_t f,uint64_t n,const std::string&name,char sep){need(!name.empty()&&name!="."&&name!=".."&&std::all_of(name.begin(),name.end(),[](char c){return(c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='.'||c=='-'||c=='_';}),"exact safe basename");return frame_dir(root,f,n,sep)+sep+name;}
inline void config(const J&j,const std::string&session,const std::string&mode){
 need(u(j,"protocol_version")==6&&s(j,"transport_layout")==layout&&s(j,"profile_id")==profile_id&&s(j,"session")==session&&u(j,"generation")==0&&s(j,"mode")==mode,"P0 config identity/version/layout");need(exact_profile(j.at("quality_profile")),"strict experimental Quality profile");
 const auto count=u(j,"frame_count");need(allowed_count(count)&&u(j,"max_frames")==count,"finite max_frames before narrowing");auto execution=s(j,"execution_mode");need(execution=="hot"||execution=="diagnostic","execution_mode");need((execution=="diagnostic"&&count==4&&(mode=="resident"||mode=="skipEvaluate"))||(execution=="hot"&&mode=="resident"),"mode/execution/count gate");
 auto wall=u(j,"service_wall_seconds");need(wall>=1&&wall<=900,"bounded service wall");need(u(j,"params_version")==1&&u(j,"initial_history_version")==1,"initial epochs");need(j.at("parameters")==spec().at("config").at("parameters"),"fixed real parameters");need(u(j.at("parameters"),"style")==0,"style unsigned0");need(j.at("parameter_application")==spec().at("config").at("parameter_application"),"truthful exact four-key application");
 need(u(j,"native_soft_deadline_ms")==uint64_t(execution=="hot"?500:30000),"mode-specific native acceptance only");
 need(s(j,"audit_kind")== (execution=="hot"?"off":"full")&&s(j,"validation_profile")== (execution=="hot"?"p0_hot_path":"p0_diagnostic"),"validation scope");
 need(u(j,"cold_init_timeout_ms")==120000&&u(j,"control_timeout_ms")==60000&&u(j,"gpu_timeout_ms")==10000&&u(j,"frame_completion_timeout_ms")==30000,"fixed hard budgets");
 need(u(j,"width")==1920&&u(j,"height")==1080&&u(j,"render_width")==1280&&u(j,"render_height")==720,"actual role extent");
}
inline std::string reset_reason(uint64_t frame,bool source_reset,bool resumed,bool gap){return frame==0?"first":source_reset?"sr_reset":resumed?"resume":gap?"source_gap":"continuous";}
struct ControlState {
 std::string session,execution;uint64_t limit=0,finalized=0,accepted=0,discarded=0,last_source=0,history=1,epoch=0;bool idle=false,stopping=false,resume_pending=false,active=false;J last_request,last_ack;
 enum Decision{Stale,Replay,Applied};
 void identity(const J&j)const{need(u(j,"protocol_version")==6&&s(j,"profile_id")==profile_id&&s(j,"execution_mode")==execution&&s(j,"session")==session&&u(j,"generation")==0&&s(j,"resource_set")==session+"-g0","control identity");need(s(j,"kind")=="control"&&s(j,"state")=="REQUESTED","control kind/state");}
 Decision apply(const J&j){
  identity(j);const auto e=u(j,"control_epoch");need(e>=1,"control epoch>=1");if(e<epoch)return Stale;if(e==epoch){need(j==last_request,"changed content at accepted epoch");return Replay;}
  need(epoch<UINT64_MAX&&e==epoch+1,"skipped/future control epoch");need(!active,"control cannot replace current consumption");
  need(u(j,"completed_count")==finalized&&u(j,"next_frame")==finalized&&finalized<=limit,"exact finalized count");need(u(j,"source_game_frame")==last_source&&b(j,"source_frame_valid")== (finalized!=0),"actual last finalized source/N0 validity");need(u(j,"params_version")==1,"fixed params_version");
  const auto action=s(j,"action"),reason=s(j,"reason");need(!discarded||action=="drain"||action=="stop","discarded session must drain/stop, not resume/reuse");need(std::set<std::string>{"enable_off","enable_on","pause","resume","load","unsupported","exit","budget"}.count(reason),"control reason");
  need(reason!="enable_off"||action=="stop","enable_off must stop not retained idle");
  need(!stopping,"no new action after terminal ready");const auto requested_history=u(j,"history_version");
  if(action=="resume"){need(idle&&history<UINT64_MAX&&requested_history==history+1&&finalized<limit,"resume only idle/supported and increment once");history=requested_history;idle=false;resume_pending=true;}
  else{need(requested_history==history,"current history required");if(action=="idle")idle=true;else if(action=="drain"||action=="stop")stopping=true;else need(false,"unknown control action");}
  epoch=e;last_request=j;last_ack=j;last_ack["kind"]="control_ack";last_ack["state"]=action=="idle"?"IDLE":action=="resume"?"READY":action=="drain"?"DRAIN_READY":"STOP_READY";last_ack["finalized_count"]=finalized;last_ack["accepted_count"]=accepted;last_ack["discarded_count"]=discarded;return Applied;
 }
 void start(){need(!active&&!idle&&!stopping&&!discarded&&finalized<limit,"one active input in READY only; discarded session cannot reuse");active=true;}
 void finalize(uint64_t source,bool accepted_NR,bool was_discarded){need(active&&source>last_source&&finalized<limit&&!(accepted_NR&&was_discarded),"actual current finalized source/ownership");active=false;++finalized;if(accepted_NR)++accepted;if(was_discarded)++discarded;last_source=source;resume_pending=false;}
};
}
