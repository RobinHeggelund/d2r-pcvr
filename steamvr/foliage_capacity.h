#pragma once
#include <array>
#include <cstdint>
#include <cstring>
#include <span>
#include <vector>

namespace framevr::foliage::capacity {
inline constexpr uint32_t NativeGroups=144,NativeThreads=8,Stride=160;
inline constexpr uint32_t NativeCount=NativeGroups*NativeThreads*NativeGroups*NativeThreads;
inline constexpr uint32_t ExpandedCount=NativeCount*4,NativeBytes=NativeCount*Stride,ExpandedBytes=ExpandedCount*Stride;
inline constexpr uint64_t InitRva=0xff8f60,GrassHandleRva=0x35487b0,ArgsHandleRva=0x35487c0;
inline constexpr size_t PatchOffset=0x73,PatchEnd=0xf6;
inline constexpr std::array<uint8_t,20> InitPrefix={0x48,0x89,0x5c,0x24,0x10,0x48,0x89,0x74,0x24,0x18,0x48,0x89,0x7c,0x24,0x20,0x55,0x41,0x56,0x41,0x57};
inline constexpr std::array<uint8_t,10> Allocation={0xc7,0x85,0xbc,0,0,0,0,0,0xa8,0x0c};
inline constexpr std::array<uint8_t,7> View={0xc7,0x45,0xc0,0,0x40,0x14,0};
inline constexpr std::array<uint8_t,7> GroupsX={0xc7,0x43,0x20,0x90,0,0,0},GroupsY={0xc7,0x43,0x24,0x90,0,0,0};
inline constexpr size_t InspectSize=0x1eb;
template<size_t N> inline bool Match(std::span<const uint8_t> code,size_t at,const std::array<uint8_t,N>& expected){
    return at<=code.size()&&N<=code.size()-at&&!std::memcmp(code.data()+at,expected.data(),N);
}
// Allocate/view capacities change in ONE loader-managed patch transaction.
// Dispatch groups stay native; the replacement shader doubles its group side.
inline bool Prepare(std::span<const uint8_t> code,std::vector<uint8_t>& patch){
    patch.clear();
    if(code.size()<InspectSize||!Match(code,0,InitPrefix)||!Match(code,0x73,Allocation)||!Match(code,0xef,View)||
       !Match(code,0x1d6,GroupsX)||!Match(code,0x1e4,GroupsY))return false;
    patch.assign(code.begin()+PatchOffset,code.begin()+PatchEnd);
    const uint32_t bytes=ExpandedBytes,count=ExpandedCount;
    std::memcpy(patch.data()+0x79-PatchOffset,&bytes,4);
    std::memcpy(patch.data()+0xf2-PatchOffset,&count,4);
    return true;
}
inline bool FullView(uint64_t offset,uint32_t count,uint32_t stride){return offset==0&&stride==Stride&&count>=ExpandedCount;}
}
