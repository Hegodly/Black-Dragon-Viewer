#pragma once
#include <d3d12.h>
#include <algorithm>
#include <array>
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

    // Bound resident draw batches so command generation and fallback work
    // stay predictable on dense regions.
    static constexpr std::size_t kMaxResidentBatchDraws=256U;
    [[nodiscard]] static bool within_resident_batch_draw_budget(
        std::size_t count) noexcept {
        return count>0U&&count<=kMaxResidentBatchDraws;
    }
    [[nodiscard]] static bool valid_instance_vertex_view(
        const D3D12_VERTEX_BUFFER_VIEW& view,UINT expected_vertices) noexcept {
        if(!view.BufferLocation||view.BufferLocation%4U||
           !view.StrideInBytes||view.StrideInBytes%4U||
           !view.SizeInBytes||view.SizeInBytes%view.StrideInBytes||
           view.SizeInBytes/view.StrideInBytes!=expected_vertices)return false;
        return view.BufferLocation<=
            std::numeric_limits<std::uint64_t>::max()-view.SizeInBytes;
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
        if(region_key.empty()||
           !within_resident_batch_draw_budget(required.size()))return false;
        // This preflight runs every frame. The batch is capped at 256, so use
        // bounded stack storage instead of allocating one tree node per mesh.
        std::array<std::uint32_t,kMaxResidentBatchDraws> seen_ids{};
        for(std::size_t i=0U;i<required.size();++i){
            const auto& item=required[i];
            if(!item.local_id||!item.vertex_count)return false;
            seen_ids[i]=item.local_id;
            const auto* resident=find(region_key+":"+std::to_string(item.local_id));
            if(!resident||!resident->resource||resident->generation!=generation_||
               resident->revision!=revision||
               !valid_instance_vertex_view(resident->view,item.vertex_count))
                return false;
        }
        auto seen_end=seen_ids.begin()+required.size();
        std::sort(seen_ids.begin(),seen_end);
        return std::adjacent_find(seen_ids.begin(),seen_end)==seen_end;
    }

    // Record an entire resident batch only after one all-or-nothing preflight.
    // No command is emitted for a stale, partial, oversized, or duplicate batch.
    [[nodiscard]] bool record_instance_batch(
        ID3D12GraphicsCommandList* commands,
        const std::string& region_key,
        std::uint64_t revision,
        const std::vector<InstanceDrawRequirement>& required) const noexcept {
        if(!commands||!can_draw_exact_instance_batch(region_key,revision,required))
            return false;
        for(const auto& item:required){
            const auto* resident=find(region_key+":"+std::to_string(item.local_id));
            commands->IASetVertexBuffers(0,1,&resident->view);
            commands->DrawInstanced(item.vertex_count,1,0,0);
        }
        return true;
    }

    struct IndirectDrawCommand {
        D3D12_VERTEX_BUFFER_VIEW vertex_buffer{};
        D3D12_DRAW_ARGUMENTS draw{};
    };
    static_assert(sizeof(IndirectDrawCommand)==32U,
                  "DX12 indirect command stride must be 32 bytes");
    // Validate both the command count and the complete writable segment with
    // subtraction-based bounds checks before CPU writes or ExecuteIndirect.
    [[nodiscard]] static bool valid_indirect_argument_range(
        std::uint64_t total_bytes,std::uint64_t offset,
        std::size_t command_count,std::size_t command_capacity) noexcept {
        if(command_capacity>kMaxResidentBatchDraws||
           command_count>command_capacity||offset%4U)return false;
        constexpr auto stride=static_cast<std::uint64_t>(
            sizeof(IndirectDrawCommand));
        if(command_capacity>
           std::numeric_limits<std::uint64_t>::max()/stride)return false;
        const auto capacity_bytes=
            static_cast<std::uint64_t>(command_capacity)*stride;
        if(offset>total_bytes||capacity_bytes>total_bytes-offset)return false;
        return command_count<=command_capacity;
    }
    // Build a bounded ExecuteIndirect command list only after validating the
    // entire resident set and every visible ID. On failure, output is untouched.
    [[nodiscard]] bool build_visible_instance_indirect_commands(
        const std::string& region_key,
        std::uint64_t revision,
        const std::vector<InstanceDrawRequirement>& required,
        const std::uint32_t* visible_ids,
        std::size_t visible_count,
        IndirectDrawCommand* output,
        std::size_t capacity,
        std::size_t& command_count) const noexcept {
        command_count=0U;
        if(!output||!visible_ids||
           !can_draw_exact_instance_batch(region_key,revision,required)||
           visible_count>required.size()||capacity<visible_count)return false;
        const auto visible_end=visible_ids+visible_count;
        if(!std::is_sorted(visible_ids,visible_end)||
           std::adjacent_find(visible_ids,visible_end)!=visible_end)return false;
        std::array<std::uint32_t,kMaxResidentBatchDraws> required_ids{};
        for(std::size_t i=0U;i<required.size();++i)
            required_ids[i]=required[i].local_id;
        auto required_end=required_ids.begin()+required.size();
        std::sort(required_ids.begin(),required_end);
        for(auto it=visible_ids;it!=visible_end;++it)
            if(!std::binary_search(required_ids.begin(),required_end,*it))
                return false;
        // No validation below this point can fail: all resources were checked
        // by the all-or-nothing preflight and all IDs are members of required.
        for(const auto& item:required){
            if(!std::binary_search(visible_ids,visible_end,item.local_id))continue;
            const auto* resident=find(region_key+":"+std::to_string(item.local_id));
            output[command_count++]={resident->view,
                D3D12_DRAW_ARGUMENTS{item.vertex_count,1U,0U,0U}};
        }
        return true;
    }

    // Record only instances proven visible in the current pass. The full
    // resident batch is preflighted first, so culling never changes the
    // combined-scene exclusion decision; unknown bounds remain in the draw list.
    [[nodiscard]] bool record_visible_instance_batch(
        ID3D12GraphicsCommandList* commands,
        const std::string& region_key,
        std::uint64_t revision,
        const std::vector<InstanceDrawRequirement>& required,
        const std::uint32_t* visible_ids,
        std::size_t visible_count) const noexcept {
        if(!commands||!visible_ids||
           !can_draw_exact_instance_batch(region_key,revision,required)||
           visible_count>required.size())return false;
        // The selector emits a sorted, unique ID list. Validate that contract
        // before using binary search so malformed callers cannot skip draws.
        const auto visible_end=visible_ids+visible_count;
        if(!std::is_sorted(visible_ids,visible_end)||
           std::adjacent_find(visible_ids,visible_end)!=visible_end)return false;
        std::array<std::uint32_t,kMaxResidentBatchDraws> required_ids{};
        for(std::size_t i=0;i<required.size();++i)
            required_ids[i]=required[i].local_id;
        auto required_end=required_ids.begin()+required.size();
        std::sort(required_ids.begin(),required_end);
        for(auto it=visible_ids;it!=visible_end;++it)
            if(!std::binary_search(required_ids.begin(),required_end,*it))return false;
        for(const auto& item:required){
            if(!std::binary_search(visible_ids,visible_end,item.local_id))continue;
            const auto* resident=find(region_key+":"+std::to_string(item.local_id));
            if(!resident||!resident->resource)return false;
            commands->IASetVertexBuffers(0,1,&resident->view);
            commands->DrawInstanced(item.vertex_count,1,0,0);
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
    // True only when one complete triangle is wholly owned by a resident
    // instance span. The subtraction form avoids size_t end-offset overflow.
    [[nodiscard]] static bool triangle_is_in_instance_spans(
        std::size_t first_vertex,
        const std::vector<InstanceSourceSpan>& spans) noexcept {
        if(first_vertex%3U)return false;
        for(const auto& span:spans){
            if(span.first_vertex%3U||span.vertex_count<3U||
               span.vertex_count%3U||first_vertex<span.first_vertex)continue;
            const auto offset=first_vertex-span.first_vertex;
            if(offset<=span.vertex_count-3U)return true;
        }
        return false;
    }

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
    struct VisibilityAabb {
        float min_x=0.f,min_y=0.f,min_z=0.f;
        float max_x=0.f,max_y=0.f,max_z=0.f;
        bool valid=false;
    };
    struct VisibilityPlane { float x=0.f,y=0.f,z=0.f,d=0.f; };
    struct InstanceVisibility {
        std::uint32_t local_id=0;
        VisibilitySphere bounds{};
        VisibilityAabb aabb{};
    };

    // Build a conservative sphere from the exact decoded world-space vertices
    // owned by one simulator instance. Invalid coordinates deliberately yield
    // unknown bounds; callers keep those objects visible instead of losing geometry.
    template<class VertexT>
    [[nodiscard]] static VisibilitySphere bounds_for_vertices(
        const VertexT* vertices,std::size_t count) noexcept {
        if(!vertices||!count)return {};
        double min_x=std::numeric_limits<double>::infinity();
        double min_y=min_x,min_z=min_x;
        double max_x=-min_x,max_y=-min_x,max_z=-min_x;
        for(std::size_t i=0;i<count;++i){
            const double x=vertices[i].px,y=vertices[i].py,z=vertices[i].pz;
            if(!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(z))return {};
            min_x=std::min(min_x,x);min_y=std::min(min_y,y);min_z=std::min(min_z,z);
            max_x=std::max(max_x,x);max_y=std::max(max_y,y);max_z=std::max(max_z,z);
        }
        const double cx=(min_x+max_x)*.5,cy=(min_y+max_y)*.5,cz=(min_z+max_z)*.5;
        const float fx=static_cast<float>(cx),fy=static_cast<float>(cy),fz=static_cast<float>(cz);
        if(!std::isfinite(fx)||!std::isfinite(fy)||!std::isfinite(fz))return {};
        double radius_squared=0.0;
        for(std::size_t i=0;i<count;++i){
            const double dx=static_cast<double>(vertices[i].px)-fx;
            const double dy=static_cast<double>(vertices[i].py)-fy;
            const double dz=static_cast<double>(vertices[i].pz)-fz;
            radius_squared=std::max(radius_squared,dx*dx+dy*dy+dz*dz);
        }
        const double radius=std::sqrt(radius_squared);
        if(!std::isfinite(radius)||radius>std::numeric_limits<float>::max())return {};
        float fr=static_cast<float>(radius);
        if(static_cast<double>(fr)<radius)
            fr=std::nextafter(fr,std::numeric_limits<float>::infinity());
        return {fx,fy,fz,fr,true};
    }

    // Build an exact world-space AABB from the same decoded instance span.
    // Invalid or absent vertices produce unknown bounds, which remain visible.
    template<class VertexT>
    [[nodiscard]] static VisibilityAabb aabb_for_vertices(
        const VertexT* vertices,std::size_t count) noexcept {
        if(!vertices||!count)return {};
        float min_x=std::numeric_limits<float>::infinity();
        float min_y=min_x,min_z=min_x;
        float max_x=-min_x,max_y=-min_x,max_z=-min_x;
        for(std::size_t i=0;i<count;++i){
            const float x=vertices[i].px,y=vertices[i].py,z=vertices[i].pz;
            if(!std::isfinite(x)||!std::isfinite(y)||!std::isfinite(z))return {};
            min_x=std::min(min_x,x);min_y=std::min(min_y,y);min_z=std::min(min_z,z);
            max_x=std::max(max_x,x);max_y=std::max(max_y,y);max_z=std::max(max_z,z);
        }
        return {min_x,min_y,min_z,max_x,max_y,max_z,true};
    }

    // Extract six Direct3D clip planes from a row-major row-vector matrix.
    // D3D depth is [0,w], not OpenGL's [-w,w].
    [[nodiscard]] static bool extract_frustum_planes(
        const std::array<float,16>& m,
        std::array<VisibilityPlane,6>& out) noexcept {
        for(const auto value:m)if(!std::isfinite(value))return false;
        const std::array<VisibilityPlane,6> candidate{{
            {m[0]+m[3],m[4]+m[7],m[8]+m[11],m[12]+m[15]},
            {m[3]-m[0],m[7]-m[4],m[11]-m[8],m[15]-m[12]},
            {m[1]+m[3],m[5]+m[7],m[9]+m[11],m[13]+m[15]},
            {m[3]-m[1],m[7]-m[5],m[11]-m[9],m[15]-m[13]},
            {m[2],m[6],m[10],m[14]},
            {m[3]-m[2],m[7]-m[6],m[11]-m[10],m[15]-m[14]}
        }};
        for(const auto& plane:candidate){
            const double x=plane.x,y=plane.y,z=plane.z;
            const double length=std::sqrt(x*x+y*y+z*z);
            if(!std::isfinite(length)||length<=0.0)return false;
        }
        out=candidate;
        return true;
    }

    // Unknown, invalid or ambiguous bounds remain visible. Never cull an
    // object simply because mesh metadata or a valid frustum has not arrived.
    template<class PlaneRange>
    [[nodiscard]] static bool potentially_visible(
        const VisibilitySphere& sphere,const PlaneRange& planes) noexcept {
        if(!sphere.valid||!std::isfinite(sphere.x)||
           !std::isfinite(sphere.y)||!std::isfinite(sphere.z)||
           !std::isfinite(sphere.radius)||sphere.radius<0.f||
           planes.size()!=6U)return true;
        for(const auto& plane:planes){
            if(!std::isfinite(plane.x)||!std::isfinite(plane.y)||
               !std::isfinite(plane.z)||!std::isfinite(plane.d))return true;
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

    // Conservative AABB/frustum test using each plane's positive vertex.
    // If even the positive vertex is outside, the entire box is outside.
    // Invalid bounds or planes are never used to hide geometry.
    template<class PlaneRange>
    [[nodiscard]] static bool potentially_visible_aabb(
        const VisibilityAabb& box,const PlaneRange& planes) noexcept {
        if(!box.valid||!std::isfinite(box.min_x)||!std::isfinite(box.min_y)||
           !std::isfinite(box.min_z)||!std::isfinite(box.max_x)||
           !std::isfinite(box.max_y)||!std::isfinite(box.max_z)||
           box.min_x>box.max_x||box.min_y>box.max_y||box.min_z>box.max_z||
           planes.size()!=6U)return true;
        for(const auto& plane:planes){
            if(!std::isfinite(plane.x)||!std::isfinite(plane.y)||
               !std::isfinite(plane.z)||!std::isfinite(plane.d))return true;
            const double nx=plane.x,ny=plane.y,nz=plane.z;
            const double length_squared=nx*nx+ny*ny+nz*nz;
            if(!std::isfinite(length_squared)||length_squared<=0.0)return true;
            const double px=nx>=0.0?box.max_x:box.min_x;
            const double py=ny>=0.0?box.max_y:box.min_y;
            const double pz=nz>=0.0?box.max_z:box.min_z;
            const double distance=nx*px+ny*py+nz*pz+plane.d;
            if(!std::isfinite(distance))return true;
            if(distance<0.0)return false;
        }
        return true;
    }

    // Deterministic per-pass draw selection. Missing, invalid or duplicate
    // bounds are conservative-visible; invalid requirement sets fail so the
    // renderer can fall back to the complete resident batch.
    [[nodiscard]] static bool select_visible_instance_ids(
        const std::vector<InstanceDrawRequirement>& required,
        const std::vector<InstanceVisibility>& bounds,
        const std::array<VisibilityPlane,6>& planes,
        std::uint32_t* visible_ids,std::size_t capacity,
        std::size_t& visible_count) noexcept {
        visible_count=0U;
        if(!within_resident_batch_draw_budget(required.size())||
           !visible_ids||capacity<required.size())return false;
        // Bounds are sorted once when the decoded scene is accepted. Refuse an
        // unsorted cache rather than risk a false-negative lookup; the caller
        // responds by drawing every resident object for this pass.
        const auto by_id=[](const InstanceVisibility& a,
                            const InstanceVisibility& b) noexcept {
            return a.local_id<b.local_id;
        };
        if(!std::is_sorted(bounds.begin(),bounds.end(),by_id))return false;
        // Validate requirement IDs in O(n log n) without changing their draw order.
        std::array<std::uint32_t,kMaxResidentBatchDraws> required_ids{};
        for(std::size_t i=0;i<required.size();++i){
            const auto& item=required[i];
            if(!item.local_id||!item.vertex_count)return false;
            required_ids[i]=item.local_id;
        }
        auto required_end=required_ids.begin()+required.size();
        std::sort(required_ids.begin(),required_end);
        if(std::adjacent_find(required_ids.begin(),required_end)!=required_end)
            return false;
        for(const auto& item:required){
            const auto found=std::lower_bound(bounds.begin(),bounds.end(),
                item.local_id,[](const InstanceVisibility& candidate,
                                 std::uint32_t id) noexcept {
                    return candidate.local_id<id;
                });
            const bool missing=found==bounds.end()||found->local_id!=item.local_id;
            const bool ambiguous=!missing&&found+1!=bounds.end()&&
                (found+1)->local_id==item.local_id;
            const bool visible=missing||ambiguous||
                (found->aabb.valid?
                    potentially_visible_aabb(found->aabb,planes):
                    potentially_visible(found->bounds,planes));
            if(visible)visible_ids[visible_count++]=item.local_id;
        }
        // Stable output makes the record path's membership checks logarithmic;
        // actual draw order remains the original requirements order.
        std::sort(visible_ids,visible_ids+visible_count);
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
