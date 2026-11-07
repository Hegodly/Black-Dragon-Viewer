#include "kami/protocol/simulator_transport.hpp"
#include "kami/protocol/caps.hpp"
#include <windows.h>
#include <algorithm>
#include <cmath>
namespace kami::protocol::sim {
void Circuit::maybe_fetch_material() noexcept {
    if(state_!=CircuitState::MovementConfirmed||texture_capability_url_.empty())return;
    try{
        if(material_worker_.valid()){
            if(material_worker_.wait_for(std::chrono::seconds(0))!=std::future_status::ready)return;
            auto result=material_worker_.get();
            const bool still_wanted=material_attempts_.contains(result.id);
            const bool terrain=std::find(region_metadata_.terrain_assets.begin(),region_metadata_.terrain_assets.end(),result.id)!=region_metadata_.terrain_assets.end();
            // Legacy terrain IDs intentionally probe both asset types. Do
            // not replace an object's failure reason with an expected 404.
            if(!terrain||result.decoded){
                material_last_format_=result.format;material_last_http_=result.status;material_last_bytes_=result.bytes;
                material_last_decode_error_=result.error;
                if(!result.decoded){
                    if(result.fetched)++material_decode_failures_;else ++material_http_failures_;
                }
            }
            // An HTTP result can arrive after the region/object interest set
            // changed. Do not resurrect a stale PBR material or force a
            // needless texture-atlas rebuild for an obsolete object.
            if(result.decoded&&still_wanted){
                loaded_materials_.insert_or_assign(result.id,std::move(result.material));
                ++spatial_object_revision_;texture_scan_revision_=~std::uint64_t{0};
            }else if(still_wanted){
                auto& attempt=material_attempts_.at(result.id);
                if(!terrain||result.status==0U||result.status>=500U)++material_failures_;
                const auto s=result.status;
                const bool transient=!result.fetched&&(s==0U||s==408U||s==429U||s==500U||s==502U||s==503U||s==504U);
                if(!transient)attempt.tries=4U;
                attempt.next=std::chrono::steady_clock::now()+std::chrono::seconds(1U<<std::min(attempt.tries,3U));
            }
        }
        const auto scan_now=std::chrono::steady_clock::now();
        if(material_last_scan_!=std::chrono::steady_clock::time_point{}&&
           scan_now-material_last_scan_<std::chrono::milliseconds(maximum_world_detail_?50:250))return;
        material_last_scan_=scan_now;
        std::map<std::string,float> wanted;
        const auto scan=[&](const auto& objects){
            for(const auto& [id,t]:objects){
                (void)id;
                std::array<float,3> position{};
                if(!objectspatial::resolve_world_position(
                        t,spatial_objects_,linked_objects_,avatar_root_ids_,
                        position))continue;
                // Keep PBR material demand aligned with the same visible
                // object interest window used by mesh/texture discovery.
                // A distant building must not displace a nearby material
                // from the bounded material cache.
                const float dx=position[0]-movement_position_.x;
                const float dy=position[1]-movement_position_.y;
                const float dz=position[2]-movement_position_.z;
                if(!std::isfinite(dx)||!std::isfinite(dy)||
                   !std::isfinite(dz)||std::abs(dx)>144.f||
                   std::abs(dy)>144.f||std::abs(dz)>128.f)continue;
                const float distance=std::hypot(std::hypot(dx,dy),dz);
                if(!std::isfinite(distance))continue;
                for(const auto& material:t.face_material_ids){
                    if(material.empty())continue;
                    const auto [it,inserted]=wanted.try_emplace(material,distance);
                    if(!inserted)it->second=std::min(it->second,distance);
                }
            }
        };
        scan(spatial_objects_);scan(linked_objects_);
        if(region_metadata_.terrain_metadata)for(const auto& id:region_metadata_.terrain_assets)
            if(!id.empty())wanted.insert_or_assign(id,0.f);
        std::vector<std::pair<std::string,float>> ranked(wanted.begin(),wanted.end());
        std::sort(ranked.begin(),ranked.end(),[](const auto& a,const auto& b){return a.second<b.second;});
        if(ranked.size()>pbr::kMaxMaterials)ranked.resize(pbr::kMaxMaterials);
        std::set<std::string> keep;for(const auto& [id,distance]:ranked){(void)distance;keep.insert(id);}
        for(auto it=loaded_materials_.begin();it!=loaded_materials_.end();){
            if(keep.contains(it->first)){++it;continue;}
            it=loaded_materials_.erase(it);++spatial_object_revision_;
        }
        for(auto it=material_attempts_.begin();it!=material_attempts_.end();)
            if(!keep.contains(it->first))it=material_attempts_.erase(it);else ++it;
        const auto now=std::chrono::steady_clock::now();
        for(const auto& [id,distance]:ranked){
            (void)distance;if(loaded_materials_.contains(id))continue;
            auto& attempt=material_attempts_[id];
            if(attempt.tries>=4U||now<attempt.next)continue;
            ++attempt.tries;auto url=texture_capability_url_;
            material_worker_=std::async(std::launch::async,[id,url=std::move(url)]() mutable {
                MaterialResult out;out.id=id;
                const auto response=caps::fetch_material_asset(url,id);
                if(!url.empty())SecureZeroMemory(url.data(),url.size()*sizeof(wchar_t));
                out.status=response.status;out.fetched=response.fetched;out.bytes=response.bytes.size();out.format=pbr::asset_format(response.bytes);
                out.decoded=response.fetched&&pbr::decode_asset(response.bytes,out.material,&out.error);return out;
            });
            break;
        }
    }catch(...){++material_failures_;}
}
}
