#include "foliage.h"
#include "foliage_capacity.h"
#include "generation_cache.h"
#include "native_lock.h"
#include "../vr/game_device_only.h"
#include <atomic>
#include <cstdio>
#include <mutex>
#include <unordered_map>
#include <vector>

namespace framevr::foliage {
namespace {
using namespace reshade::api;
struct Variant {Parameters parameters;unsigned axis=1;pipeline replacement{};};
struct Entry {
    device* owner=nullptr;pipeline original{};pipeline_layout layout{};
    std::vector<uint8_t> native;
    pipeline_flags flags=pipeline_flags::none;
    std::vector<dynamic_state> dynamicStates;
    // Recorded command lists may still reference an older variant after Apply.
    // Retain variants for the same lifetime as the original game pipeline.
    std::vector<Variant> variants;
};
struct Bound {device* owner=nullptr;pipeline replacement{};unsigned axis=1;};
struct CapacityBuffer {device* owner;resource buffer;};
struct CapacityView {device* owner;resource buffer;resource_view view;bool srv,uav;};
NativeMutex mutex;
std::vector<Entry> entries;
std::vector<CapacityBuffer> capacityBuffers;
std::vector<CapacityView> capacityViews;
std::unordered_map<command_list*,device*> expandedLists;
Parameters parameters;
std::atomic<uint64_t> generation{1};
bool (*enabled)()=nullptr;
thread_local bool ours=false;
void (*logger)(const char*)=nullptr;
struct OwnCalls {OwnCalls(){ours=true;}~OwnCalls(){ours=false;}};
bool Ready(device* dev){
    for(const auto& buffer:capacityBuffers)if(buffer.owner==dev){
        bool srv=false,uav=false;
        for(const auto& view:capacityViews)if(view.owner==dev&&view.buffer.handle==buffer.buffer.handle){srv|=view.srv;uav|=view.uav;}
        if(srv&&uav)return true;
    }
    return false;
}
void ResourceCreated(device* dev,const resource_desc& desc,const subresource_data*,resource_usage,resource buffer){
    if(CapacityAxis()!=2||desc.type!=resource_type::buffer||desc.buffer.size!=capacity::ExpandedBytes)return;
    std::lock_guard lock(mutex);capacityBuffers.push_back({dev,buffer});++generation;
    if(logger)logger("VR foliage capacity: actual 810 MiB GPU buffer observed; awaiting matching structured SRV and UAV");
}
void ViewCreated(device* dev,resource buffer,resource_usage usage,const resource_view_desc& desc,resource_view view){
    if(CapacityAxis()!=2||desc.type!=resource_view_type::buffer||desc.format!=format::unknown||
       !capacity::FullView(desc.buffer.offset,desc.buffer.structured.count,desc.buffer.structured.stride))return;
    std::lock_guard lock(mutex);
    if(!std::any_of(capacityBuffers.begin(),capacityBuffers.end(),[&](const auto& b){return b.owner==dev&&b.buffer.handle==buffer.handle;}))return;
    const bool before=Ready(dev);
    capacityViews.push_back({dev,buffer,view,(usage&resource_usage::shader_resource)!=resource_usage{},(usage&resource_usage::unordered_access)!=resource_usage{}});
    ++generation;
    if(!before&&Ready(dev)&&logger)logger("VR foliage capacity: GPU buffer plus 5308416-element SRV/UAV verified; 4x candidate shaders enabled");
}
void ViewDestroyed(device* dev,resource_view view){
    std::lock_guard lock(mutex);
    const auto removed=std::erase_if(capacityViews,[&](const auto& v){return v.owner==dev&&v.view.handle==view.handle;});
    if(removed)++generation;
}
void ResourceDestroyed(device* dev,resource buffer){
    std::lock_guard lock(mutex);
    const auto removed=std::erase_if(capacityBuffers,[&](const auto& b){return b.owner==dev&&b.buffer.handle==buffer.handle;});
    if(removed){std::erase_if(capacityViews,[&](const auto& v){return v.owner==dev&&v.buffer.handle==buffer.handle;});++generation;}
}
void Reset(command_list* commands){std::lock_guard lock(mutex);expandedLists.erase(commands);}
bool Dispatch(command_list* commands,uint32_t x,uint32_t y,uint32_t z){
    if(ours||CapacityAxis()!=2)return false;
    std::lock_guard lock(mutex);
    const auto it=expandedLists.find(commands);if(it==expandedLists.end()||!it->second)return false;
    // The group count is part of the capacity proof. Skip an unexpected grass
    // dispatch rather than let a larger grid append beyond the verified view.
    if(x==capacity::NativeGroups&&y==capacity::NativeGroups&&z==1&&Ready(commands->get_device()))return false;
    static bool told=false;if(!told&&logger){told=true;logger("VR foliage capacity: unexpected dispatch or lost views; expanded grass dispatch skipped");}
    return true;
}
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
        if(logger)logger("VR foliage: verified grass placement shader captured; live controls ready");
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
        std::erase_if(capacityBuffers,[&](const auto& b){return b.owner==dev;});
        std::erase_if(capacityViews,[&](const auto& v){return v.owner==dev;});
        std::erase_if(expandedLists,[&](const auto& pair){return pair.second==dev;});
        ++generation;
    }
    OwnCalls own;for(const auto& v:pending)if(v.replacement.handle)dev->destroy_pipeline(v.replacement);
}
void Bind(command_list* commands,pipeline_stage stages,pipeline original){
    if(ours||(stages&pipeline_stage::compute_shader)==pipeline_stage{})return;
    if(!enabled||!enabled()){if(CapacityAxis()==2)Reset(commands);return;}
    static thread_local GenerationCache<Bound> cache;
    Bound found{};const auto version=generation.load(std::memory_order_acquire);auto* dev=commands->get_device();
    if(!cache.Get(original.handle,version,found)){
        std::lock_guard lock(mutex);
        for(auto& entry:entries)if(entry.owner==dev&&entry.original.handle==original.handle){
            found.owner=dev;
            found.axis=CapacityAxis()==2&&Ready(dev)?2:1;
            for(const auto& v:entry.variants)if(v.parameters==parameters&&v.axis==found.axis){found.replacement=v.replacement;break;}
            const bool built=std::any_of(entry.variants.begin(),entry.variants.end(),[&](const Variant& v){return v.parameters==parameters&&v.axis==found.axis;});
            if(!built){
                std::vector<uint8_t> code;
                if(BuildShader(entry.native,parameters,code,found.axis)){
                    shader_desc shader{};shader.code=code.data();shader.code_size=code.size();
                    pipeline_subobject parts[]={{pipeline_subobject_type::compute_shader,1,&shader},
                        {pipeline_subobject_type::flags,1,&entry.flags},
                        {pipeline_subobject_type::dynamic_pipeline_states,uint32_t(entry.dynamicStates.size()),entry.dynamicStates.data()}};
                    OwnCalls own;
                    if(!dev->create_pipeline(entry.layout,entry.dynamicStates.empty()?2:3,parts,&found.replacement))found.replacement={};
                }
                entry.variants.push_back({parameters,found.axis,found.replacement});
                if(logger)logger(found.replacement.handle?(found.axis==2?"VR foliage: live grass parameters activated with 4x candidates (16x16 threads, native dispatch)":"VR foliage: live grass parameters activated with native candidate count"):
                    "VR foliage: shader variant failed; retaining native grass");
            }
            break;
        }
        cache.Put(original.handle,version,found);
    }
    if(CapacityAxis()==2){std::lock_guard lock(mutex);if(found.owner==dev&&found.replacement.handle&&found.axis==2)expandedLists[commands]=dev;else expandedLists.erase(commands);}
    if(found.owner!=dev||!found.replacement.handle)return;
    OwnCalls own;commands->bind_pipeline(stages,found.replacement);
}
}
void Register(void (*log)(const char*),bool (*isEnabled)()){
    logger=log;enabled=isEnabled;
    reshade::register_event<reshade::addon_event::init_resource>(&d2rvr::D3D12Only<&ResourceCreated>::Call);
    reshade::register_event<reshade::addon_event::destroy_resource>(&d2rvr::D3D12Only<&ResourceDestroyed>::Call);
    reshade::register_event<reshade::addon_event::init_resource_view>(&d2rvr::D3D12Only<&ViewCreated>::Call);
    reshade::register_event<reshade::addon_event::destroy_resource_view>(&d2rvr::D3D12Only<&ViewDestroyed>::Call);
    reshade::register_event<reshade::addon_event::dispatch>(&d2rvr::D3D12Only<&Dispatch>::Call);
    reshade::register_event<reshade::addon_event::reset_command_list>(&d2rvr::D3D12Only<&Reset>::Call);
    reshade::register_event<reshade::addon_event::destroy_command_list>(&d2rvr::D3D12Only<&Reset>::Call);
    reshade::register_event<reshade::addon_event::init_pipeline>(&d2rvr::D3D12Only<&Created>::Call);
    reshade::register_event<reshade::addon_event::destroy_pipeline>(&d2rvr::D3D12Only<&Destroyed>::Call);
    reshade::register_event<reshade::addon_event::destroy_device>(&d2rvr::D3D12Only<&DeviceDestroyed>::Call);
    reshade::register_event<reshade::addon_event::bind_pipeline>(&d2rvr::D3D12Only<&Bind>::Call);
}
void Configure(Parameters p){
    std::lock_guard lock(mutex);p=Normalize(p);if(p==parameters)return;
    parameters=p;++generation;
    if(logger){
        char text[384];std::snprintf(text,sizeof text,"VR foliage: requested footprint %.2f, origins X/Z %.2f/%.2f, spacing %.2f, camera %.2f, height scale %.2f offsets %.2f/%.2f, local exclusion %s, room mask %s",
            p.footprint,p.originX,p.originZ,p.spacing,p.camera,p.heightScale,p.heightX,p.heightZ,p.localExclusion?"native":"BYPASSED (diagnostic)",p.roomMask?"native":"BYPASSED (diagnostic)");logger(text);
    }
}
}
