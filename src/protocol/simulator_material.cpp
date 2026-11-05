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
            auto& attempt=material_attempts_[result.id];
            if(result.decoded){
                loaded_materials_.insert_or_assign(result.id,std::move(result.material));
                ++spatial_object_revision_;texture_scan_revision_=~std::uint64_t{0};
            }else{
                const bool terrain=std::find(region_metadata_.terrain_assets.begin(),region_metadata_.terrain_assets.end(),result.id)!=region_metadata_.terrain_assets.end();
                if(!terrain||result.status==0U||result.status>=500U)++material_failures_;
                const auto s=result.status;
                const bool transient=!result.fetched&&(s==0U||s==408U||s==429U||s==500U||s==502U||s==503U||s==504U);
                if(!transient)attempt.tries=4U;
                attempt.next=std::chrono::steady_clock::now()+std::chrono::seconds(1U<<std::min(attempt.tries,3U));
            }
        }
        const auto scan_now=std::chrono::steady_clock::now();
        if(material_last_scan_!=std::chrono::steady_clock::time_point{}&&
           scan_now-material_last_scan_<std::chrono::milliseconds(250))return;
        material_last_scan_=scan_now;
        std::map<std::string,float> wanted;
        const auto scan=[&](const auto& objects){
            for(const auto& [id,t]:objects){
                (void)id;const auto* anchor=world_mesh_anchor(t);if(!anchor)continue;
                const float distance=std::hypot(std::hypot(anchor->x-movement_position_.x,
                    anchor->y-movement_position_.y),anchor->z-movement_position_.z);
                if(!std::isfinite(distance)||distance>220.f)continue;
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
                out.status=response.status;out.fetched=response.fetched;
                out.decoded=response.fetched&&pbr::decode_asset(response.bytes,out.material);return out;
            });
            break;
        }
    }catch(...){++material_failures_;}
}
}
