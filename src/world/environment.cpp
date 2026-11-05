#include "kami/world/renderer.hpp"
#include <DirectXMath.h>
#include <algorithm>
#include <array>
#include <cstring>
namespace kami::world {
namespace {
constexpr UINT side=64U,mips=7U,irradiance_side=16U;
constexpr DXGI_FORMAT format=DXGI_FORMAT_R16G16B16A16_FLOAT;
D3D12_HEAP_PROPERTIES gpu_heap(D3D12_HEAP_TYPE type){D3D12_HEAP_PROPERTIES p{};p.Type=type;return p;}
D3D12_RESOURCE_BARRIER transition(ID3D12Resource* r,D3D12_RESOURCE_STATES from,D3D12_RESOURCE_STATES to){
    D3D12_RESOURCE_BARRIER b{};b.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    b.Transition.pResource=r;b.Transition.StateBefore=from;b.Transition.StateAfter=to;
    b.Transition.Subresource=D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;return b;
}
}
void Renderer::write_environment_probe(){
    if(!environment_data_)return;
    const auto& p=environment_capture_probe_;
    const std::array<std::array<float,4>,6> rows{{
        {p.center[0],p.center[1],p.center[2],environment_ready_?1.f:0.f},
        {p.half[0],p.half[1],p.half[2],p.ambiance},
        {p.box?1.f:0.f,p.clip,p.server?1.f:0.f,0.f},
        {p.axis[0][0],p.axis[0][1],p.axis[0][2],0.f},
        {p.axis[1][0],p.axis[1][1],p.axis[1][2],0.f},
        {p.axis[2][0],p.axis[2][1],p.axis[2][2],0.f}}};
    void* data=nullptr;const D3D12_RANGE no_read{0U,0U};
    if(SUCCEEDED(environment_data_->Map(0U,&no_read,&data))&&data){
        std::memcpy(data,rows.data(),sizeof(rows));environment_data_->Unmap(0U,nullptr);
    }
}
bool Renderer::make_environment(){
    auto* device=device_.native_device();if(!device||!shadow_srv_heap_)return false;
    D3D12_DESCRIPTOR_HEAP_DESC rtv{};rtv.Type=D3D12_DESCRIPTOR_HEAP_TYPE_RTV;rtv.NumDescriptors=55U;
    D3D12_DESCRIPTOR_HEAP_DESC dsv{};dsv.Type=D3D12_DESCRIPTOR_HEAP_TYPE_DSV;dsv.NumDescriptors=1U;
    if(FAILED(device->CreateDescriptorHeap(&rtv,IID_PPV_ARGS(environment_rtv_.GetAddressOf())))||
       FAILED(device->CreateDescriptorHeap(&dsv,IID_PPV_ARGS(environment_dsv_.GetAddressOf()))))return false;
    const auto heap=gpu_heap(D3D12_HEAP_TYPE_DEFAULT);
    const auto make=[&](Microsoft::WRL::ComPtr<ID3D12Resource>& resource,UINT size,UINT faces,UINT levels){
        D3D12_RESOURCE_DESC desc{};desc.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;
        desc.Width=size;desc.Height=size;desc.DepthOrArraySize=static_cast<UINT16>(faces);
        desc.MipLevels=static_cast<UINT16>(levels);desc.Format=format;desc.SampleDesc.Count=1U;
        desc.Flags=D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;
        D3D12_CLEAR_VALUE clear{};clear.Format=format;
        return SUCCEEDED(device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&desc,
            D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,&clear,IID_PPV_ARGS(resource.GetAddressOf())));
    };
    if(!make(environment_raw_,side,6U,1U)||!make(environment_filtered_,side,6U,mips)||
       !make(environment_diffuse_,irradiance_side,6U,1U)||!make(environment_brdf_,side,1U,1U))return false;
    const auto descriptor_step=device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    auto rt=environment_rtv_->GetCPUDescriptorHandleForHeapStart();
    const auto views=[&](ID3D12Resource* resource,UINT levels,UINT faces){
        for(UINT mip=0U;mip<levels;++mip)for(UINT face=0U;face<faces;++face){
            D3D12_RENDER_TARGET_VIEW_DESC desc{};desc.Format=format;
            desc.ViewDimension=D3D12_RTV_DIMENSION_TEXTURE2DARRAY;
            desc.Texture2DArray.MipSlice=mip;desc.Texture2DArray.FirstArraySlice=face;
            desc.Texture2DArray.ArraySize=1U;device->CreateRenderTargetView(resource,&desc,rt);rt.ptr+=descriptor_step;
        }
    };
    views(environment_raw_.Get(),1U,6U);views(environment_filtered_.Get(),mips,6U);views(environment_diffuse_.Get(),1U,6U);
    D3D12_RENDER_TARGET_VIEW_DESC lut{};lut.Format=format;lut.ViewDimension=D3D12_RTV_DIMENSION_TEXTURE2D;
    device->CreateRenderTargetView(environment_brdf_.Get(),&lut,rt);
    D3D12_RESOURCE_DESC depth{};depth.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    depth.Width=side;depth.Height=side;depth.DepthOrArraySize=1U;depth.MipLevels=1U;
    depth.Format=DXGI_FORMAT_D32_FLOAT;depth.SampleDesc.Count=1U;depth.Flags=D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;
    D3D12_CLEAR_VALUE clear{};clear.Format=depth.Format;clear.DepthStencil.Depth=1.f;
    if(FAILED(device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&depth,
        D3D12_RESOURCE_STATE_DEPTH_WRITE,&clear,IID_PPV_ARGS(environment_depth_.GetAddressOf()))))return false;
    device->CreateDepthStencilView(environment_depth_.Get(),nullptr,environment_dsv_->GetCPUDescriptorHandleForHeapStart());
    auto srv=shadow_srv_heap_->GetCPUDescriptorHandleForHeapStart();
    const auto srv_step=device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);srv.ptr+=5U*srv_step;
    const auto cube_srv=[&](ID3D12Resource* resource,UINT levels){
        D3D12_SHADER_RESOURCE_VIEW_DESC desc{};desc.Format=format;desc.ViewDimension=D3D12_SRV_DIMENSION_TEXTURECUBE;
        desc.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;desc.TextureCube.MipLevels=levels;
        device->CreateShaderResourceView(resource,&desc,srv);srv.ptr+=srv_step;
    };
    cube_srv(environment_raw_.Get(),1U);cube_srv(environment_filtered_.Get(),mips);cube_srv(environment_diffuse_.Get(),1U);
    D3D12_SHADER_RESOURCE_VIEW_DESC brdf{};brdf.Format=format;brdf.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;
    brdf.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;brdf.Texture2D.MipLevels=1U;
    device->CreateShaderResourceView(environment_brdf_.Get(),&brdf,srv);srv.ptr+=srv_step;
    D3D12_RESOURCE_DESC data{};data.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;data.Width=6U*16U;
    data.Height=1U;data.DepthOrArraySize=1U;data.MipLevels=1U;data.SampleDesc.Count=1U;data.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
    const auto upload=gpu_heap(D3D12_HEAP_TYPE_UPLOAD);
    if(FAILED(device->CreateCommittedResource(&upload,D3D12_HEAP_FLAG_NONE,&data,
        D3D12_RESOURCE_STATE_GENERIC_READ,nullptr,IID_PPV_ARGS(environment_data_.GetAddressOf()))))return false;
    D3D12_SHADER_RESOURCE_VIEW_DESC buffer{};buffer.Format=DXGI_FORMAT_UNKNOWN;buffer.ViewDimension=D3D12_SRV_DIMENSION_BUFFER;
    buffer.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;buffer.Buffer.NumElements=6U;
    buffer.Buffer.StructureByteStride=16U;device->CreateShaderResourceView(environment_data_.Get(),&buffer,srv);
    environment_ready_=false;environment_face_=0U;environment_updates_=0U;environment_dirty_=true;write_environment_probe();return true;
}
bool Renderer::record_environment(const std::array<float,63>& original){
    if(environment_frozen_||!graphics_.pbr||(!environment_test_&&(!region_mesh_||preview_mesh_)))return true;
    if(!environment_raw_||!environment_capture_pipeline_||!environment_filter_pipeline_)return false;
    using namespace DirectX;
    const auto now=std::chrono::steady_clock::now();
    if(environment_face_==0U){
        if(environment_ready_&&!environment_dirty_&&now-environment_last_<std::chrono::seconds(2))return true;
        if(!environment_test_)environment_capture_probe_=select_environment_probe(environment_server_probes_,live_region_origin_,
            {original[20],original[21],original[22]});
    }
    static constexpr std::array<std::array<float,3>,6> directions{{{1,0,0},{-1,0,0},{0,1,0},{0,-1,0},{0,0,1},{0,0,-1}}};
    static constexpr std::array<std::array<float,3>,6> ups{{{0,1,0},{0,1,0},{0,0,-1},{0,0,1},{0,1,0},{0,1,0}}};
    const auto& p=environment_capture_probe_;const auto& d=directions[environment_face_];const auto& up=ups[environment_face_];
    const auto eye=XMVectorSet(p.center[0],p.center[1],p.center[2],1.f);
    const auto view=XMMatrixLookAtLH(eye,XMVectorAdd(eye,XMVectorSet(d[0],d[1],d[2],0)),XMVectorSet(up[0],up[1],up[2],0));
    const auto projection=XMMatrixPerspectiveFovLH(XM_PIDIV2,1.f,std::max(.01f,p.clip),std::max(512.f,p.clip+1024.f));
    XMFLOAT4X4 vp{};XMStoreFloat4x4(&vp,view*projection);
    auto constants=original;std::memcpy(constants.data(),&vp,sizeof(vp));
    constants[20]=p.center[0];constants[21]=p.center[1];constants[22]=p.center[2];
    constants[25]=0.f;constants[52]=float(side);constants[53]=float(side);constants[54]=-1.f;constants[55]=0.f;
    for(unsigned i=56U;i<63U;++i)constants[i]=0.f;
    auto to_rt=transition(environment_raw_.Get(),D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_RENDER_TARGET);
    commands_->ResourceBarrier(1U,&to_rt);
    const auto rtv_step=device_.native_device()->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
    auto target=environment_rtv_->GetCPUDescriptorHandleForHeapStart();target.ptr+=environment_face_*rtv_step;
    const auto depth=environment_dsv_->GetCPUDescriptorHandleForHeapStart();
    const D3D12_VIEWPORT viewport{0,0,float(side),float(side),0,1};const D3D12_RECT rect{0,0,LONG(side),LONG(side)};
    commands_->RSSetViewports(1U,&viewport);commands_->RSSetScissorRects(1U,&rect);
    constexpr float black[]{0,0,0,0};commands_->ClearRenderTargetView(target,black,0,nullptr);
    commands_->ClearDepthStencilView(depth,D3D12_CLEAR_FLAG_DEPTH,1.f,0,0,nullptr);
    commands_->OMSetRenderTargets(1U,&target,FALSE,nullptr);commands_->SetPipelineState(environment_sky_pipeline_.Get());
    commands_->SetGraphicsRoot32BitConstants(0,63,constants.data(),0);commands_->DrawInstanced(3U,1U,0U,0U);
    commands_->OMSetRenderTargets(1U,&target,FALSE,&depth);commands_->SetPipelineState(environment_capture_pipeline_.Get());
    commands_->DrawInstanced(static_cast<UINT>(terrain_count_),1U,0U,0U);
    auto to_srv=transition(environment_raw_.Get(),D3D12_RESOURCE_STATE_RENDER_TARGET,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    commands_->ResourceBarrier(1U,&to_srv);
    if(++environment_face_<6U)return true;
    environment_face_=0U;
    const auto render_filter=[&](ID3D12Resource* resource,UINT begin,UINT levels,UINT size,UINT faces,UINT mode){
        auto a=transition(resource,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_RENDER_TARGET);commands_->ResourceBarrier(1U,&a);
        for(UINT mip=0U;mip<levels;++mip){
            const UINT extent=std::max(1U,size>>mip);
            const D3D12_VIEWPORT v{0,0,float(extent),float(extent),0,1};const D3D12_RECT r{0,0,LONG(extent),LONG(extent)};
            commands_->RSSetViewports(1U,&v);commands_->RSSetScissorRects(1U,&r);
            for(UINT face=0U;face<faces;++face){
                auto out=environment_rtv_->GetCPUDescriptorHandleForHeapStart();out.ptr+=(begin+mip*faces+face)*rtv_step;
                commands_->OMSetRenderTargets(1U,&out,FALSE,nullptr);commands_->SetPipelineState(environment_filter_pipeline_.Get());
                constants[48]=float(mode);constants[49]=levels>1U?float(mip)/float(levels-1U):0.f;constants[50]=float(face);
                commands_->SetGraphicsRoot32BitConstants(0,63,constants.data(),0);commands_->DrawInstanced(3U,1U,0U,0U);
            }
        }
        auto b=transition(resource,D3D12_RESOURCE_STATE_RENDER_TARGET,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);commands_->ResourceBarrier(1U,&b);
    };
    render_filter(environment_filtered_.Get(),6U,mips,side,6U,0U);
    render_filter(environment_diffuse_.Get(),48U,1U,irradiance_side,6U,1U);
    if(environment_updates_==0U)render_filter(environment_brdf_.Get(),54U,1U,side,1U,2U);
    environment_ready_=true;++environment_updates_;environment_dirty_=false;environment_last_=now;write_environment_probe();return true;
}
}
