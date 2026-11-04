#pragma once
// Runtime transformation of the user's own grass shader. No extracted game
// shader is distributed. All accepted inputs are exact, previously inspected
// D2R 3.3.93847 DXBCs; unknown shaders retain the game's pipeline.
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <span>
#include <vector>

namespace framevr::foliage {
struct Parameters {
    float footprint=1,originX=1,originZ=1,spacing=1,camera=1,heightScale=1,heightX=1,heightZ=1;
    bool operator==(const Parameters&)const=default;
};
inline float Slider(float v){return std::isfinite(v)?std::round(std::clamp(v,0.f,10.f)*20.f)/20.f:1.f;}
inline Parameters Normalize(Parameters p){
    p.footprint=Slider(p.footprint);p.originX=Slider(p.originX);p.originZ=Slider(p.originZ);
    p.spacing=Slider(p.spacing);p.camera=Slider(p.camera);p.heightScale=Slider(p.heightScale);
    p.heightX=Slider(p.heightX);p.heightZ=Slider(p.heightZ);return p;
}
// Origin controls use 1 as neutral, with 90 world units per step of 1.
// Spacing controls the generated square's width. Footprint independently clips
// that square: widening the clip cannot generate additional candidates.
inline std::array<float,2> OriginShift(Parameters p){return {90.f*(p.originX-p.spacing),90.f*(p.originZ-p.spacing)};}
inline uint64_t Hash(std::span<const uint8_t> b){uint64_t h=1469598103934665603ull;for(auto v:b)h=(h^v)*1099511628211ull;return h;}
inline bool Supported(std::span<const uint8_t> b){
    const auto h=Hash(b);
    return (b.size()==31388&&(h==0x8a2dce3f6b00ecb1ull||h==0x4c2ddfc23a098efeull||h==0xb19763763633b60full))||
           (b.size()==31468&&h==0x87613ddc1edc699eull);
}
namespace dxbc {
using Bytes=std::vector<uint8_t>;
inline uint32_t Get(std::span<const uint8_t> b,size_t o){uint32_t v;std::memcpy(&v,b.data()+o,4);return v;}
inline void Put(Bytes& b,size_t o,uint32_t v){std::memcpy(b.data()+o,&v,4);}
inline uint32_t F(float f){return std::bit_cast<uint32_t>(f);}
inline void Words(Bytes& b,std::initializer_list<uint32_t> w){for(auto v:w){const auto n=b.size();b.resize(n+4);Put(b,n,v);}}
inline void Add(Bytes& b,uint32_t r,float x,float y){Words(b,{0x0a000000,0x100032,r,0x100046,r,0x40f2,F(x),F(y),0,0});}
inline void Md5(std::array<uint32_t,4>& state,const uint8_t* block){
    static constexpr unsigned shifts[]={7,12,17,22,5,9,14,20,4,11,16,23,6,10,15,21};
    static const auto constants=[](){std::array<uint32_t,64> k{};for(int i=0;i<64;++i)k[i]=uint32_t(std::abs(std::sin(double(i+1)))*4294967296.);return k;}();
    uint32_t a=state[0],b=state[1],c=state[2],d=state[3];
    for(unsigned i=0;i<64;++i){
        uint32_t f,g;
        if(i<16){f=(b&c)|(~b&d);g=i;}else if(i<32){f=(d&b)|(~d&c);g=(5*i+1)%16;}
        else if(i<48){f=b^c^d;g=(3*i+5)%16;}else{f=c^(b|~d);g=7*i%16;}
        uint32_t word;std::memcpy(&word,block+4*g,4);
        const auto next=b+std::rotl(a+f+constants[i]+word,int(shifts[(i/16)*4+i%4]));a=d;d=c;c=b;b=next;
    }
    state[0]+=a;state[1]+=b;state[2]+=c;state[3]+=d;
}
// DXBC uses MD5 compression with Microsoft's distinct final-block layout.
inline void Checksum(Bytes& b){
    std::array<uint32_t,4> s{0x67452301,0xefcdab89,0x98badcfe,0x10325476};
    const size_t size=b.size()-20,full=size/64,rem=size%64;const uint32_t bits=uint32_t(size*8);
    for(size_t i=0;i<full;++i)Md5(s,b.data()+20+64*i);
    std::array<uint8_t,64> last{};const auto* rest=b.data()+20+64*full;
    if(rem>=56){std::memcpy(last.data(),rest,rem);last[rem]=0x80;Md5(s,last.data());last.fill(0);}
    else {std::memcpy(last.data()+4,rest,rem);last[4+rem]=0x80;}
    std::memcpy(last.data(),&bits,4);const uint32_t end=(bits>>2)|1;std::memcpy(last.data()+60,&end,4);Md5(s,last.data());
    std::memcpy(b.data()+4,s.data(),16);
}
struct Instruction {size_t begin,end;};
inline bool Program(const Bytes& b,size_t& header,size_t& begin,size_t& end,std::vector<Instruction>& instructions){
    if(b.size()<32||std::memcmp(b.data(),"DXBC",4)||Get(b,24)!=b.size())return false;
    const auto count=Get(b,28);if(count<1||count>16||32+count*4>b.size())return false;
    header=Get(b,32+4*(count-1));if(header+8>b.size()||std::memcmp(b.data()+header,"SHEX",4))return false;
    begin=header+8;end=begin+Get(b,header+4);
    if(end!=b.size()||end-begin<8||Get(b,begin+4)*4!=end-begin)return false;
    for(size_t pos=begin+8;pos<end;){
        const auto words=(Get(b,pos)>>24)&0x7f;
        if(!words||pos+4*words>end)return false;
        instructions.push_back({pos,pos+4*words});pos+=4*words;
    }
    return true;
}
inline bool VecAt(const Bytes& b,Instruction i,std::array<float,4> values,size_t& found){
    bool match=false;
    for(size_t p=i.begin+4;p+16<=i.end;p+=4){
        bool same=true;for(int k=0;k<4;++k)same&=Get(b,p+4*k)==F(values[k]);
        if(same){if(match)return false;found=p;match=true;}
    }
    return match;
}
inline void SetVec(Bytes& b,size_t p,std::array<float,4> v){for(int k=0;k<4;++k)Put(b,p+4*k,F(v[k]));}
inline void Finish(Bytes& b,size_t header,size_t begin){Put(b,header+4,uint32_t(b.size()-begin));Put(b,begin+4,uint32_t((b.size()-begin)/4));Put(b,24,uint32_t(b.size()));Checksum(b);}
inline bool Canonical(std::span<const uint8_t> input,Bytes& out){
    if(!Supported(input))return false;
    out.assign(input.begin(),input.end());if(Hash(input)==0x8a2dce3f6b00ecb1ull)return true;
    size_t head,lo,hi;std::vector<Instruction> instructions;if(!Program(out,head,lo,hi,instructions))return false;
    Bytes body;unsigned spacing=0,jitter=0,height=0,clip=0,removed=0;
    Bytes originAdd,uvAdd;Add(originAdd,0,-90,-90);Add(uvAdd,1,.25f,.25f);
    for(auto ins:instructions){
        Bytes one(out.begin()+ins.begin,out.begin()+ins.end);size_t p;
        if(one==originAdd||one==uvAdd){++removed;continue;}
        Instruction local{0,one.size()};
        if(VecAt(one,local,{.3125f,.3125f,0,0},p)){SetVec(one,p,{.15625f,.15625f,0,0});++spacing;}
        else if(VecAt(one,local,{.3125f,0,.3125f,0},p)){SetVec(one,p,{.15625f,0,.15625f,0});++jitter;}
        else if(VecAt(one,local,{1.f/360,1.f/360,0,0},p)){SetVec(one,p,{1.f/180,1.f/180,0,0});++height;}
        else if(VecAt(one,local,{2.1f,2.1f,0,0},p)){SetVec(one,p,{1.05f,1.05f,0,0});++clip;}
        body.insert(body.end(),one.begin(),one.end());
    }
    if(spacing!=1||jitter!=1||height!=1||clip>1||removed!=(input.size()==31468?2u:0u))return false;
    out.resize(lo+8);out.insert(out.end(),body.begin(),body.end());Finish(out,head,lo);
    return out.size()==31388&&Hash(out)==0x8a2dce3f6b00ecb1ull;
}
// The gate uses a new temporary, leaving every native register intact.
inline void FootprintGate(Bytes& b,Parameters p){
    Words(b,{0x09000000,0x100032,17,0x100086,0,0x80208ae6,0x41,10,47}); // P.xz - engine origin
    Add(b,17,-90*p.originX,-90*p.originZ);
    Words(b,{0x0b000031,0x100032,17,0x80100046,0x81,17,0x4002,F(90*p.footprint),F(90*p.footprint),0,0});
    Words(b,{0x07000001,0x100012,17,0x10001a,17,0x10000a,17}); // inside X AND Z
    Words(b,{0x0300001f,0x10000a,17,0x0100003e,0x01000015}); // if_z: ret; endif
}
} // namespace dxbc
inline bool BuildShader(std::span<const uint8_t> input,Parameters parameters,std::vector<uint8_t>& output){
    using namespace dxbc;
    output.clear();Bytes source;if(!Canonical(input,source))return false;
    const auto p=Normalize(parameters);if(p==Parameters{}){output=std::move(source);return true;}
    size_t head,lo,hi;std::vector<Instruction> instructions;if(!Program(source,head,lo,hi,instructions))return false;
    Bytes body;const auto shift=OriginShift(p);unsigned spacing=0,jitter=0,height=0,clip=0,temps=0;
    for(auto ins:instructions){
        Bytes one(source.begin()+ins.begin,source.begin()+ins.end);size_t pos;Instruction local{0,one.size()};
        if((Get(one,0)&0x7ff)==104){if(Get(one,4)!=17)return false;Put(one,4,18);++temps;}
        if(VecAt(one,local,{.15625f,.15625f,0,0},pos)){
            // Zero footprint/spacing suppresses grass, without invalid sampling or extra work.
            if(p.footprint==0||p.spacing==0)Words(body,{0x0100003e});
            SetVec(one,pos,{.15625f*p.spacing,.15625f*p.spacing,0,0});
            Add(one,0,shift[0],shift[1]);++spacing;
        }else if(VecAt(one,local,{.15625f,0,.15625f,0},pos)){
            SetVec(one,pos,{.15625f*p.spacing,0,.15625f*p.spacing,0});
            // Skip the extra boundary test when the footprint contains the grid,
            // including native jitter. Defaults therefore reproduce stock exactly.
            if(p.footprint<p.spacing)FootprintGate(one,p);++jitter;
        }else if(VecAt(one,local,{1.f/180,1.f/180,0,0},pos)){
            SetVec(one,pos,{p.heightScale/180,p.heightScale/180,0,0});
            Add(one,1,.5f*(p.heightX-1),.5f*(p.heightZ-1));++height;
        }else if(VecAt(one,local,{1.05f,1.05f,0,0},pos)){
            SetVec(one,pos,{1.05f*p.camera,1.05f*p.camera,0,0});++clip;
        }
        body.insert(body.end(),one.begin(),one.end());
    }
    if(spacing!=1||jitter!=1||height!=1||clip!=1||temps!=1)return false;
    output.assign(source.begin(),source.begin()+lo+8);output.insert(output.end(),body.begin(),body.end());Finish(output,head,lo);
    return true;
}
} // namespace framevr::foliage
