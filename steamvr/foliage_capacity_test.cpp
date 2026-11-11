#include "foliage_capacity.h"
#include "foliage.h"
#include <D2RLPlugin/context.h>
#include <windows.h>
#include <cstdio>
#include <cstdlib>
#include <string>
using namespace framevr::foliage;
using namespace framevr::foliage::capacity;
static unsigned checks=0,patches=0;static bool reject=false;static std::string last;
static void Check(bool ok,const char* label){++checks;if(!ok){std::fprintf(stderr,"FAIL: %s\n",label);std::exit(1);}}
static void Log(const char* text){last=text;}
static bool Patch(const D2RL::PluginContext* ctx,uint64_t rva,const void* expected,uint32_t expectedSize,const void* replacement,uint32_t size) noexcept {
    ++patches;auto* target=reinterpret_cast<uint8_t*>(ctx->exeBase+rva);
    if(reject||rva!=InitRva+PatchOffset||size!=PatchEnd-PatchOffset||size!=expectedSize||std::memcmp(target,expected,size))return false;
    std::memcpy(target,replacement,size);return true;
}
static std::array<uint8_t,InspectSize> Fixture(){
    std::array<uint8_t,InspectSize> c{};c.fill(0xcc);
    const auto put=[&](size_t at,const auto& v){std::memcpy(c.data()+at,v.data(),v.size());};
    put(0,InitPrefix);put(0x73,Allocation);put(0xef,View);put(0x1d6,GroupsX);put(0x1e4,GroupsY);return c;
}
int main(){
    const auto code=Fixture();std::vector<uint8_t> patch;
    Check(Prepare(code,patch),"verified initializer creates coherent capacity patch");
    auto changed=code;std::memcpy(changed.data()+PatchOffset,patch.data(),patch.size());
    uint32_t bytes,count;std::memcpy(&bytes,changed.data()+0x79,4);std::memcpy(&count,changed.data()+0xf2,4);
    Check(bytes==849346560&&count==5308416&&bytes==count*Stride,"allocation exactly contains four times the candidates");
    for(size_t i=0;i<code.size();++i)if(!(i>=0x79&&i<0x7d)&&!(i>=0xf2&&i<0xf6))Check(code[i]==changed[i],"only allocation and view immediates change");
    for(const auto range:{std::pair{size_t(0),InitPrefix.size()},std::pair{size_t(0x73),Allocation.size()},std::pair{size_t(0xef),View.size()},std::pair{size_t(0x1d6),GroupsX.size()},std::pair{size_t(0x1e4),GroupsY.size()}})
        for(size_t i=range.first;i<range.first+range.second;++i){auto bad=code;bad[i]^=1;Check(!Prepare(bad,patch)&&patch.empty(),"every signature byte is required before mutation");}
    Check(!Prepare(std::span(code).first(InspectSize-1),patch),"short initializer rejected");
    Check(FullView(0,ExpandedCount,Stride)&&!FullView(0,NativeCount,Stride)&&!FullView(160,ExpandedCount,Stride)&&!FullView(0,ExpandedCount,16),"expanded shader requires full correctly-strided zero-offset view");
    // Use actual Windows guarded reads and the SDK patch adapter against owned
    // virtual memory. No game process, shader, installation or save is opened.
    auto* image=static_cast<uint8_t*>(VirtualAlloc(nullptr,0x3550000,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE));Check(image!=nullptr,"owned module-sized address range");
    std::memcpy(image+InitRva,code.data(),code.size());
    D2RL::PluginApi api{};api.apiSize=sizeof api;api.patchBytes=&Patch;
    D2RL::PluginContext ctx{};ctx.contextSize=sizeof ctx;ctx.api=&api;ctx.exeBase=reinterpret_cast<uintptr_t>(image);ctx.buildName="93847";
    wchar_t folder[MAX_PATH],path[MAX_PATH];Check(GetTempPathW(MAX_PATH,folder)&&GetTempFileNameW(folder,L"fgc",0,path),"isolated INI");
    Check(WritePrivateProfileStringW(L"foliage",L"candidate_count",L"1",path),"native count requested");
    InstallCapacity(&ctx,path,Log);Check(patches==0&&CapacityAxis()==1,"native opt-out makes no patch");
    Check(WritePrivateProfileStringW(L"foliage",L"candidate_count",L"4",path),"four-times count requested");
    Check(WritePrivateProfileStringW(L"mode",L"platform",L"0",path),"monitor mode requested");
    InstallCapacity(&ctx,path,Log);Check(patches==0&&CapacityAxis()==1,"monitor mode retains native capacity");
    Check(WritePrivateProfileStringW(L"mode",L"platform",L"1",path),"VR mode restored without requiring headset state");
    ctx.buildName="unknown";InstallCapacity(&ctx,path,Log);Check(patches==0&&CapacityAxis()==1,"unknown build untouched");ctx.buildName="93847";
    *reinterpret_cast<uint64_t*>(image+GrassHandleRva)=1;InstallCapacity(&ctx,path,Log);Check(patches==0&&CapacityAxis()==1,"already allocated grass cannot be resized live");*reinterpret_cast<uint64_t*>(image+GrassHandleRva)=0;
    *reinterpret_cast<uint64_t*>(image+ArgsHandleRva)=1;InstallCapacity(&ctx,path,Log);Check(patches==0&&CapacityAxis()==1,"already allocated draw args block late installation");*reinterpret_cast<uint64_t*>(image+ArgsHandleRva)=0;
    image[InitRva+0x73]^=1;InstallCapacity(&ctx,path,Log);Check(patches==0&&CapacityAxis()==1,"mismatched code untouched");image[InitRva+0x73]^=1;
    reject=true;InstallCapacity(&ctx,path,Log);Check(patches==1&&CapacityAxis()==1&&!std::memcmp(image+InitRva,code.data(),code.size()),"rejected atomic loader patch leaves native code and shader size");
    reject=false;InstallCapacity(&ctx,path,Log);Check(patches==2&&CapacityAxis()==2&&!std::memcmp(image+InitRva,changed.data(),changed.size()),"single loader patch grows allocation and views together");
    InstallCapacity(&ctx,path,Log);Check(patches==2,"startup configuration is frozen and idempotent");
    Check(DeleteFileW(path)&&VirtualFree(image,0,MEM_RELEASE),"owned fixture cleanup");
    std::printf("%u foliage capacity checks passed\n",checks);
}
