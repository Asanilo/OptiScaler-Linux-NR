#pragma once
// SPDX-License-Identifier: MIT
#include "fixture_util.h"
#include <json.hpp>
#include <set>
#include <algorithm>
#include <cmath>
using Json=nlohmann::json;
static Json parse_control(const std::string& s){
 require(s.size()<=65536,"control JSON exceeds 64KiB");std::set<std::string> keys;
 auto callback=[&](int depth,Json::parse_event_t event,Json& parsed){
  if(event==Json::parse_event_t::key&&depth==1)require(keys.insert(parsed.get<std::string>()).second,"duplicate top-level JSON key");return true;};
 Json j=Json::parse(s,callback);require(j.is_object(),"control must be JSON object");return j;
}
static uint64_t uint_value(const Json&j,const char*key){require(j.contains(key)&&j[key].is_number_unsigned(),("unsigned JSON field required: "+std::string(key)).c_str());return j[key].get<uint64_t>();}
static std::string string_value(const Json&j,const char*key){require(j.contains(key)&&j[key].is_string(),("string JSON field required: "+std::string(key)).c_str());return j[key].get<std::string>();}
static bool bool_value(const Json&j,const char*key){require(j.contains(key)&&j[key].is_boolean(),("bool JSON field required: "+std::string(key)).c_str());return j[key].get<bool>();}
static float finite_value(const Json&j,const char*key){require(j.contains(key)&&j[key].is_number(),("finite number required: "+std::string(key)).c_str());float v=j[key].get<float>();require(std::isfinite(v),"nonfinite frame parameter");return v;}
static bool safe_token(const std::string&s){return !s.empty()&&s.size()<=63&&std::all_of(s.begin(),s.end(),[](char c){return(c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-';});}
static std::string read_bounded(const std::string& dir,const std::string& name,uint32_t ms){
 const auto start=GetTickCount64();while(GetTickCount64()-start<ms){
  if(GetFileAttributesA((dir+"\\native.abort").c_str())!=INVALID_FILE_ATTRIBUTES)throw Stop(3,"native abort/quarantine; no normal release");
  FILE*f=fopen((dir+"\\"+name).c_str(),"rb");if(f){std::string text;char b[4096];size_t n;while((n=fread(b,1,sizeof b,f))){text.append(b,n);if(text.size()>65536){fclose(f);throw Stop(2,"control exceeds bounded bytes");}}bool ok=!ferror(f);fclose(f);require(ok,"read control");return text;}Sleep(5);
 }throw Stop(3,"bounded control timeout "+name+"; hold resources/no normal release");
}
static std::string fnv_bytes(const void*p,size_t bytes){uint64_t h=0xcbf29ce484222325ull;const auto*b=static_cast<const uint8_t*>(p);for(size_t i=0;i<bytes;i++)h=(h^b[i])*0x100000001b3ull;char s[17]{};snprintf(s,sizeof s,"%016llx",(unsigned long long)h);return s;}
