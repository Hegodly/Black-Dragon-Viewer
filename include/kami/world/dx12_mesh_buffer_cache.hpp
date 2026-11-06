#pragma once
#include <d3d12.h>
#include <wrl/client.h>
#include <cstdint>
#include <cstring>
#include <limits>
#include <map>
#include <string>
#include <utility>

namespace kami::world {

// DX12-owned persistent asset buffers. The cache deliberately does not alter
// the existing shader ABI: the renderer will bind these as ordinary vertex
// streams in both its depth and PBR passes when the instance path is wired.
class Dx12MeshBufferCache final {
public:
    struct Buffer {
        Microsoft::WRL::ComPtr<ID3D12Resource> resource;
        D3D12_VERTEX_BUFFER_VIEW view{};
        std::uint64_t generation=0;
        std::uint64_t revision=0;
    };

    void begin_generation() noexcept { ++generation_; }
    void clear() noexcept { buffers_.clear(); generation_=0; }
    [[nodiscard]] std::size_t size() const noexcept { return buffers_.size(); }
    [[nodiscard]] std::uint64_t allocated_bytes() const noexcept {
        std::uint64_t total=0;
        for(const auto& [id,buffer]:buffers_){
            (void)id;
            if(buffer.resource)total+=buffer.resource->GetDesc().Width;
        }
        return total;
    }
    // This is a GPU resource cache, not a mesh draw scheduler. Callers must
    // submit each resident asset using the original shader input ABI.
    void touch(const std::string& asset_id) noexcept {
        const auto it=buffers_.find(asset_id);
        if(it!=buffers_.end())it->second.generation=generation_;
    }
    [[nodiscard]] const Buffer* find(const std::string& asset_id) const noexcept {
        auto it=buffers_.find(asset_id);
        return it==buffers_.end()?nullptr:&it->second;
    }

    // The caller must fence outstanding draw submissions before replacing or
    // evicting buffers. An unchanged revision never reuploads the same mesh.
    [[nodiscard]] bool upload(ID3D12Device* device,
                              const std::string& asset_id,
                              std::uint64_t revision,
                              const void* vertices,
                              std::size_t bytes,
                              UINT stride) {
        if(!device||asset_id.empty()||!vertices||!stride||!bytes||
           bytes>std::numeric_limits<UINT>::max()||
           bytes%stride!=0U)return false;
        auto existing=buffers_.find(asset_id);
        if(existing!=buffers_.end()&&existing->second.revision==revision&&
           existing->second.view.SizeInBytes==bytes&&
           existing->second.view.StrideInBytes==stride){
            existing->second.generation=generation_;
            return true;
        }
        // In-place updates are legal only after the caller fences GPU work.
        // Retain capacity to avoid reallocating on every simulator update.
        const bool reuse=existing!=buffers_.end()&&
            existing->second.resource&&
            existing->second.resource->GetDesc().Width>=bytes;
        Microsoft::WRL::ComPtr<ID3D12Resource> resource;
        if(reuse)resource=existing->second.resource;
        else {
            D3D12_HEAP_PROPERTIES heap{};
            heap.Type=D3D12_HEAP_TYPE_UPLOAD;
            heap.CreationNodeMask=1;
            heap.VisibleNodeMask=1;
            D3D12_RESOURCE_DESC desc{};
            desc.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;
            const std::uint64_t previous=existing!=buffers_.end()&&
                existing->second.resource?
                existing->second.resource->GetDesc().Width:0U;
            desc.Width=std::min<std::uint64_t>(
                std::numeric_limits<UINT>::max(),
                std::max<std::uint64_t>(bytes,previous+previous/2U));
            desc.Height=1;
            desc.DepthOrArraySize=1;
            desc.MipLevels=1;
            desc.SampleDesc.Count=1;
            desc.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
            if(FAILED(device->CreateCommittedResource(
                &heap,D3D12_HEAP_FLAG_NONE,&desc,
                D3D12_RESOURCE_STATE_GENERIC_READ,nullptr,
                IID_PPV_ARGS(resource.GetAddressOf()))))return false;
        }
        void* mapped=nullptr;
        const D3D12_RANGE no_read{0,0};
        if(FAILED(resource->Map(0,&no_read,&mapped))||!mapped)return false;
        std::memcpy(mapped,vertices,bytes);
        const D3D12_RANGE written{0,bytes};
        resource->Unmap(0,&written);
        Buffer candidate;
        candidate.view.BufferLocation=resource->GetGPUVirtualAddress();
        candidate.view.SizeInBytes=static_cast<UINT>(bytes);
        candidate.view.StrideInBytes=stride;
        candidate.resource=std::move(resource);
        candidate.generation=generation_;
        candidate.revision=revision;
        buffers_.insert_or_assign(asset_id,std::move(candidate));
        return true;
    }

    // Budgeted residency for future per-asset mesh streams. Call only after
    // the graphics fence completes. Never evict assets used in the current
    // generation, nor the active combined-scene fallback buffer.
    // Returns the number of bytes actually released; if live assets alone
    // exceed the budget, report zero rather than discarding visible meshes.
    [[nodiscard]] std::uint64_t evict_stale_to_budget(
        std::uint64_t budget_bytes) {
        std::uint64_t used=allocated_bytes();
        std::uint64_t released=0U;
        while(used>budget_bytes){
            auto victim=buffers_.end();
            for(auto it=buffers_.begin();it!=buffers_.end();++it){
                if(it->first=="__kami_scene__"||
                   it->second.generation==generation_)continue;
                if(victim==buffers_.end()||
                   it->second.generation<victim->second.generation)
                    victim=it;
            }
            if(victim==buffers_.end())break;
            const auto bytes=victim->second.resource?
                victim->second.resource->GetDesc().Width:0U;
            used-=bytes;
            released+=bytes;
            buffers_.erase(victim);
        }
        return released;
    }
    // Evict only after the graphics queue fence has completed.
    void evict_unseen() {
        for(auto it=buffers_.begin();it!=buffers_.end();){
            if(it->first!="__kami_scene__"&&
               it->second.generation!=generation_)it=buffers_.erase(it);
            else ++it;
        }
    }
private:
    std::uint64_t generation_=0;
    std::map<std::string,Buffer> buffers_;
};
} // namespace kami::world
