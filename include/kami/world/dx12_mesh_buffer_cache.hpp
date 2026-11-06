#pragma once
#include <d3d12.h>
#include <algorithm>
#include <wrl/client.h>
#include <cstdint>
#include <cmath>
#include <cstring>
#include <limits>
#include <map>
#include <set>
#include <vector>
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
            // Growth is bounded before addition to avoid overflow. Keep
            // allocations aligned for future per-asset DX12 buffer streams.
            const std::uint64_t limit=std::numeric_limits<UINT>::max();
            const std::uint64_t growth=previous<=limit-previous/2U?
                previous+previous/2U:limit;
            const std::uint64_t wanted=std::max<std::uint64_t>(bytes,growth);
            desc.Width=(std::min(wanted,limit)+255U)&~std::uint64_t{255U};
            if(desc.Width>limit)desc.Width=limit&~std::uint64_t{255U};
            if(desc.Width<bytes)return false;
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

    // Submit an already-resident vertex stream using the caller's current
    // root signature, pipeline state, material descriptors and shader ABI.
    // The caller owns command-list lifetime and must fence GPU work before
    // replacing or evicting this resource. No upload occurs in this path.
    [[nodiscard]] bool record_draw(ID3D12GraphicsCommandList* commands,
                                   const std::string& asset_id,
                                   UINT vertex_count,
                                   UINT first_vertex=0U) const noexcept {
        if(!commands||!vertex_count)return false;
        const auto* buffer=find(asset_id);
        if(!buffer||!buffer->resource||!buffer->view.StrideInBytes)return false;
        const UINT capacity=buffer->view.SizeInBytes/buffer->view.StrideInBytes;
        if(first_vertex>capacity||vertex_count>capacity-first_vertex)return false;
        commands->IASetVertexBuffers(0,1,&buffer->view);
        commands->DrawInstanced(vertex_count,1,first_vertex,0);
        return true;
    }

    // Upload a fully transformed instance span from the decoded scene.
    // Each simulator-local instance receives its own resident vertex stream;
    // the original Vertex layout, texture layers and PBR material indices
    // remain byte-for-byte unchanged. Call only after fencing GPU readers.
    template<class VertexT>
    [[nodiscard]] bool upload_instance(ID3D12Device* device,
                                       const std::string& region_key,
                                       std::uint32_t local_id,
                                       std::uint64_t revision,
                                       const VertexT* scene_vertices,
                                       std::size_t scene_count,
                                       std::size_t first_vertex,
                                       std::size_t vertex_count) {
        if(region_key.empty()||!local_id||!scene_vertices||!vertex_count||
           first_vertex>scene_count||vertex_count>scene_count-first_vertex||
           vertex_count>std::numeric_limits<UINT>::max()/sizeof(VertexT))
            return false;
        const auto key=region_key+":"+std::to_string(local_id);
        return upload(device,key,revision,scene_vertices+first_vertex,
                      vertex_count*sizeof(VertexT),sizeof(VertexT));
    }

    // Record an individual simulator mesh instance using the exact same
    // vertex layout and root bindings as the combined-scene renderer.
    // The caller must select the correct depth/PBR pipeline and exclude this
    // span from combined-scene draws to avoid double rendering.
    [[nodiscard]] bool record_instance_draw(
        ID3D12GraphicsCommandList* commands,
        const std::string& region_key,
        std::uint32_t local_id) const noexcept {
        if(region_key.empty()||!local_id)return false;
        const auto key=region_key+":"+std::to_string(local_id);
        const auto* buffer=find(key);
        if(!buffer||!buffer->view.StrideInBytes)return false;
        const UINT count=buffer->view.SizeInBytes/buffer->view.StrideInBytes;
        return record_draw(commands,key,count);
    }

    // Preflight a complete resident instance batch before recording any draw.
    // A failed preflight allows the caller to retain the combined-scene path
    // without partially submitting the mesh batch (which would duplicate or
    // omit surfaces when switching between paths).
    [[nodiscard]] bool can_draw_instance_batch(
        const std::string& region_key,
        const std::vector<std::uint32_t>& local_ids) const noexcept {
        if(region_key.empty()||local_ids.empty())return false;
        std::set<std::uint32_t> unique_ids;
        for(const auto id:local_ids){
            if(!id||!unique_ids.insert(id).second)return false;
            const auto* buffer=find(region_key+":"+std::to_string(id));
            if(!buffer||!buffer->resource||!buffer->view.StrideInBytes||
               !buffer->view.SizeInBytes||
               buffer->view.SizeInBytes%buffer->view.StrideInBytes)return false;
        }
        return true;
    }

    // Exact preflight for switching a complete region mesh batch away from
    // the combined-scene draw. Reject stale revisions and mismatched vertex
    // counts: otherwise a successful residency lookup can silently omit
    // triangles after a simulator geometry update.
    struct InstanceDrawRequirement {
        std::uint32_t local_id=0;
        UINT vertex_count=0;
    };
    [[nodiscard]] bool can_draw_exact_instance_batch(
        const std::string& region_key,
        std::uint64_t revision,
        const std::vector<InstanceDrawRequirement>& required) const noexcept {
        if(region_key.empty()||required.empty())return false;
        std::set<std::uint32_t> seen;
        for(const auto& item:required){
            if(!item.local_id||!item.vertex_count||
               !seen.insert(item.local_id).second)return false;
            const auto* resident=find(region_key+":"+std::to_string(item.local_id));
            if(!resident||!resident->resource||resident->generation!=generation_||
               resident->revision!=revision||
               !resident->view.StrideInBytes||
               resident->view.SizeInBytes%resident->view.StrideInBytes||
               resident->view.SizeInBytes/resident->view.StrideInBytes!=item.vertex_count)
                return false;
        }
        return true;
    }

    // Validate that source mesh spans form a non-overlapping subset of the
    // decoded scene. A resident GPU batch alone cannot prove this: duplicate
    // source ranges can produce double-drawn triangles after path switching.
    struct InstanceSourceSpan {
        std::uint32_t local_id=0;
        std::size_t first_vertex=0;
        std::size_t vertex_count=0;
    };
    [[nodiscard]] static bool validate_instance_source_spans(
        std::size_t scene_vertex_count,
        const std::vector<InstanceSourceSpan>& spans) noexcept {
        if(!scene_vertex_count||spans.empty())return false;
        std::set<std::uint32_t> ids;
        std::vector<std::pair<std::size_t,std::size_t>> intervals;
        intervals.reserve(spans.size());
        for(const auto& span:spans){
            if(!span.local_id||!ids.insert(span.local_id).second||
               !span.vertex_count||span.first_vertex>scene_vertex_count||
               span.vertex_count>scene_vertex_count-span.first_vertex||
               span.first_vertex%3U||span.vertex_count%3U)return false;
            intervals.emplace_back(span.first_vertex,
                                   span.first_vertex+span.vertex_count);
        }
        std::sort(intervals.begin(),intervals.end());
        for(std::size_t i=1;i<intervals.size();++i)
            if(intervals[i].first<intervals[i-1].second)return false;
        return true;
    }

    // Switching from the combined scene is safe only when the individual
    // resident streams cover every mesh triangle exactly once. The older
    // overlap-only validation intentionally permits holes for staging, but
    // an authoritative draw path must reject them to avoid invisible meshes.
    [[nodiscard]] static bool covers_complete_mesh_scene(
        std::size_t mesh_vertex_count,
        const std::vector<InstanceSourceSpan>& spans) noexcept {
        if(!validate_instance_source_spans(mesh_vertex_count,spans)||
           mesh_vertex_count%3U)return false;
        std::vector<std::pair<std::size_t,std::size_t>> intervals;
        intervals.reserve(spans.size());
        for(const auto& span:spans)
            intervals.emplace_back(span.first_vertex,
                                   span.first_vertex+span.vertex_count);
        std::sort(intervals.begin(),intervals.end());
        std::size_t cursor=0;
        for(const auto& interval:intervals){
            if(interval.first!=cursor)return false;
            cursor=interval.second;
        }
        return cursor==mesh_vertex_count;
    }

    // An authoritative mesh draw must not reuse the opaque shadow/PBR path
    // for alpha-blended triangles. This checks material classification before
    // replacing the combined scene, retaining its established blend ordering.
    [[nodiscard]] static bool opaque_material_parity(
        const std::vector<std::uint32_t>& material_ids,
        const std::vector<bool>& blended_materials) noexcept {
        if(material_ids.empty()||material_ids.size()%3U)return false;
        for(std::size_t i=0;i<material_ids.size();i+=3U){
            const auto material=material_ids[i];
            if(material_ids[i+1U]!=material||material_ids[i+2U]!=material||
               material>=blended_materials.size()||
               blended_materials[material])return false;
        }
        return true;
    }

    // Conservative CPU-side visibility preflight for future GPU-driven draws.
    // Unknown or invalid bounds remain visible: never cull a simulator object
    // simply because its metadata has not arrived yet.
    struct VisibilitySphere {
        float x=0.f,y=0.f,z=0.f,radius=0.f;
        bool valid=false;
    };
    struct VisibilityPlane { float x=0.f,y=0.f,z=0.f,d=0.f; };
    [[nodiscard]] static bool potentially_visible(
        const VisibilitySphere& sphere,
        const std::vector<VisibilityPlane>& planes) noexcept {
        if(!sphere.valid||!std::isfinite(sphere.x)||
           !std::isfinite(sphere.y)||!std::isfinite(sphere.z)||
           !std::isfinite(sphere.radius)||sphere.radius<0.f||
           planes.size()!=6U)return true;
        for(const auto& plane:planes){
            if(!std::isfinite(plane.x)||!std::isfinite(plane.y)||
               !std::isfinite(plane.z)||!std::isfinite(plane.d))return true;
            // Evaluate extreme simulator coordinates in double precision:
            // float dot products can overflow and make visibility unreliable.
            const double nx=plane.x,ny=plane.y,nz=plane.z;
            const double length=std::sqrt(nx*nx+ny*ny+nz*nz);
            if(!std::isfinite(length)||length<=0.0)return true;
        }
        for(const auto& plane:planes){
            const double nx=plane.x,ny=plane.y,nz=plane.z;
            const double length=std::sqrt(nx*nx+ny*ny+nz*nz);
            const double distance=nx*static_cast<double>(sphere.x)+
                ny*static_cast<double>(sphere.y)+
                nz*static_cast<double>(sphere.z)+plane.d;
            if(!std::isfinite(distance))return true;
            if(distance < -static_cast<double>(sphere.radius)*length)return false;
        }
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
