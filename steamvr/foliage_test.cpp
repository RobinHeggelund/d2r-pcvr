#include "foliage_settings.h"
#include "vr_settings_content.h"
#include <windows.h>
#include <d3d11.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iterator>
#include <limits>
#include <string>
using Microsoft::WRL::ComPtr;
using namespace framevr::foliage;
static int checks=0;
static void Check(bool condition,const char* label){++checks;if(!condition){std::fprintf(stderr,"FAIL: %s\n",label);std::exit(1);}}
static void Basic(){
    using namespace framevr::settings;
    Check(FromSettings(Defaults())==Parameters{},"menu defaults reproduce native parameters");
    for(int key=FoliageFootprint;key<=FoliageHeightZ;++key){
        const auto& d=Definitions[key];Check(d.min==0&&d.max==10&&d.step==.05f&&d.defaultValue==1,"requested slider range and neutral default");
        const auto* row=content::RowOf(Key(key));Check(row&&row->page==View::Foliage&&row->kind==content::RowKind::Slider,"all controls on dedicated panel");
    }
    auto p=Parameters{};p.spacing=2;Check(OriginShift(p)==std::array<float,2>{-90,-90},"doubling spacing preserves native centre");
    p.originX=2;p.originZ=0;Check(OriginShift(p)==std::array<float,2>{0,-180},"both ground axes move independently");
    Check(Slider(-1)==0&&Slider(11)==10&&Slider(.124f)==.1f&&Slider(std::numeric_limits<float>::quiet_NaN())==1,"invalid and fractional parameters are bounded");
    std::vector<uint8_t> out{1,2,3};const std::vector<uint8_t> bad(32);
    Check(!BuildShader(bad,{},out)&&out.empty(),"unknown shader fails closed with no stale output");
    // Check checksum implementation against Microsoft's compiler on our own code.
    const char* hlsl="RWByteAddressBuffer result:register(u0);[numthreads(8,8,1)]void main(uint3 id:SV_DispatchThreadID){result.Store(id.x*4,id.y+7);}";
    ComPtr<ID3DBlob> code,errors;
    Check(SUCCEEDED(D3DCompile(hlsl,std::strlen(hlsl),"foliage-checksum",nullptr,nullptr,"main","cs_5_0",0,0,&code,&errors)),"compile independent checksum fixture");
    const auto* first=static_cast<const uint8_t*>(code->GetBufferPointer());
    std::vector<uint8_t> copy(first,first+code->GetBufferSize()),expected=copy;dxbc::Checksum(copy);
    Check(copy==expected,"DXBC checksum agrees with compiler");
    Check(!BuildShader(copy,{},out),"unrelated valid compute shaders stay untouched");
}
// Optional, local-only integration check. Accepts extracted .bin or raw DXBC;
// the game-owned fixture is never committed or required by ordinary CTest.
static void NativeFixture(const char* path){
    std::ifstream file(path,std::ios::binary);Check(bool(file),"open local shader fixture");
    std::vector<uint8_t> raw((std::istreambuf_iterator<char>(file)),{});
    if(raw.size()>240&&!std::memcmp(raw.data()+236,"DXBC",4))raw.erase(raw.begin(),raw.begin()+236);
    Check(Supported(raw),"fixture is an exact supported native or experimental shader");
    std::vector<uint8_t> stock;Check(BuildShader(raw,{},stock),"normalize earlier experiments back to native");
    Check(Hash(stock)==0x8a2dce3f6b00ecb1ull,"default output is byte-for-byte stock DXBC");
    ComPtr<ID3D11Device> dev;D3D_FEATURE_LEVEL level;
    Check(SUCCEEDED(D3D11CreateDevice(nullptr,D3D_DRIVER_TYPE_WARP,nullptr,0,nullptr,0,D3D11_SDK_VERSION,&dev,&level,nullptr)),"create independent WARP validation device");
    std::vector<Parameters> cases{{}};
    for(int axis=0;axis<8;++axis)for(float value:{0.f,.05f,.5f,2.f,10.f}){
        Parameters p;switch(axis){case 0:p.footprint=value;break;case 1:p.originX=value;break;case 2:p.originZ=value;break;
            case 3:p.spacing=value;break;case 4:p.camera=value;break;case 5:p.heightScale=value;break;case 6:p.heightX=value;break;case 7:p.heightZ=value;break;}
        cases.push_back(p);
    }
    cases.push_back({2,1,1,2,2,.5f,1.5f,1.5f}); // earlier centered experiment, expressed by live sliders
    cases.push_back({.5f,0,10,10,0,0,10,0}); // footprint gate, extreme offsets and zero sampling scale
    cases.push_back({0,10,0,0,10,10,0,10});
    for(const auto p:cases){
        std::vector<uint8_t> code;Check(BuildShader(raw,p,code),"transform native shader for slider case");
        ComPtr<ID3DBlob> disassembly;Check(SUCCEEDED(D3DDisassemble(code.data(),code.size(),0,nullptr,&disassembly)),"disassemble complete transformed shader");
        ComPtr<ID3D11ComputeShader> shader;
        const HRESULT hr=dev->CreateComputeShader(code.data(),code.size(),nullptr,&shader);
        if(FAILED(hr))std::fprintf(stderr,"WARP rejected footprint %.2f spacing %.2f origins %.2f/%.2f camera %.2f height %.2f/%.2f/%.2f: %08lx\n",p.footprint,p.spacing,p.originX,p.originZ,p.camera,p.heightScale,p.heightX,p.heightZ,static_cast<unsigned long>(hr));
        Check(SUCCEEDED(hr),"WARP accepts transformed bytecode, including boundary and zero cases");
        auto check=code;dxbc::Checksum(check);Check(check==code,"variant checksum verified");
        const std::string text(static_cast<const char*>(disassembly->GetBufferPointer()),disassembly->GetBufferSize());
        Check(text.find("dcl_thread_group 8, 8, 1")!=std::string::npos&&text.find("dcl_uav_structured u0, 160")!=std::string::npos,"native dispatch group and output stride retained");
        if(p.footprint<p.spacing)Check(text.find("if_z r17.x")!=std::string::npos,"independent footprint gate present");
    }
    std::printf("Validated %zu native variants from %s\n",cases.size(),path);
}
int main(int argc,char** argv){Basic();for(int i=1;i<argc;++i)NativeFixture(argv[i]);std::printf("%d foliage checks passed\n",checks);}
