#pragma once
// SPDX-License-Identifier: GPL-3.0-or-later
#include <cstdint>
#include <cstring>
inline uint16_t diagnostic_half_rne(float f){uint32_t v;std::memcpy(&v,&f,4);uint16_t s=uint16_t((v>>16)&0x8000);uint32_t e=(v>>23)&255,m=v&0x7fffff;if(e==255)return s|0x7c00|(m?0x200:0);int x=int(e)-127+15;if(x>=31)return s|0x7c00;if(x<=0){if(x<-10)return s;m|=0x800000;unsigned shift=unsigned(14-x);uint32_t q=m>>shift,r=m&((uint32_t(1)<<shift)-1),half=uint32_t(1)<<(shift-1);q+=r>half||(r==half&&(q&1));return s|uint16_t(q);}uint32_t q=m>>13,r=m&8191;q+=r>4096||(r==4096&&(q&1));if(q==1024){q=0;if(++x==31)return s|0x7c00;}return s|uint16_t(x<<10)|uint16_t(q);}
