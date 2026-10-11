// SPDX-License-Identifier: GPL-3.0-or-later
// Tests only the shared portable wire predicates; no Wine/Vulkan/model invocation.
#include "quality_wire.h"
#include <iostream>
using Json=nlohmann::json;
static void need(bool v,const char*m){if(!v)throw std::runtime_error(m);}
int main(){try{
 auto p=quality_wire::profile();need(quality_wire::matches(p),"profile defaults");
 auto q=p;q["working_scale"]=1u;q=quality_wire::parse(q.dump());need(quality_wire::matches(q),"numeric1 int accepted and left unchanged");
 for(auto field:{"passthrough","exposure_valid","mv_values_jittered"}){q=p;q[field]=0u;need(!quality_wire::matches(q),"uint is not bool");}
 q=p;q["color_width"]=1920.0;need(!quality_wire::matches(q),"extent must uint");
 q=p;q["nr_mvec_scale_x"]=true;need(!quality_wire::matches(q),"numeric bool rejected");
 q=p;q["guide_width"]=1920u;need(!quality_wire::matches(q),"guide mismatch rejected");
 q=p;q["extra"]=1;need(!quality_wire::matches(q),"additional profile key rejected");q=p;q.erase("phase_correction");need(!quality_wire::matches(q),"missing profile key rejected");
 for(auto text:{R"({"x":1,"x":2})",R"({"q":{"x":1,"x":2}})",R"({"q":[{"x":1,"x":2}]})",R"({"x":NaN})",R"({"x":1e999})","[]"}){
  bool rejected=false;try{(void)quality_wire::parse(text);}catch(const std::exception&){rejected=true;}need(rejected,"duplicate/nonJSON/nonfinite object rejected");
 }
 need(quality_wire::parse(R"({"q":[{"x":1},{"x":2}]})").is_object(),"separate sibling objects legal");
 need(quality_wire::mode_count_matches("noop",1,"noop")&&quality_wire::mode_count_matches("single",1,"single")&&quality_wire::mode_count_matches("resident",4,"smoke")&&quality_wire::mode_count_matches("skipEvaluate",1,"skipEvaluate")&&quality_wire::mode_count_matches("skipEvaluate",4,"skipEvaluate"),"authorized mode counts");
 for(uint64_t n:{0ull,120ull,1800ull,4294967300ull})need(!quality_wire::mode_count_matches("resident",n,"smoke"),"unsupported/truncated count rejected");
 need(!quality_wire::mode_count_matches("noop",4,"noop")&&!quality_wire::mode_count_matches("single",4,"single"),"single/noop exactly1");
 std::cout<<Json{{"status","PASS_NO_GPU_WIRE"},{"scope","shared portable JSON/profile/mode predicates only; actual helper start NOT_RUN"},{"GPU_execution",false},{"Wine_execution",false},{"model_calls",0},{"duplicate_nested_key_rejection",true},{"numeric_representation_preserved",true},{"passthrough_bool_only",true},{"uint64_mode_count_before_narrowing",true}}.dump()<<'\n';return 0;
}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 2;}}
