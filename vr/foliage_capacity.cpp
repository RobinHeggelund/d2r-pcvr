#include "../steamvr/foliage_capacity.h"
#include "../steamvr/foliage.h"
#include "../steamvr/memory.h"
#include <D2RLPlugin/context.h>
#include <windows.h>
#include <atomic>
#include <cstring>

namespace framevr::foliage {
namespace {std::atomic<unsigned> capacityAxis{1};}
unsigned CapacityAxis(){return capacityAxis.load(std::memory_order_acquire);}
void InstallCapacity(const D2RL::PluginContext* ctx,const wchar_t* ini,void (*log)(const char*)){
    using namespace capacity;
    if(CapacityAxis()==2)return;
    if(GetPrivateProfileIntW(L"mode",L"platform",1,ini)==0){if(log)log("VR foliage capacity: monitor mode retains native allocation");return;}
    // Frozen at process startup. This experiment defaults to 4x; setting 1 in
    // the INI restores the native allocation on the next full restart.
    const int requested=GetPrivateProfileIntW(L"foliage",L"candidate_count",4,ini);
    if(requested!=4){if(log)log("VR foliage capacity: native 1x requested (candidate_count; restart-only)");return;}
    if(!ctx||!ctx->exeBase||!ctx->buildName||std::strcmp(ctx->buildName,"93847")){
        if(log)log("VR foliage capacity: 4x requires verified build 93847; retaining native capacity");return;
    }
    const auto base=static_cast<uintptr_t>(ctx->exeBase);
    uint64_t grass=~uint64_t{},args=~uint64_t{};
    if(!ReadMemory(&grass,reinterpret_cast<void*>(base+GrassHandleRva),sizeof grass)||
       !ReadMemory(&args,reinterpret_cast<void*>(base+ArgsHandleRva),sizeof args)||grass||args){
        if(log)log("VR foliage capacity: resources already initialized or unreadable; retaining native capacity (full restart required)");return;
    }
    std::array<uint8_t,InspectSize> code{};std::vector<uint8_t> patch;
    if(!ReadMemory(code.data(),reinterpret_cast<void*>(base+InitRva),code.size())||!Prepare(code,patch)){
        if(log)log("VR foliage capacity: initializer guards unavailable; retaining native capacity");return;
    }
    // Both immediates are updated together before the game constructs either
    // resource or its views. No live GPU allocation is resized or replaced.
    if(!ctx->PatchBytes(InitRva+PatchOffset,code.data()+PatchOffset,static_cast<uint32_t>(patch.size()),
                        patch.data(),static_cast<uint32_t>(patch.size()))){
        if(log)log("VR foliage capacity: startup allocation/view patch rejected; retaining native capacity");return;
    }
    capacityAxis.store(2,std::memory_order_release);
    if(log)log("VR foliage capacity: 4x startup allocation configured: 5308416 records, 810 MiB; awaiting GPU buffer and SRV/UAV verification");
}
}
