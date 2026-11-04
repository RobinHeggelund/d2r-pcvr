#include "foliage.h"
#include "generation_cache.h"
#include "native_lock.h"
#include "../vr/game_device_only.h"
#include <atomic>
#include <cstdio>
#include <mutex>
#include <vector>

namespace framevr::foliage {
namespace {
using namespace reshade::api;
struct Variant {Parameters parameters;pipeline replacement{};};
struct Entry {
    device* owner=nullptr;pipeline original{};pipeline_layout layout{};
    std::vector<uint8_t> native;
    pipeline_flags flags=pipeline_flags::none;
    std::vector<dynamic_state> dynamicStates;
    // Recorded command lists may still reference an older variant after Apply.
    // Retain variants for the same lifetime as the original game pipeline.
    std::vector<Variant> variants;
};
struct Bound {device* owner=nullptr;pipeline replacement{};};
NativeMutex mutex;
std::vector<Entry> entries;
Parameters parameters;
std::atomic<uint64_t> generation{1};
bool (*enabled)()=nullptr;
thread_local bool ours=false;
void (*logger)(const char*)=nullptr;
struct OwnCalls {OwnCalls(){ours=true;}~OwnCalls(){ours=false;}};
void Created(device* dev,pipeline_layout layout,uint32_t count,const pipeline_subobject* parts,pipeline original){
    if(ours)return;
    for(uint32_t i=0;i<count;++i){
        if(parts[i].type!=pipeline_subobject_type::compute_shader||parts[i].count!=1||!parts[i].data)continue;
        const auto& shader=*static_cast<const shader_desc*>(parts[i].data);
        if(!shader.code||!Supported({static_cast<const uint8_t*>(shader.code),shader.code_size}))return;
        Entry e;e.owner=dev;e.original=original;e.layout=layout;
        if(shader.spec_constants!=0)return;
        // ReShade also supplies dynamic-state metadata for D3D12 compute
        // pipelines. Own these arrays; event description pointers are temporary.
        for(uint32_t j=0;j<count;++j)if(j!=i){
            const auto& part=parts[j];
            if(!part.count)continue;
            if(!part.data)return;
            if(part.type==pipeline_subobject_type::flags&&part.count==1)e.flags=*static_cast<const pipeline_flags*>(part.data);
            else if(part.type==pipeline_subobject_type::dynamic_pipeline_states){
                const auto* first=static_cast<const dynamic_state*>(part.data);e.dynamicStates.assign(first,first+part.count);
            }else {if(logger)logger("VR foliage: unsupported compute pipeline description; retaining native grass");return;}
        }
        const auto* start=static_cast<const uint8_t*>(shader.code);e.native.assign(start,start+shader.code_size);
        {std::lock_guard lock(mutex);
            for(const auto& old:entries)if(old.owner==dev&&old.original.handle==original.handle)return;
            entries.push_back(std::move(e));++generation;
        }
        if(logger)logger("VR foliage: verified grass placement shader captured; live controls ready (native candidate capacity)");
        return;
    }
}
void Destroyed(device* dev,pipeline original){
    if(ours)return;
    std::vector<Variant> pending;
    {std::lock_guard lock(mutex);
        for(auto it=entries.begin();it!=entries.end();++it)if(it->owner==dev&&it->original.handle==original.handle){pending=std::move(it->variants);entries.erase(it);++generation;break;}
    }
    OwnCalls own;for(const auto& v:pending)if(v.replacement.handle)dev->destroy_pipeline(v.replacement);
}
void DeviceDestroyed(device* dev){
    if(ours)return;
    std::vector<Variant> pending;
    {std::lock_guard lock(mutex);
        for(auto it=entries.begin();it!=entries.end();)if(it->owner==dev){pending.insert(pending.end(),it->variants.begin(),it->variants.end());it=entries.erase(it);}else ++it;
        ++generation;
    }
    OwnCalls own;for(const auto& v:pending)if(v.replacement.handle)dev->destroy_pipeline(v.replacement);
}
void Bind(command_list* commands,pipeline_stage stages,pipeline original){
    if(ours||(stages&pipeline_stage::compute_shader)==pipeline_stage{}||!enabled||!enabled())return;
    static thread_local GenerationCache<Bound> cache;
    Bound found{};const auto version=generation.load(std::memory_order_acquire);auto* dev=commands->get_device();
    if(!cache.Get(original.handle,version,found)){
        std::lock_guard lock(mutex);
        for(auto& entry:entries)if(entry.owner==dev&&entry.original.handle==original.handle){
            found.owner=dev;
            for(const auto& v:entry.variants)if(v.parameters==parameters){found.replacement=v.replacement;break;}
            const bool built=std::any_of(entry.variants.begin(),entry.variants.end(),[&](const Variant& v){return v.parameters==parameters;});
            if(!built){
                std::vector<uint8_t> code;
                if(BuildShader(entry.native,parameters,code)){
                    shader_desc shader{};shader.code=code.data();shader.code_size=code.size();
                    pipeline_subobject parts[]={{pipeline_subobject_type::compute_shader,1,&shader},
                        {pipeline_subobject_type::flags,1,&entry.flags},
                        {pipeline_subobject_type::dynamic_pipeline_states,uint32_t(entry.dynamicStates.size()),entry.dynamicStates.data()}};
                    OwnCalls own;
                    if(!dev->create_pipeline(entry.layout,entry.dynamicStates.empty()?2:3,parts,&found.replacement))found.replacement={};
                }
                entry.variants.push_back({parameters,found.replacement});
                if(logger)logger(found.replacement.handle?"VR foliage: live grass parameters activated; no restart required":"VR foliage: shader variant failed; retaining native grass");
            }
            break;
        }
        cache.Put(original.handle,version,found);
    }
    if(found.owner!=dev||!found.replacement.handle)return;
    OwnCalls own;commands->bind_pipeline(stages,found.replacement);
}
}
void Register(void (*log)(const char*),bool (*isEnabled)()){
    logger=log;enabled=isEnabled;
    reshade::register_event<reshade::addon_event::init_pipeline>(&d2rvr::D3D12Only<&Created>::Call);
    reshade::register_event<reshade::addon_event::destroy_pipeline>(&d2rvr::D3D12Only<&Destroyed>::Call);
    reshade::register_event<reshade::addon_event::destroy_device>(&d2rvr::D3D12Only<&DeviceDestroyed>::Call);
    reshade::register_event<reshade::addon_event::bind_pipeline>(&d2rvr::D3D12Only<&Bind>::Call);
}
void Configure(Parameters p){
    std::lock_guard lock(mutex);p=Normalize(p);if(p==parameters)return;
    parameters=p;++generation;
    if(logger){
        char text[256];std::snprintf(text,sizeof text,"VR foliage: requested footprint %.2f, origins X/Z %.2f/%.2f, spacing %.2f, camera %.2f, height scale %.2f offsets %.2f/%.2f",
            p.footprint,p.originX,p.originZ,p.spacing,p.camera,p.heightScale,p.heightX,p.heightZ);logger(text);
    }
}
}
