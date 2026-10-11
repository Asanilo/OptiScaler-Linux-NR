#pragma once
// SPDX-License-Identifier: GPL-3.0-or-later
// Pure deterministic control routing; no Wine/Vulkan/device/model calls.
#include <cstdint>
#include <cstdio>
#include <string>
#include <stdexcept>
namespace quality_paths {
inline constexpr const char* layout="frame-buckets-64-v1";
inline bool allowed_count(uint64_t n){return n==1||n==4||n==120||n==1800;}
inline uint64_t bucket_count(uint64_t n){if(!allowed_count(n))throw std::invalid_argument("bounded finite frame_count required");return (n+63)/64;}
template<class Json>bool layout_matches(const Json&j){return j.is_object()&&j.contains("transport_layout")&&j.at("transport_layout").is_string()&&j.at("transport_layout")==layout;}
inline std::string frame_relative(uint64_t f,uint64_t n,char sep){
 if(!allowed_count(n)||f>=n||(sep!='/'&&sep!='\\'))throw std::invalid_argument("frame routing outside declared count");
 char leaf[32]{};std::snprintf(leaf,sizeof leaf,"bucket-%06llu",(unsigned long long)(f/64));return std::string("frames")+sep+leaf;
}
inline std::string frame_dir(const std::string&root,uint64_t f,uint64_t n,char sep){
 if(root.empty()||root.find('\0')!=std::string::npos)throw std::invalid_argument("nonempty root required");
 return root+sep+frame_relative(f,n,sep);
}
inline std::string frame_path(const std::string&root,uint64_t f,uint64_t n,const std::string&basename,char sep){
 if(basename.empty()||basename=="."||basename=="..")throw std::invalid_argument("exact basename required");
 for(char c:basename)if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-'||c=='_'||c=='.'))throw std::invalid_argument("basename cannot escape declared leaf");
 return frame_dir(root,f,n,sep)+sep+basename;
}
}
