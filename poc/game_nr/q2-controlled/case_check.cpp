// SPDX-License-Identifier: GPL-3.0-or-later
#include "q2_case.h"
#include <fstream>
#include <iostream>
using q2::J;
int main(int argc,char**argv){
 try{q2::need(argc==7,"exact six CPU-only case paths");unsigned rejected=0;for(int a=1;a<argc;a++){
 std::ifstream f(argv[a]);q2::need(bool(f),"read case JSON");std::string s((std::istreambuf_iterator<char>(f)),{});auto j=q2::parse(s);q2::validate(j);
 auto reject=[&](J v){bool threw=false;try{q2::validate(v);}catch(...){threw=true;}q2::need(threw,"invalid schema accepted");++rejected;};
 auto v=j;v["branches"].erase(v["branches"].begin());reject(v);v=j;v["branches"][1]["branch_id"]="primary";reject(v);v=j;v["branches"][4]["frames"][1]["reset"]=0;reject(v);v=j;v["branches"][0]["frames"][0]["source"]["bytes"]=4;reject(v);v=j;v["nr_mvec_scale"]=J::array({2,2});reject(v);v=j;v["model_parameters"]["passthrough"]=0;reject(v);v=j;v["budgets_ms"]["outer"]=600000;reject(v);v=j;v["branches"][0]["frames"][0]["mv"]["extent"]=J::array({1,1});reject(v);
 bool dup=false;try{q2::parse("{\"schema\":1,\"schema\":2}");}catch(...){dup=true;}q2::need(dup,"duplicate accepted");++rejected;
 }
 std::cout<<J{{"status","PASS_CPU_SCHEMA_ONLY"},{"cases",6},{"invalid_variants_rejected",rejected},{"GPU_execution",false},{"Wine_execution",false},{"model_calls",0}}.dump()<<"\n";return 0;
 }catch(const std::exception&e){std::cerr<<e.what()<<"\n";return 2;}
}
