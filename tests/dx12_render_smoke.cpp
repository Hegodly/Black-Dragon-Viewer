#include "kami/world/renderer.hpp"
#include "kami/world/live_primitive_geometry.hpp"
#include "kami/protocol/bootstrap.hpp"
#include "kami/features/creator_studio.hpp"
#include "kami/graphics/settings.hpp"
#include <windows.h>
#include <iostream>
#include <sstream>
#include <cmath>
#include <algorithm>
#include <string>
#include <string_view>
#include <filesystem>
#include <objbase.h>
#include <wincodec.h>


// Test-only access: exercise the production atlas uploader and scene shader
// with synthetic pixels. No account, bearer token or network is involved.
namespace kami::world {
struct RendererPixelTestAccess {
    static bool pbr_images(Renderer& r,const std::map<std::string,protocol::texture::DecodedTexture>& images,
                           const std::map<std::string,std::uint16_t>& layers){
        r.wait_for_gpu();const auto previous=r.live_texture_layers_;
        r.live_texture_layers_=layers;
        return r.make_material_texture(&images,&previous)==TextureUploadStatus::Committed;
    }
    static bool pbr_material(Renderer& r,const protocol::pbr::Material& material){
        r.wait_for_gpu();
        protocol::objectspatial::Transform t;t.local_id=1U;t.face_material_ids[0]="material";
        const auto table=build_pbr_table({{1U,t}},{},{{"material",material}},{},r.live_texture_layers_);
        r.pbr_rows_=table.rows;return r.make_material_records(r.pbr_rows_);
    }
    static void environment(Renderer& r,bool capture,bool frozen){
        r.wait_for_gpu();r.environment_test_=capture;r.environment_frozen_=frozen;
        if(capture&&!frozen){r.environment_ready_=false;r.environment_updates_=0U;r.environment_face_=0U;
            r.environment_capture_probe_=EnvironmentProbe{};r.write_environment_probe();}
    }
    static bool terrain(Renderer& r,float start){
        r.wait_for_gpu();protocol::pbr::Material red,green;red.unlit=green.unlit=true;
        red.base={1,0,0,1};green.base={0,1,0,1};
        protocol::objectspatial::Transform t;t.face_material_ids[0]="red";t.face_material_ids[1]="green";
        auto table=build_pbr_table({{1U,t}}, {}, {{"red",red},{"green",green}}, {}, {});
        std::array<PBRRow,protocol::pbr::kRows> marker{};
        marker[0]={1,1,2,2};marker[2]={start,start,start,start};marker[3]={1,1,1,1};marker[4]={-1,0,0,0};
        table.rows.insert(table.rows.end(),marker.begin(),marker.end());
        r.pbr_rows_=table.rows;return r.make_material_records(r.pbr_rows_);
    }
    static bool blend_active(const Renderer& r){return !r.transparent_vertices_.empty();}
    static bool shadow_writes(Renderer& r,std::size_t& writes) {
        r.wait_for_gpu();
        auto* device=r.device_.native_device();
        const auto desc=r.shadow_map_->GetDesc();
        D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
        UINT rows=0U;UINT64 row_bytes=0U,bytes=0U;
        device->GetCopyableFootprints(&desc,0U,1U,0U,&footprint,&rows,&row_bytes,&bytes);
        D3D12_HEAP_PROPERTIES heap{};heap.Type=D3D12_HEAP_TYPE_READBACK;
        D3D12_RESOURCE_DESC buffer{};
        buffer.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;buffer.Width=bytes;
        buffer.Height=1U;buffer.DepthOrArraySize=1U;buffer.MipLevels=1U;
        buffer.SampleDesc.Count=1U;buffer.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        Microsoft::WRL::ComPtr<ID3D12Resource> readback;
        if(!bytes||bytes>16U*1024U*1024U||row_bytes!=desc.Width*4U||
           FAILED(device->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,
            &buffer,D3D12_RESOURCE_STATE_COPY_DEST,nullptr,
            IID_PPV_ARGS(readback.GetAddressOf())))||
           FAILED(r.texture_copy_allocator_->Reset())||
           FAILED(r.texture_copy_commands_->Reset(r.texture_copy_allocator_.Get(),nullptr)))
            return false;
        auto* cmd=r.texture_copy_commands_.Get();
        D3D12_RESOURCE_BARRIER barrier{};barrier.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        barrier.Transition.pResource=r.shadow_map_.Get();
        barrier.Transition.Subresource=D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        barrier.Transition.StateBefore=D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
        barrier.Transition.StateAfter=D3D12_RESOURCE_STATE_COPY_SOURCE;
        cmd->ResourceBarrier(1U,&barrier);
        D3D12_TEXTURE_COPY_LOCATION from{},to{};
        from.pResource=r.shadow_map_.Get();from.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        to.pResource=readback.Get();to.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        to.PlacedFootprint=footprint;
        cmd->CopyTextureRegion(&to,0U,0U,0U,&from,nullptr);
        std::swap(barrier.Transition.StateBefore,barrier.Transition.StateAfter);
        cmd->ResourceBarrier(1U,&barrier);
        if(FAILED(cmd->Close()))return false;
        ID3D12CommandList* lists[]={cmd};r.device_.graphics_queue()->ExecuteCommandLists(1U,lists);
        r.wait_for_gpu();
        void* data=nullptr;const D3D12_RANGE range{0U,static_cast<SIZE_T>(bytes)};
        if(FAILED(readback->Map(0U,&range,&data))||!data)return false;
        writes=0U;
        for(UINT y=0U;y<rows;++y){
            const auto* row=reinterpret_cast<const float*>(
                static_cast<const std::uint8_t*>(data)+footprint.Offset+
                std::size_t(y)*footprint.Footprint.RowPitch);
            for(UINT x=0U;x<desc.Width;++x)if(row[x]<0.999999f)++writes;
        }
        const D3D12_RANGE no_write{0U,0U};readback->Unmap(0U,&no_write);return true;
    }
    static bool upload(Renderer& r,const std::string& id,std::uint8_t alpha) {
        r.wait_for_gpu();
        const auto previous=r.live_texture_layers_;
        r.live_texture_layers_={{id,protocol::texture::kGPUImageLayers}};
        protocol::texture::DecodedTexture image{};
        image.width=image.height=1U;image.rgba={255U,0U,255U,alpha};
        const std::map<std::string,protocol::texture::DecodedTexture> images{{id,image}};
        return r.make_material_texture(&images,&previous)==TextureUploadStatus::Committed;
    }
};
}
namespace {
bool count_magenta(const std::wstring& file,std::size_t& result,
    std::uint64_t* hash=nullptr,std::array<std::uint64_t,3>* sums=nullptr) {
    Microsoft::WRL::ComPtr<IWICImagingFactory> factory;
    Microsoft::WRL::ComPtr<IWICBitmapDecoder> decoder;
    Microsoft::WRL::ComPtr<IWICBitmapFrameDecode> frame;
    Microsoft::WRL::ComPtr<IWICFormatConverter> converter;
    if(FAILED(CoCreateInstance(CLSID_WICImagingFactory,nullptr,
        CLSCTX_INPROC_SERVER,IID_PPV_ARGS(factory.GetAddressOf())))||
       FAILED(factory->CreateDecoderFromFilename(file.c_str(),nullptr,
        GENERIC_READ,WICDecodeMetadataCacheOnLoad,decoder.GetAddressOf()))||
       FAILED(decoder->GetFrame(0U,frame.GetAddressOf()))||
       FAILED(factory->CreateFormatConverter(converter.GetAddressOf()))||
       FAILED(converter->Initialize(frame.Get(),GUID_WICPixelFormat32bppRGBA,
        WICBitmapDitherTypeNone,nullptr,0.,WICBitmapPaletteTypeCustom)))return false;
    UINT w=0U,h=0U;
    if(FAILED(converter->GetSize(&w,&h))||!w||!h||w>4096U||h>4096U)return false;
    std::vector<std::uint8_t> rgba(std::size_t(w)*h*4U);
    if(FAILED(converter->CopyPixels(nullptr,w*4U,
        static_cast<UINT>(rgba.size()),rgba.data())))return false;
    result=0U;if(hash)*hash=1469598103934665603ULL;if(sums)*sums={};
    for(std::size_t at=0U;at<rgba.size();at+=4U){
        for(unsigned channel=0U;channel<3U;++channel){
            if(hash)*hash=(*hash^rgba[at+channel])*1099511628211ULL;
            if(sums)(*sums)[channel]+=rgba[at+channel];
        }
        if(rgba[at]>40U&&rgba[at+2U]>40U&&
           unsigned(rgba[at])>unsigned(rgba[at+1U])+40U&&
           unsigned(rgba[at+2U])>unsigned(rgba[at+1U])+40U)++result;
    }
    return true;
}

bool check_pbr_pixels(kami::world::Renderer& renderer) {
    using namespace kami;
    graphics::Settings graphics;graphics.pbr=true;graphics.shadows=false;
    graphics.animate_avatar=false;graphics.vsync=false;graphics.fps_limit=0U;
    graphics.contact_ambient=false;graphics.diamond_glints=false;graphics.atmosphere_haze=false;
    renderer.set_graphics(graphics);
    camera::Settings camera;camera.head_tracking=false;camera.smoothing=0U;camera.collision=false;
    renderer.set_camera_settings(camera);
    std::array<std::string,8> ids;
    for(unsigned i=0U;i<ids.size();++i)ids[i]="12345678-90ab-4cde-8fab-00000000000"+std::to_string(i+1U);
    const auto image=[](std::array<std::uint8_t,4> pixel){
        protocol::texture::DecodedTexture t;t.width=t.height=1U;
        t.rgba.assign(pixel.begin(),pixel.end());return t;
    };
    std::map<std::string,protocol::texture::DecodedTexture> images{
        {ids[0],image({255U,0U,255U,0U})},{ids[1],image({255U,0U,255U,255U})},
        {ids[2],image({255U,64U,0U,255U})},{ids[3],image({255U,128U,128U,255U})},
        {ids[4],image({0U,255U,0U,255U})},{ids[5],image({0U,255U,255U,255U})},
        {ids[6],image({128U,128U,128U,255U})}};
    protocol::texture::DecodedTexture pattern;pattern.width=pattern.height=2U;
    pattern.rgba={255,0,0,255, 0,255,0,255, 0,0,255,255, 255,255,255,255};
    images.emplace(ids[7],pattern);
    std::map<std::string,std::uint16_t> layers;
    for(unsigned i=0U;i<ids.size();++i)layers.emplace(ids[i],static_cast<std::uint16_t>(i+1U));
    if(!world::RendererPixelTestAccess::pbr_images(renderer,images,layers)){std::wcerr<<L"PBR pixel check failed at line "<<__LINE__<<L"\n";return false;}
    auto scene=world::build_test_scene(8);scene.terrain_count=scene.vertices.size();scene.avatar_count=0U;
    for(auto& v:scene.vertices){
        v.pbr_material=1U;v.texture_layer=0.f;v.r=0.f;v.g=1.f;v.b=0.f;
        v.bone_weights=0U;v.body_region=0.f;v.metallic=0.f;
    }
    wchar_t folder[MAX_PATH]{};if(!GetTempPathW(MAX_PATH,folder)){std::wcerr<<L"PBR pixel check failed at line "<<__LINE__<<L"\n";return false;}
    const std::wstring target=std::wstring(folder)+L"KAMI-PBR-Pixel-Test.png";
    struct Result {std::size_t magenta=0U;std::uint64_t hash=0U;std::array<std::uint64_t,3> sum{};};
    const auto render=[&](const protocol::pbr::Material& material,Result& out){
        if(!world::RendererPixelTestAccess::pbr_material(renderer,material)||
           !renderer.set_creator_preview(scene)||!renderer.capture_png(target)){std::wcerr<<L"PBR pixel check failed at line "<<__LINE__<<L"\n";return false;}
        renderer.draw();
        const bool ok=renderer.ready()&&renderer.capture_status()==world::CaptureStatus::Saved&&
            count_magenta(target,out.magenta,&out.hash,&out.sum);
        std::error_code ignored;std::filesystem::remove(target,ignored);return ok;
    };
    // Decode the same standard LLSD envelope as a ViewerAsset response,
    // then exercise the actual table upload and production pixel shader.
    const std::string header="<? LLSD/Binary ?>\n";
    std::vector<std::uint8_t> asset(header.begin(),header.end());asset.push_back('{');
    const auto size=[&](std::size_t n){for(int shift=24;shift>=0;shift-=8)asset.push_back(static_cast<std::uint8_t>(n>>shift));};
    const auto string=[&](const std::string& value){size(value.size());asset.insert(asset.end(),value.begin(),value.end());};
    const std::string json=R"({"asset":{"version":"2.0"},"images":[{"uri":")"+ids[0]+R"("}],"textures":[{"source":0}],"materials":[{"doubleSided":true,"extensions":{"KHR_materials_unlit":{}},"pbrMetallicRoughness":{"baseColorTexture":{"index":0}}}]})";
    size(3U);
    for(const auto& [key,value]:std::map<std::string,std::string>{{"version","1.1"},{"type","GLTF 2.0"},{"data",json}}){
        asset.push_back('k');string(key);asset.push_back('s');string(value);
    }asset.push_back('}');
    protocol::pbr::Material m;protocol::pbr::DecodeError error;
    if(!protocol::pbr::decode_asset(asset,m,&error)||error!=protocol::pbr::DecodeError::None)return false;
    Result opaque,mask,full,half,empty;
    // OPAQUE must ignore image alpha. A green legacy tint must not replace
    // the PBR material's magenta base map.
    if(!render(m,opaque)||opaque.magenta<64U){std::wcerr<<L"PBR pixel check failed at line "<<__LINE__<<L"\n";return false;}
    std::wcout<<L"KAMI STANDARD LLSD MATERIAL PIXELS VERIFIED: ViewerAsset envelope to DX12 base map\n";
    m.alpha=1U;m.cutoff=.5f;
    if(!render(m,mask)||mask.magenta!=0U){std::wcerr<<L"PBR pixel check failed at line "<<__LINE__<<L"\n";return false;}
    m.images[0]=ids[1];m.base[3]=.5f;m.cutoff=.75f;
    if(!render(m,mask)||mask.magenta!=0U){std::wcerr<<L"PBR pixel check failed at line "<<__LINE__<<L"\n";return false;}
    m.cutoff=.25f;if(!render(m,full)||full.magenta<64U){std::wcerr<<L"PBR pixel check failed at line "<<__LINE__<<L"\n";return false;}
    m.alpha=2U;m.base[3]=0.f;
    if(!render(m,empty)||empty.magenta!=0U||!world::RendererPixelTestAccess::blend_active(renderer)){std::wcerr<<L"PBR pixel check failed at line "<<__LINE__<<L"\n";return false;}
    m.base[3]=1.f;if(!render(m,full)){std::wcerr<<L"PBR pixel check failed at line "<<__LINE__<<L"\n";return false;}
    m.base[3]=.5f;if(!render(m,half)){std::wcerr<<L"PBR pixel check failed at line "<<__LINE__<<L"\n";return false;}
    if(!(empty.sum[0]<half.sum[0]&&half.sum[0]<full.sum[0])){std::wcerr<<L"PBR pixel check failed at line "<<__LINE__<<L"\n";return false;}
    m.alpha=0U;m.base[3]=1.f;m.images[0]=ids[7];
    Result uv0,uv1;if(!render(m,uv0)){std::wcerr<<L"PBR pixel check failed at line "<<__LINE__<<L"\n";return false;}
    m.uv[0].scale={2.f,3.f};m.uv[0].offset={.23f,.41f};m.uv[0].rotation=.3f;
    if(!render(m,uv1)||uv0.hash==uv1.hash){std::wcerr<<L"PBR pixel check failed at line "<<__LINE__<<L"\n";return false;}
    // Repeat the baseline to prove animation/camera drift cannot make a
    // missing map appear to pass these pixel-difference checks.
    m=protocol::pbr::Material{};m.double_sided=true;m.base={.8f,.5f,.3f,1.f};
    m.metallic=.8f;m.roughness=.5f;
    Result baseline,repeated,orm,normal,ao,emission,off;
    if(!render(m,baseline)||!render(m,repeated)||baseline.hash!=repeated.hash){std::wcerr<<L"PBR pixel check failed at line "<<__LINE__<<L"\n";return false;}
    m.images[2]=ids[2];if(!render(m,orm)||orm.hash==baseline.hash){std::wcerr<<L"PBR pixel check failed at line "<<__LINE__<<L"\n";return false;}
    m.images[2].clear();m.images[1]=ids[3];
    if(!render(m,normal)||normal.hash==baseline.hash){std::wcerr<<L"PBR pixel check failed at line "<<__LINE__<<L"\n";return false;}
    m.images[1].clear();m.images[4]=ids[5];
    if(!render(m,ao)||ao.hash==baseline.hash){std::wcerr<<L"PBR pixel check failed at line "<<__LINE__<<L"\n";return false;}
    m.images[4].clear();m.emissive={0.f,1.f,0.f};m.images[3]=ids[4];
    if(!render(m,emission)||emission.sum[1]<=baseline.sum[1]){std::wcerr<<L"PBR pixel check failed at line "<<__LINE__<<L"\n";return false;}
    graphics.pbr=false;renderer.set_graphics(graphics);
    if(!render(m,off)||off.hash==emission.hash){std::wcerr<<L"PBR pixel check failed at line "<<__LINE__<<L"\n";return false;}
    renderer.clear_creator_preview();
    std::wcout<<L"KAMI PBR PIXELS VERIFIED: base color, transforms, normal, ORM, AO, emissive, OPAQUE/MASK/BLEND and PBR toggle\n";
    return true;
}

bool check_environment_pixels(kami::world::Renderer& renderer){
    using namespace kami;using Access=world::RendererPixelTestAccess;
    graphics::Settings graphics;graphics.pbr=true;graphics.shadows=false;graphics.animate_avatar=false;
    graphics.vsync=false;graphics.fps_limit=0;graphics.contact_ambient=false;graphics.diamond_glints=false;
    renderer.set_graphics(graphics);
    protocol::objectspatial::Transform enclosure;enclosure.local_id=1;enclosure.x=enclosure.y=enclosure.z=0;
    enclosure.sx=enclosure.sy=enclosure.sz=40;enclosure.primitive=protocol::objectspatial::PrimitiveKind::Box;
    auto room=world::build_live_primitive_geometry({{1U,enclosure}}, {},0,0,0).scene;
    for(auto& v:room.vertices)v.pbr_material=1;
    protocol::pbr::Material green;green.base={0,1,0,1};green.unlit=true;green.double_sided=true;
    if(!Access::pbr_material(renderer,green)||!renderer.set_creator_preview(room))return false;
    Access::environment(renderer,true,false);
    for(unsigned i=0;i<6;++i)renderer.draw();
    if(!renderer.ready()||!renderer.environment_lighting_ready()||renderer.environment_updates()!=1U)return false;
    Access::environment(renderer,true,true);
    auto scene=world::build_test_scene(8);scene.terrain_count=scene.vertices.size();scene.avatar_count=0;
    for(auto& v:scene.vertices){v.pbr_material=1;v.bone_weights=0;v.body_region=0;}
    protocol::pbr::Material metal;metal.metallic=1;metal.roughness=.08f;metal.double_sided=true;
    if(!Access::pbr_material(renderer,metal)||!renderer.set_creator_preview(scene))return false;
    wchar_t folder[MAX_PATH]{};if(!GetTempPathW(MAX_PATH,folder))return false;
    const std::wstring target=std::wstring(folder)+L"KAMI-IBL-Terrain-Test.png";
    const auto capture=[&](std::array<std::uint64_t,3>& sum){
        std::size_t count=0;if(!renderer.capture_png(target))return false;renderer.draw();
        const bool ok=renderer.ready()&&renderer.capture_status()==world::CaptureStatus::Saved&&count_magenta(target,count,nullptr,&sum);
        std::error_code ignored;std::filesystem::remove(target,ignored);return ok;
    };
    std::array<std::uint64_t,3> reflected{},disabled{},red{},land_green{};
    if(!capture(reflected))return false;
    graphics.pbr=false;renderer.set_graphics(graphics);if(!capture(disabled))return false;
    std::wcout<<L"IBL channels on="<<reflected[0]<<L","<<reflected[1]<<L","<<reflected[2]
        <<L" off="<<disabled[0]<<L","<<disabled[1]<<L","<<disabled[2]<<L"\n";
    if(reflected[1]<=reflected[0]+10000U||reflected[1]<=disabled[1])return false;
    graphics.pbr=true;renderer.set_graphics(graphics);
    for(auto& v:scene.vertices)v.pbr_material=3;
    if(!Access::terrain(renderer,10000)||!renderer.set_creator_preview(scene)||!capture(red))return false;
    if(!Access::terrain(renderer,-10000)||!capture(land_green))return false;
    if(red[0]<=red[1]+10000U||land_green[1]<=land_green[0]+10000U)return false;
    Access::environment(renderer,false,true);renderer.clear_creator_preview();
    std::wcout<<L"KAMI PBR ENVIRONMENT TERRAIN PIXELS VERIFIED: captured geometry, GGX IBL and height-selected terrain materials\n";
    return true;
}

bool check_sampled_pixels(kami::world::Renderer& renderer) {
    auto graphics=kami::graphics::Settings{};
    graphics.pbr=false;graphics.shadows=true;graphics.animate_avatar=false;
    renderer.set_graphics(graphics);renderer.reset_camera();
    auto scene=kami::world::build_test_scene(8);
    for(auto& v:scene.vertices){
        v.r=v.g=v.b=1.f;v.metallic=0.f;
        v.bone_weights=0U;v.body_region=0.f;
        v.texture_layer=float(kami::protocol::texture::kGPUImageLayers);
    }
    if(!renderer.set_creator_preview(scene))return false;
    wchar_t folder[MAX_PATH]{};
    if(!GetTempPathW(MAX_PATH,folder))return false;
    const std::wstring target=std::wstring(folder)+L"KAMI-Texture-Pixel-Test.png";
    const auto capture=[&](std::size_t& count){
        if(!renderer.capture_png(target))return false;
        renderer.draw();
        const bool ok=renderer.ready()&&
            renderer.capture_status()==kami::world::CaptureStatus::Saved&&
            count_magenta(target,count);
        std::error_code ignored;std::filesystem::remove(target,ignored);
        return ok;
    };
    std::size_t opaque=0U,transparent=0U;
    if(!kami::world::RendererPixelTestAccess::upload(renderer,"opaque",255U)||
       !capture(opaque)||opaque<64U){
        std::wcerr<<L"Opaque texture color pixel check failed: "<<opaque<<L"\n";
        return false;
    }
    std::size_t opaque_depth=0U;
    if(!kami::world::RendererPixelTestAccess::shadow_writes(renderer,opaque_depth)||
       opaque_depth==0U){std::wcerr<<L"Opaque shadow pixel check failed\n";return false;}
    if(!kami::world::RendererPixelTestAccess::upload(renderer,"transparent",0U)||
       !capture(transparent)||transparent!=0U){
        std::wcerr<<L"Transparent texels incorrectly remain visible: "<<transparent<<L"\n";
        return false;
    }
    std::size_t transparent_depth=0U;
    if(!kami::world::RendererPixelTestAccess::shadow_writes(renderer,transparent_depth)||
       transparent_depth!=0U){std::wcerr<<L"Invisible texels cast solid shadows\n";return false;}
    renderer.clear_creator_preview();
    std::wcout<<L"KAMI TEXTURE PIXELS VERIFIED: "<<opaque
              <<L" opaque color pixels, "<<transparent<<L" invisible pixels; "
              <<opaque_depth<<L" opaque shadow pixels, "<<transparent_depth
              <<L" invisible shadow pixels\n";
    return true;
}
}

namespace {
LRESULT CALLBACK smoke_window(HWND window,UINT message,WPARAM wp,LPARAM lp) {
    return DefWindowProcW(window,message,wp,lp);
}
void print_failure(const wchar_t* step,const kami::world::Renderer& renderer) {
    std::wcerr<<L"KAMI DX12 smoke FAILED at "<<step
              <<L": "<<renderer.error()<<L"\n";
}
}
int main(int argc,char** argv) {
    // The --force-warp mode must prove software rasterization, not merely
    // a DX12 device which happens to be available on the CI machine.
    bool force_warp=false;
    for(int i=1;i<argc;++i){
        if(std::string_view(argv[i])=="--force-warp")force_warp=true;
        else{std::wcerr<<L"Unrecognized DX12 smoke option\n";return 31;}
    }
    auto instance=GetModuleHandleW(nullptr);
    WNDCLASSEXW desc{};
    desc.cbSize=sizeof(desc);
    desc.hInstance=instance;
    desc.lpfnWndProc=smoke_window;
    desc.lpszClassName=L"KAMI_DX12_Render_Smoke";
    if(!RegisterClassExW(&desc)) {
        std::wcerr<<L"NO WINDOWING: cannot register a test window\n";
        return 77;
    }
    HWND window=CreateWindowExW(0,desc.lpszClassName,L"KAMI DX12 Renderer Smoke",
        WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,680,520,
        nullptr,nullptr,instance,nullptr);
    if(!window) {
        std::wcerr<<L"NO WINDOWING: test window creation unavailable\n";
        return 77;
    }
    ShowWindow(window,SW_SHOWNOACTIVATE);
    // WIC screenshot encoding relies on a COM apartment, just like the GUI app.
    const HRESULT co_ready=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    if(FAILED(co_ready)) {
        DestroyWindow(window);
        UnregisterClassW(desc.lpszClassName,instance);
        std::wcerr<<L"Could not initialize COM for DX12 photo smoke test\n";
        return 1;
    }
    int result=0;
    {
        kami::world::Renderer renderer;
        if(!renderer.initialize(window,640,480,true,force_warp)) {
            print_failure(L"initialize",renderer);
            // No hardware/display in an automated server VM can prevent
            // an HWND swap chain. Report SKIPPED, not a false GPU pass.
            if(force_warp)result=30; // no silent skip for requested WARP
            else if(renderer.error().find(L"No DirectX 12 device")!=std::wstring::npos ||
                    renderer.error().find(L"swap chain")!=std::wstring::npos)
                result=77;
            else result=1;
        } else {
            if(force_warp&&!renderer.software_dx12_active()){
                print_failure(L"explicit WARP selection",renderer);
                result=29;
            }else if(renderer.vertex_count()==0) {
                print_failure(L"empty scene",renderer);
                result=2;
            } else if(renderer.skin_compatibility().status!=
                      kami::rig::Compatibility::PhysicsReady) {
                print_failure(L"weighted fitted-mesh skin integrity",renderer);
                result=19;
            } else {
                renderer.draw();
                if(!renderer.ready()) {
                    print_failure(L"initial draw",renderer);
                    result=3;
                }
                kami::graphics::Settings quality{};
                quality.preset=kami::graphics::Preset::Quality;
                quality.render_scale_percent=75;
                quality.vsync=false;
                quality.pbr=true;
                quality.shadows=true;
                quality.animate_avatar=true;
                renderer.set_graphics(quality);
                if(renderer.ready()) renderer.draw();
                if(!renderer.ready()) {
                    print_failure(L"PBR and shadow preset",renderer);
                    result=4;
                }
                quality.shadows=false;
                quality.pbr=false;
                renderer.set_graphics(quality);
                if(renderer.ready()) renderer.draw();
                if(!renderer.ready()) {
                    print_failure(L"no-PBR no-shadows",renderer);
                    result=5;
                }
                renderer.resize(400,300);
                renderer.orbit(.18f,.20f);
                renderer.zoom(-1.f);
                if(renderer.ready()) renderer.draw();
                if(!renderer.ready()) {
                    print_failure(L"resize and camera",renderer);
                    result=6;
                }
                if(!result) {
                    const float previous_z=renderer.avatar_state().z;
                    renderer.set_avatar_controls({1.f,0.f,0.f,true,false});
                    Sleep(35);
                    renderer.draw();
                    Sleep(35);
                    renderer.draw();
                    if(renderer.avatar_state().z<=previous_z+.001f ||
                       !renderer.ready()) {
                        print_failure(L"avatar world movement",renderer);
                        result=14;
                    }
                    renderer.set_avatar_controls({0.f,0.f,0.f,false,true});
                    Sleep(35);
                    renderer.draw();
                    if(renderer.avatar_state().grounded || !renderer.ready()) {
                        print_failure(L"avatar jump and DX12 animation",renderer);
                        result=15;
                    }
                    renderer.reset_avatar();
                    renderer.set_avatar_controls({});
                    renderer.draw();
                    if(!renderer.ready()) {
                        print_failure(L"avatar reset and shadow draw",renderer);
                        result=16;
                    }
                }
                if(!result) {
                    // Body physics must affect real DX12 geometry, with the
                    // exact same deform values used for shadow and color passes.
                    renderer.reset_avatar();
                    renderer.set_body_physics_profile(kami::avatar::BodyProfile::Lively);
                    renderer.set_avatar_controls({1.f,0.f,0.f,true,false});
                    float max_motion=0.f;
                    float max_open_close=0.f,max_chest_twist=0.f;
                    for(int i=0;i<15;++i) {
                        Sleep(25);
                        renderer.draw();
                        const auto physics=renderer.body_physics_offsets();
                        const auto directional=renderer.detailed_body_physics_offsets();
                        max_motion=std::max(max_motion,
                            std::max(std::abs(physics.breast_y),
                            std::max(std::abs(physics.belly_y),std::abs(physics.butt_y))));
                        max_open_close=std::max(max_open_close,
                            std::abs(directional.at(kami::rigphysics::Axis::ButtOpenClose)));
                        max_chest_twist=std::max(max_chest_twist,
                            std::abs(directional.at(kami::rigphysics::Axis::ChestTwist)));
                    }
                    if(!renderer.ready()||max_open_close<.0001f||
                       max_chest_twist<.0001f ||
                       !renderer.skin_compatibility().butt_open_close_ready) {
                        print_failure(L"KAMI chest twist + opposing butt clap GPU skinning",renderer);
                        result=22;
                    }
                    if(!renderer.ready()||max_motion<.002f) {
                        print_failure(L"visible avatar body physics",renderer);
                        result=17;
                    }
                    const auto directional=renderer.detailed_body_physics_offsets();
                    if(!renderer.ready() ||
                       (!std::isfinite(directional.at(kami::rigphysics::Axis::ChestLateral))) ||
                       (!std::isfinite(directional.at(kami::rigphysics::Axis::ChestDepth))) ||
                       (!std::isfinite(directional.at(kami::rigphysics::Axis::ButtLateral)))) {
                        print_failure(L"six-axis GPU rig update",renderer);
                        result=20;
                    }
                    renderer.set_body_physics_profile(kami::avatar::BodyProfile::Off);
                    renderer.draw();
                    const auto disabled=renderer.body_physics_offsets();
                    if(!renderer.ready()||disabled.breast_y!=0.f||
                       disabled.belly_y!=0.f||disabled.butt_y!=0.f){
                        print_failure(L"body physics off",renderer);
                        result=18;
                    }
                    renderer.reset_avatar();
                    renderer.set_avatar_controls({});
                }
                if(!result) {
                    auto preview=kami::world::build_test_scene(8);
                    for(auto& v:preview.vertices)
                        v.texture_layer=float(kami::protocol::texture::kGPUImageLayers);
                    auto malicious=preview;
                    if(malicious.vertices.size()>malicious.terrain_count) {
                        auto& v=malicious.vertices[malicious.terrain_count];
                        v.bone_weights=255U;
                        v.bone_indices=200U;
                        if(renderer.set_creator_preview(std::move(malicious))) {
                            print_failure(L"rejected invalid skin weights",renderer);
                            result=21;
                        }
                    }
                    if(!result && !renderer.set_creator_preview(preview)) {
                        print_failure(L"OBJ preview geometry",renderer);result=7;
                    } else if(!result) {
                        renderer.draw();
                        if(!renderer.ready()){print_failure(L"preview draw",renderer);result=8;}
                        renderer.clear_creator_preview();
                        if(renderer.ready()) renderer.draw();
                        if(!renderer.ready()){print_failure(L"preview restore",renderer);result=9;}
                    }
                }
                if(!result) {
                    // Actual Creator Studio OBJ -> DX12 upload and render
                    // (not just the procedural preview mesh smoke).
                    std::istringstream obj_source(
                        "v 0 0 0\nv 1 0 0\nv 1 1 0\nv 0 1 0\n"
                        "f 1 2 3 4\n");
                    kami::creator::Preview imported_preview{};
                    std::string import_error;
                    if(!kami::creator::import_obj(
                        obj_source,imported_preview,import_error)) {
                        std::wcerr<<L"OBJ smoke parser failed\n";result=23;
                    } else if(!renderer.set_creator_preview(
                                   std::move(imported_preview.scene))||
                              renderer.vertex_count()!=6U||
                              renderer.skin_compatibility().status!=
                                  kami::rig::Compatibility::Unrigged) {
                        print_failure(L"OBJ GPU preview upload",renderer);
                        result=24;
                    } else {
                        renderer.draw();
                        if(!renderer.ready()) {
                            print_failure(L"OBJ GPU preview draw",renderer);
                            result=25;
                        }
                    }
                    renderer.clear_creator_preview();
                    if(renderer.ready())renderer.draw();
                }
                if(!result) {
                    // Synthetic verified protocol stages only: this verifies
                    // that already-decoded world triangles use the SAME DX12
                    // shadow, PBR and color path as the existing scene.
                    kami::protocol::Bootstrap connected;
                    constexpr auto agent="12345678-90ab-4cde-8fab-000000000001";
                    constexpr auto sid="12345678-90ab-4cde-8fab-000000000002";
                    constexpr auto region="12345678-90ab-4cde-8fab-000000000003";
                    const bool staged=
                        connected.endpoint_checked({
                            kami::protocol::ProbeStatus::TlsEndpointReached,405})&&
                        connected.login_reply({
                            agent,sid,"sim1234.agni.lindenlab.com",13000,
                            L"https://sim1234.agni.lindenlab.com:12345/cap/abc"},true)&&
                        connected.circuit_ack(agent,true)&&
                        connected.capability_reply(
                            L"https://sim1234.agni.lindenlab.com:12345/cap/def",true)&&
                        connected.region_snapshot(
                            {region,agent,128.f,128.f,40.f},true);
                    auto procedural=kami::world::build_test_scene(8);
                    kami::world::SceneMesh decoded{};
                    decoded.vertices.assign(
                        procedural.vertices.begin(),
                        procedural.vertices.begin()+6);
                    decoded.terrain_count=decoded.vertices.size();
                    if(!staged||
                       renderer.set_decoded_region_geometry(
                           connected,decoded,false)||
                       !renderer.set_decoded_region_geometry(
                           connected,std::move(decoded),true)||
                       !renderer.region_geometry_active()||
                       renderer.vertex_count()!=6U) {
                        print_failure(L"synthetic verified static region DX12 upload",renderer);
                        result=26;
                    } else {
                        renderer.draw();
                        if(!renderer.ready()) {
                            print_failure(L"static region PBR and shadow draw",renderer);
                            result=27;
                        }
                        connected.reset();
                        renderer.clear_decoded_region_geometry();
                        if(renderer.region_geometry_active()||
                           renderer.vertex_count()<=6U||!renderer.ready())
                            result=28;
                        if(renderer.ready())renderer.draw();
                    }
                }
                if(!result) {
                    wchar_t folder[MAX_PATH]{};
                    if(!GetTempPathW(MAX_PATH,folder))result=10;
                    else {
                        const std::wstring target=std::wstring(folder)+L"KAMI-DX12-Capture-Test.png";
                        if(!renderer.capture_png(target))result=11;
                        else {
                            renderer.draw();
                            if(renderer.capture_status()!=kami::world::CaptureStatus::Saved)
                                result=12;
                            else if(!std::filesystem::exists(target))
                                result=13;
                        }
                        std::error_code ignored;
                        std::filesystem::remove(target,ignored);
                    }
                }
                if(!result&&!check_sampled_pixels(renderer))result=32;
                if(!result&&!check_pbr_pixels(renderer)){std::wcerr<<L"PBR pixel regression failed\n";result=33;}
                if(!result&&!check_environment_pixels(renderer)){std::wcerr<<L"Environment/terrain pixel regression failed\n";result=34;}
                if(!result) std::wcout
                    <<(force_warp?L"KAMI WARP VIRTUAL GPU VERIFIED":L"KAMI DX12 SMOKE PASS")
                    <<L" - adapter: "<<renderer.adapter_name()
                    <<L"; drawn="<<renderer.vertex_count()
                    <<L" vertices; PBR, shadows, texture upload, resize, FPS presentation path\n";
            }
        }
        renderer.shutdown();
    }
    DestroyWindow(window);
    UnregisterClassW(desc.lpszClassName,instance);
    CoUninitialize();
    if(result!=0 && result!=77)
        std::wcerr<<L"KAMI DX12 smoke failure stage "<<result<<L"\n";
    return result;
}


