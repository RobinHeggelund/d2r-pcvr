// Explicit isolated D3D12/WARP capacity check; never opens the game process.
#include "foliage_capacity.h"
#include <windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
using Microsoft::WRL::ComPtr;
using namespace framevr::foliage::capacity;
static void Check(bool ok,const char* what){if(!ok){std::fprintf(stderr,"FAIL: %s\n",what);std::exit(1);}}
static void Hr(HRESULT hr,const char* what){if(FAILED(hr)){std::fprintf(stderr,"FAIL: %s (%08lx)\n",what,static_cast<unsigned long>(hr));std::exit(1);}}
int main(){
    ComPtr<IDXGIFactory4> factory;Hr(CreateDXGIFactory1(IID_PPV_ARGS(&factory)),"DXGI factory");
    ComPtr<IDXGIAdapter> adapter;Hr(factory->EnumWarpAdapter(IID_PPV_ARGS(&adapter)),"isolated WARP adapter");
    ComPtr<ID3D12Device> dev;Hr(D3D12CreateDevice(adapter.Get(),D3D_FEATURE_LEVEL_11_0,IID_PPV_ARGS(&dev)),"software D3D12 device");
    auto buffer=[&](D3D12_HEAP_TYPE heap,uint64_t bytes,D3D12_RESOURCE_STATES state,D3D12_RESOURCE_FLAGS flags){
        ComPtr<ID3D12Resource> result;D3D12_HEAP_PROPERTIES properties{};properties.Type=heap;
        D3D12_RESOURCE_DESC desc{};desc.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;desc.Width=bytes;
        desc.Height=1;desc.DepthOrArraySize=1;desc.MipLevels=1;desc.SampleDesc.Count=1;desc.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;desc.Flags=flags;
        Hr(dev->CreateCommittedResource(&properties,D3D12_HEAP_FLAG_NONE,&desc,state,nullptr,IID_PPV_ARGS(&result)),"capacity buffer allocation");return result;
    };
    auto records=buffer(D3D12_HEAP_TYPE_DEFAULT,ExpandedBytes,D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    auto args=buffer(D3D12_HEAP_TYPE_DEFAULT,16,D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
    auto upload=buffer(D3D12_HEAP_TYPE_UPLOAD,16,D3D12_RESOURCE_STATE_GENERIC_READ,D3D12_RESOURCE_FLAG_NONE);
    auto readback=buffer(D3D12_HEAP_TYPE_READBACK,64,D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_FLAG_NONE);
    uint32_t* mapped=nullptr;D3D12_RANGE none{0,0};Hr(upload->Map(0,&none,reinterpret_cast<void**>(&mapped)),"map initial draw args");
    mapped[0]=22;mapped[1]=mapped[2]=mapped[3]=0;upload->Unmap(0,nullptr);
    D3D12_DESCRIPTOR_HEAP_DESC hd{};hd.Type=D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;hd.NumDescriptors=3;hd.Flags=D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    ComPtr<ID3D12DescriptorHeap> heap;Hr(dev->CreateDescriptorHeap(&hd,IID_PPV_ARGS(&heap)),"descriptor heap");
    auto cpu=heap->GetCPUDescriptorHandleForHeapStart();const auto increment=dev->GetDescriptorHandleIncrementSize(hd.Type);
    D3D12_UNORDERED_ACCESS_VIEW_DESC uav{};uav.ViewDimension=D3D12_UAV_DIMENSION_BUFFER;uav.Buffer.NumElements=ExpandedCount;uav.Buffer.StructureByteStride=Stride;
    dev->CreateUnorderedAccessView(records.Get(),nullptr,&uav,cpu);cpu.ptr+=increment;
    uav.Format=DXGI_FORMAT_R32_TYPELESS;uav.Buffer.NumElements=4;uav.Buffer.StructureByteStride=0;uav.Buffer.Flags=D3D12_BUFFER_UAV_FLAG_RAW;
    dev->CreateUnorderedAccessView(args.Get(),nullptr,&uav,cpu);cpu.ptr+=increment;
    D3D12_SHADER_RESOURCE_VIEW_DESC srv{};srv.ViewDimension=D3D12_SRV_DIMENSION_BUFFER;srv.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srv.Buffer.NumElements=ExpandedCount;srv.Buffer.StructureByteStride=Stride;dev->CreateShaderResourceView(records.Get(),&srv,cpu);
    D3D12_DESCRIPTOR_RANGE range{};range.RangeType=D3D12_DESCRIPTOR_RANGE_TYPE_UAV;range.NumDescriptors=2;
    D3D12_ROOT_PARAMETER param{};param.ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;param.DescriptorTable.NumDescriptorRanges=1;param.DescriptorTable.pDescriptorRanges=&range;
    D3D12_ROOT_SIGNATURE_DESC rd{};rd.NumParameters=1;rd.pParameters=&param;
    ComPtr<ID3DBlob> signature,errors;Hr(D3D12SerializeRootSignature(&rd,D3D_ROOT_SIGNATURE_VERSION_1,&signature,&errors),"root signature serialization");
    ComPtr<ID3D12RootSignature> root;Hr(dev->CreateRootSignature(0,signature->GetBufferPointer(),signature->GetBufferSize(),IID_PPV_ARGS(&root)),"root signature");
    const char* code=R"(
struct Record { uint4 v[10]; };
RWStructuredBuffer<Record> placement : register(u0);
RWByteAddressBuffer drawArgs : register(u1);
[numthreads(16,16,1)] void main(uint3 id:SV_DispatchThreadID) {
    uint index=id.y*2304+id.x;
    placement[index].v[0]=uint4(index,id.x,id.y,0x1234abcd);
    uint previous;drawArgs.InterlockedAdd(4,1,previous);
})";
    ComPtr<ID3DBlob> shader;Hr(D3DCompile(code,std::strlen(code),"foliage-4x-capacity",nullptr,nullptr,"main","cs_5_0",D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&shader,&errors),"capacity shader compilation");
    D3D12_COMPUTE_PIPELINE_STATE_DESC pd{};pd.pRootSignature=root.Get();pd.CS={shader->GetBufferPointer(),shader->GetBufferSize()};
    ComPtr<ID3D12PipelineState> pso;Hr(dev->CreateComputePipelineState(&pd,IID_PPV_ARGS(&pso)),"capacity pipeline");
    D3D12_COMMAND_QUEUE_DESC qd{};ComPtr<ID3D12CommandQueue> queue;Hr(dev->CreateCommandQueue(&qd,IID_PPV_ARGS(&queue)),"software command queue");
    ComPtr<ID3D12CommandAllocator> allocator;Hr(dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&allocator)),"allocator");
    ComPtr<ID3D12GraphicsCommandList> commands;Hr(dev->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,allocator.Get(),nullptr,IID_PPV_ARGS(&commands)),"command list");
    auto transition=[&](ID3D12Resource* resource,D3D12_RESOURCE_STATES before,D3D12_RESOURCE_STATES after){
        D3D12_RESOURCE_BARRIER b{};b.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;b.Transition={resource,D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES,before,after};commands->ResourceBarrier(1,&b);
    };
    commands->CopyBufferRegion(args.Get(),0,upload.Get(),0,16);transition(args.Get(),D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
    ID3D12DescriptorHeap* heaps[]={heap.Get()};commands->SetDescriptorHeaps(1,heaps);commands->SetComputeRootSignature(root.Get());
    commands->SetPipelineState(pso.Get());commands->SetComputeRootDescriptorTable(0,heap->GetGPUDescriptorHandleForHeapStart());commands->Dispatch(NativeGroups,NativeGroups,1);
    transition(records.Get(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_COPY_SOURCE);
    transition(args.Get(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_COPY_SOURCE);
    commands->CopyBufferRegion(readback.Get(),0,records.Get(),0,16);
    commands->CopyBufferRegion(readback.Get(),16,records.Get(),uint64_t(NativeCount)*Stride,16);
    commands->CopyBufferRegion(readback.Get(),32,records.Get(),uint64_t(ExpandedCount-1)*Stride,16);
    commands->CopyBufferRegion(readback.Get(),48,args.Get(),0,16);Hr(commands->Close(),"close commands");
    ID3D12CommandList* lists[]={commands.Get()};queue->ExecuteCommandLists(1,lists);
    ComPtr<ID3D12Fence> fence;Hr(dev->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&fence)),"fence");Hr(queue->Signal(fence.Get(),1),"signal");
    const HANDLE event=CreateEventW(nullptr,FALSE,FALSE,nullptr);Check(event!=nullptr,"completion event");Hr(fence->SetEventOnCompletion(1,event),"completion fence");
    Check(WaitForSingleObject(event,30000)==WAIT_OBJECT_0,"bounded software GPU wait");CloseHandle(event);
    D3D12_RANGE rangeRead{0,64};Hr(readback->Map(0,&rangeRead,reinterpret_cast<void**>(&mapped)),"read completed GPU results");
    Check(mapped[0]==0&&mapped[3]==0x1234abcd,"first candidate stored");
    Check(mapped[4]==NativeCount&&mapped[7]==0x1234abcd,"candidate beyond original allocation stored");
    Check(mapped[8]==ExpandedCount-1&&mapped[11]==0x1234abcd,"last expanded candidate stored");
    Check(mapped[12]==22&&mapped[13]==ExpandedCount&&mapped[14]==0&&mapped[15]==0,"16-byte indirect args retain vertex count and exact expanded instance count");
    readback->Unmap(0,&none);Hr(dev->GetDeviceRemovedReason(),"software device remains healthy");
    std::printf("D3D12 WARP: 810 MiB allocation, 5308416-element SRV/UAV, 144x144 dispatch with16x16 threads, boundary records and exact instance count verified\n");
}
