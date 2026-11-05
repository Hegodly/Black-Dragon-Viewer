#include "kami/world/device.hpp"
#include <d3d12.h>
#include <dxgi1_6.h>
#include "kami/world/scene.hpp"
#include "kami/world/texture_atlas.hpp"
#include <array>
static_assert(sizeof(kami::world::Vertex)==68U,
    "DX12 mesh texture layer must fit the vertex input layout");

int main() {
    // Incremental GPU atlas uploads must touch only changed material
    // slices, not repack unrelated textures on every J2C arrival.
    using kami::world::changed_atlas_layers;
    const std::map<std::string,std::uint16_t> before{
        {"texture-a",1U},{"texture-b",6U},{"texture-c",24U}};
    const std::map<std::string,std::uint16_t> after{
        {"texture-a",1U},{"texture-d",6U},{"texture-c",24U}};
    if(changed_atlas_layers(before,before).size()!=0U)return 7;
    if(changed_atlas_layers(before,after)!=
       std::vector<std::uint16_t>{6U})return 8;
    if(changed_atlas_layers(after,{} )!=
       std::vector<std::uint16_t>({1U,6U,24U}))return 9;
    if(changed_atlas_layers({},before)!=
       std::vector<std::uint16_t>({1U,6U,24U}))return 10;
    if(!changed_atlas_layers(before,after,0U).empty())return 11;
    kami::world::Device device;
    if(device.initialize(true)==kami::world::DeviceStatus::Unavailable)
        return 0; // The hosted VM may not have a DX12 adapter.
    D3D12_RESOURCE_DESC texture{};
    texture.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texture.Width=64;
    texture.Height=64;
    texture.DepthOrArraySize=1;
    texture.MipLevels=1;
    texture.Format=DXGI_FORMAT_R8G8B8A8_UNORM;
    texture.SampleDesc.Count=1;
    texture.Layout=D3D12_TEXTURE_LAYOUT_UNKNOWN;
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout{};
    UINT rows=0;
    UINT64 row_bytes=0,total_bytes=0;
    device.native_device()->GetCopyableFootprints(
        &texture,0,1,0,&layout,&rows,&row_bytes,&total_bytes);
    if(rows!=64 || row_bytes!=256) return 1;
    if(layout.Footprint.RowPitch<row_bytes ||
       layout.Footprint.RowPitch%256!=0) return 2;
    if(total_bytes<row_bytes*rows) return 3;
    if(layout.Footprint.Format!=DXGI_FORMAT_R8G8B8A8_UNORM) return 4;
    // Live SL textures use one bounded 25-slice GPU image array:
    // fallback layer zero + up to 24 independently decoded assets.
    texture.Width=256;
    texture.Height=256;
    texture.DepthOrArraySize=25;
    std::array<D3D12_PLACED_SUBRESOURCE_FOOTPRINT,25> mapped{};
    std::array<UINT,25> rows_arr{};
    std::array<UINT64,25> sizes{};
    UINT64 upload=0;
    device.native_device()->GetCopyableFootprints(
        &texture,0,25,0,mapped.data(),rows_arr.data(),sizes.data(),&upload);
    if(upload==0||upload>16U*1024U*1024U)return 5;
    for(unsigned i=0;i<25;++i){
        if(rows_arr[i]!=256U||sizes[i]!=1024U||
           mapped[i].Footprint.RowPitch%256U!=0U)return 6;
    }
    return 0;
}
