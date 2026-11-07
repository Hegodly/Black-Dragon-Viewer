#include "kami/protocol/simulator_transport.hpp"
#include "kami/protocol/caps.hpp"
#include "kami/protocol/texture_decode.hpp"
#include <windows.h>
#include <algorithm>
#include <cmath>
#include <future>
#include <string_view>
#include <utility>

namespace kami::protocol::sim {
bool Circuit::set_texture_capability(std::wstring capability) noexcept {
    if(!caps::trusted_mesh_caps_url(capability)||
       !texture_capability_url_.empty()||
       state_!=CircuitState::MovementConfirmed)return false;
    texture_capability_url_=std::move(capability);
    texture_scan_revision_=~std::uint64_t{0};
    texture_last_scan_position_valid_=false;
    return true;
}
void Circuit::prune_decoded_textures() noexcept {
    // R48 retained every image referenced anywhere within 176 m. Once
    // 24 valid textures filled that cache, KAMI stopped downloading new
    // nearby faces even when the avatar moved to a different building.
    // Keep only the nearest bounded requested IDs, never stale far-away
    // images. The DX12 array still uses a bounded number of GPU slices.
    for(auto it=decoded_textures_.begin();it!=decoded_textures_.end();){
        if(texture_desired_ids_.contains(it->first)){++it;continue;}
        const auto bytes=it->second.rgba.size();
        decoded_texture_bytes_=
            bytes>decoded_texture_bytes_?0U:decoded_texture_bytes_-bytes;
        (void)texture_scheduler_.forget_finished(it->first);
        it=decoded_textures_.erase(it);
        ++spatial_object_revision_; // update GPU atlas and scene together
    }
}
void Circuit::schedule_visible_textures() noexcept {
    if(texture_capability_url_.empty()||!movement_position_.verified)return;
    const bool moved=!texture_last_scan_position_valid_||
        std::hypot(std::hypot(
            movement_position_.x-texture_last_scan_x_,
            movement_position_.y-texture_last_scan_y_),
            movement_position_.z-texture_last_scan_z_)>=12.f;
    if(texture_scan_revision_==spatial_render_revision()&&!moved)return;
    try{
        // Aggregate nearest visible demand PER UUID across all root/linkset
        // objects. The old scan requested every ID until the cache filled.
        std::map<std::string,float> nearest;
        const auto record=[&](const std::string& id,float distance){
            if(id.empty())return;
            const auto [it,inserted]=nearest.try_emplace(id,distance);
            if(!inserted)it->second=std::min(it->second,distance);
        };
        std::map<std::string,std::uint32_t> drawable_faces;
        const auto scan=[&](const auto& objects){
            for(const auto& [id,t]:objects){
                (void)id;
                // R53: both decoded mesh UV faces and supported
                // simple primitive surfaces can now consume JPEG2000.
                // Unknown cuts/sculpts remain intentionally unrendered.
                if(t.mesh_asset_id.empty()&&
                   t.primitive==objectspatial::PrimitiveKind::Unknown)continue;
                std::array<float,3> position{};
                if(!objectspatial::resolve_world_position(
                        t,spatial_objects_,linked_objects_,avatar_root_ids_,
                        position)|| 
                   std::abs(position[0]-movement_position_.x)>144.f||
                   std::abs(position[1]-movement_position_.y)>144.f||
                   std::abs(position[2]-movement_position_.z)>96.f)continue;
                const float distance=std::hypot(std::hypot(
                    position[0]-movement_position_.x,
                    position[1]-movement_position_.y),
                    position[2]-movement_position_.z);
                std::uint32_t mask=0U;
                if(!t.mesh_asset_id.empty()){
                    const auto loaded=loaded_meshes_.find(t.mesh_asset_id);
                    if(loaded==loaded_meshes_.end())continue;
                    const auto [it,inserted]=drawable_faces.try_emplace(
                        t.mesh_asset_id,0U);
                    if(inserted){
                        for(const auto& triangle:loaded->second.triangles)
                            if(triangle.has_uv&&triangle.face_index<32U)
                                it->second|=std::uint32_t{1}<<triangle.face_index;
                    }
                    mask=it->second;
                }else{
                    const unsigned faces=
                        t.primitive==objectspatial::PrimitiveKind::Box?6U:
                        t.primitive==objectspatial::PrimitiveKind::Cylinder?3U:
                        t.primitive==objectspatial::PrimitiveKind::Prism?5U:1U;
                    mask=(std::uint32_t{1}<<faces)-1U;
                }
                // Match GPU material demand: missing meshes, faces without
                // UVs and overridden defaults must not occupy the cache.
                for(unsigned face=0U;face<32U;++face)
                    if(mask&(std::uint32_t{1}<<face))
                    {
                        pbr::Material material;
                        if(pbr::effective(t.face_material_ids[face],t.local_id,face,
                            loaded_materials_,material_overrides_,material)){
                            for(const auto& image:material.images)record(image,distance);
                        }else record(t.face_texture_override[face]?
                            t.face_texture_asset_ids[face]:t.default_texture_asset_id,distance);
                    }
            }
        };
        scan(spatial_objects_);
        scan(linked_objects_);
        if(region_metadata_.terrain_metadata)for(const auto& id:region_metadata_.terrain_assets){
            pbr::Material material;
            if(pbr::effective(id,0U,0U,loaded_materials_,{},material)){
                for(const auto& image:material.images)record(image,0.f);
            }else record(id,0.f);
        }
        std::vector<std::pair<std::string,float>> ranked(
            nearest.begin(),nearest.end());
        std::sort(ranked.begin(),ranked.end(),
            [](const auto& a,const auto& b){
                if(a.second!=b.second)return a.second<b.second;
                return a.first<b.first;
            });
        if(ranked.size()>kMaxDecodedTextureAssets)
            ranked.resize(kMaxDecodedTextureAssets);
        std::set<std::string> wanted;
        for(const auto& [uuid,distance]:ranked){
            (void)distance;
            wanted.insert(uuid);
        }
        // Cancel stale queued requests where safe. In-progress HTTP
        // workers are never cancelled mid-packet, keeping futures sound.
        for(const auto& old:texture_desired_ids_)
            if(!wanted.contains(old)){
                (void)texture_scheduler_.forget_finished(old);
                texture_retry_after_.erase(old);
            }
        texture_desired_ids_.swap(wanted);
        prune_decoded_textures();
        // Record revision AFTER pruning, which can increment it.
        texture_scan_revision_=spatial_render_revision();
        texture_last_scan_x_=movement_position_.x;
        texture_last_scan_y_=movement_position_.y;
        texture_last_scan_z_=movement_position_.z;
        texture_last_scan_position_valid_=true;
        for(const auto& [uuid,distance]:ranked)
            (void)texture_scheduler_.request(
                uuid,load::Kind::VisibleTexture,distance,true,0U);
    }catch(...){
        // Out-of-memory should skip one interest-list update, never crash
        // the simulator poll loop. A later spatial/position update retries.
        texture_scan_revision_=~std::uint64_t{0};
    }
}
void Circuit::maybe_fetch_texture() noexcept {
    schedule_visible_textures();
    if(state_!=CircuitState::MovementConfirmed)return;
    if(texture_worker_.valid()){
        if(texture_worker_.wait_for(std::chrono::seconds(0))!=
           std::future_status::ready)return;
        try {
            auto result=texture_worker_.get();
            const bool accepted=texture_scheduler_.download_finished(
                result.id,result.bytes,result.fetched);
            // A request may finish after the camera's interest set moved.
            // Do not let obsolete data consume the new neighborhood's budget.
            if(!texture_desired_ids_.contains(result.id)){
                (void)texture_scheduler_.forget_finished(result.id);
                texture_retry_after_.erase(result.id);
            }else if(result.fetched&&accepted){
                ++texture_download_count_;
                // The async decoder has already completed this exact UUID.
                // Never pop an unrelated decode job by scheduler priority.
                if(texture_scheduler_.begin_decode(result.id)){
                    const auto old=decoded_textures_.find(result.id);
                    const std::size_t prior=old==decoded_textures_.end()?0U:
                        old->second.rgba.size();
                    const auto used=decoded_texture_bytes_-
                        std::min(prior,decoded_texture_bytes_);
                    const auto bytes=result.image.rgba.size();
                    const bool fits=(old!=decoded_textures_.end()||
                            decoded_textures_.size()<kMaxDecodedTextureAssets)&&
                        used<=kMaxDecodedTextureBytes&&
                        bytes<=kMaxDecodedTextureBytes-used;
                    const bool good=result.decoded&&fits;
                    (void)texture_scheduler_.decode_finished(result.id,good);
                    if(good){
                        decoded_texture_bytes_=used+bytes;
                        decoded_textures_.insert_or_assign(
                            result.id,std::move(result.image));
                        // Notify the 800 ms coalesced DX12 region uploader:
                        // a new J2C image must also rebuild the texture atlas.
                        ++spatial_object_revision_;
                    }else if(result.decoded&&!fits){
                        // Cache pressure is not corrupt JPEG2000. Release
                        // the finished scheduler entry and retry discovery
                        // after eviction instead of permanently hiding it.
                        (void)texture_scheduler_.forget_finished(result.id);
                        texture_scan_revision_=~std::uint64_t{0};
                    }else ++texture_failure_count_;
                }
            }else{
                ++texture_failure_count_;
                const auto status=result.http_status;
                const bool transient=status==0U||status==429U||
                    status==500U||status==502U||status==503U||status==504U;
                const auto* job=texture_scheduler_.find(result.id);
                if(accepted&&transient&&job&&
                   job->retry_count<load::Scheduler::kMaxTransientRetries&&
                   texture_retry_after_.size()<kMaxDecodedTextureAssets){
                    const auto delay=std::chrono::seconds(
                        1U<<std::min(job->retry_count,3U));
                    texture_retry_after_.insert_or_assign(
                        result.id,std::chrono::steady_clock::now()+delay);
                }
            }
        }catch(...){++texture_failure_count_;}
    }
    const auto retry_now=std::chrono::steady_clock::now();
    for(auto it=texture_retry_after_.begin();it!=texture_retry_after_.end();){
        if(!texture_desired_ids_.contains(it->first)){
            it=texture_retry_after_.erase(it);
        }else if(retry_now>=it->second){
            (void)texture_scheduler_.retry_failed(it->first);
            it=texture_retry_after_.erase(it);
        }else ++it;
    }
    if(texture_worker_.valid()||texture_capability_url_.empty())return;
    const auto now=std::chrono::steady_clock::now();
    if(texture_last_start_!=std::chrono::steady_clock::time_point{}&&
       now-texture_last_start_<std::chrono::milliseconds(maximum_world_detail_?35:250))
        return;
    if(decoded_textures_.size()>=kMaxDecodedTextureAssets||
       decoded_texture_bytes_>=kMaxDecodedTextureBytes)return;
    auto next=texture_scheduler_.next_download();
    if(!next)return;
    texture_last_start_=now;
    auto url=texture_capability_url_;
    try {
        texture_worker_=std::async(std::launch::async,
            [url=std::move(url),id=next->id]()mutable {
                TextureResult result{};
                result.id=id;
                const auto reply=caps::fetch_texture_asset(url,id);
                if(!url.empty())
                    SecureZeroMemory(url.data(),url.size()*sizeof(wchar_t));
                result.http_status=reply.status;
                result.fetched=reply.fetched;
                result.bytes=static_cast<std::uint32_t>(reply.bytes.size());
                if(reply.fetched){
                    result.decoded=texture::decode_jpeg2000(
                        reply.bytes,result.image);
                    if(result.decoded)
                        result.decoded=texture::downscale_preview(
                            result.image,texture::kPreviewExtent);
                }
                return result;
            });
    }catch(...){
        // std::async may fail under thread or memory pressure. Treat that
        // local launch failure like a transient fetch failure, otherwise
        // this texture remains Failed forever and cannot re-enter residency.
        const auto delay=texture_scheduler_.worker_launch_failed(next->id);
        if(delay&&texture_retry_after_.size()<kMaxDecodedTextureAssets)
            texture_retry_after_.insert_or_assign(
                next->id,std::chrono::steady_clock::now()+
                std::chrono::seconds(*delay));
        ++texture_failure_count_;
    }
}
} // namespace kami::protocol::sim

