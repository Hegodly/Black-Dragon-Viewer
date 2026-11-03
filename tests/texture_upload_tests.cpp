#include "kami/world/device.hpp"
#include <d3d12.h>
#include <dxgi1_6.h>

int main() {
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
    return 0;
}
