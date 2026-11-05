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
    if(decoded_textures_.empty())return;
    const auto within_retention=[&](const objectspatial::Transform& t){
        return std::isfinite(t.x)&&std::isfinite(t.y)&&std::isfinite(t.z)&&
            std::abs(t.x-movement_position_.x)<=176.f&&
            std::abs(t.y-movement_position_.y)<=176.f&&
            std::abs(t.z-movement_position_.z)<=128.f;
    };
    const auto used=[&](std::string_view asset){
        const auto check=[&](const auto& objects){
            for(const auto& [id,t]:objects){
                (void)id;
                if(t.default_texture_asset_id!=asset)continue;
                const auto* anchor=world_mesh_anchor(t);
                if(anchor&&within_retention(*anchor))return true;
            }
            return false;
        };
        return check(spatial_objects_)||check(linked_objects_);
    };
    for(auto it=decoded_textures_.begin();it!=decoded_textures_.end();){
        if(used(it->first)){++it;continue;}
        const auto bytes=it->second.rgba.size();
        decoded_texture_bytes_=
            bytes>decoded_texture_bytes_?0U:decoded_texture_bytes_-bytes;
        (void)texture_scheduler_.forget_finished(it->first);
        it=decoded_textures_.erase(it);
    }
}
void Circuit::schedule_visible_textures() noexcept {
    if(texture_capability_url_.empty()||!movement_position_.verified)return;
    const bool moved=!texture_last_scan_position_valid_||
        std::hypot(std::hypot(
            movement_position_.x-texture_last_scan_x_,
            movement_position_.y-texture_last_scan_y_),
            movement_position_.z-texture_last_scan_z_)>=12.f;
    if(texture_scan_revision_==spatial_object_revision_&&!moved)return;
    prune_decoded_textures();
    texture_scan_revision_=spatial_object_revision_;
    texture_last_scan_x_=movement_position_.x;
    texture_last_scan_y_=movement_position_.y;
    texture_last_scan_z_=movement_position_.z;
    texture_last_scan_position_valid_=true;
    const auto near_enough=[&](const objectspatial::Transform& t) {
        return std::isfinite(t.x)&&std::isfinite(t.y)&&std::isfinite(t.z)&&
            std::abs(t.x-movement_position_.x)<=144.f&&
            std::abs(t.y-movement_position_.y)<=144.f&&
            std::abs(t.z-movement_position_.z)<=96.f;
    };
    const auto schedule=[&](const auto& objects){
        for(const auto& [id,t]:objects){
            (void)id;
            if(t.default_texture_asset_id.empty())continue;
            const auto* anchor=world_mesh_anchor(t);
            if(!anchor||!near_enough(*anchor))continue;
            const float distance=std::hypot(std::hypot(
                anchor->x-movement_position_.x,
                anchor->y-movement_position_.y),
                anchor->z-movement_position_.z);
            (void)texture_scheduler_.request(
                t.default_texture_asset_id,load::Kind::VisibleTexture,
                distance,true,0U);
        }
    };
    schedule(spatial_objects_);
    schedule(linked_objects_);
}
void Circuit::maybe_fetch_texture() noexcept {
    schedule_visible_textures();
    if(state_!=CircuitState::MovementConfirmed)return;
    if(texture_worker_.valid()){
        if(texture_worker_.wait_for(std::chrono::seconds(0))!=
           std::future_status::ready)return;
        try {
            auto result=texture_worker_.get();
            (void)texture_scheduler_.download_finished(
                result.id,result.bytes,result.fetched);
            if(result.fetched){
                ++texture_download_count_;
                const auto selected=texture_scheduler_.next_decode();
                if(selected&&selected->id==result.id){
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
                    }else ++texture_failure_count_;
                }
            }else{
                ++texture_failure_count_;
            }
        }catch(...){++texture_failure_count_;}
    }
    if(texture_worker_.valid()||texture_capability_url_.empty())return;
    const auto now=std::chrono::steady_clock::now();
    if(texture_last_start_!=std::chrono::steady_clock::time_point{}&&
       now-texture_last_start_<std::chrono::milliseconds(250))
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
                if(reply.fetched)
                    result.decoded=texture::decode_jpeg2000(
                        reply.bytes,result.image);
                return result;
            });
    }catch(...){
        (void)texture_scheduler_.download_finished(next->id,0U,false);
        ++texture_failure_count_;
    }
}
} // namespace kami::protocol::sim
