#include "kami/protocol/simulator_transport.hpp"
#include "kami/protocol/caps.hpp"
#include <windows.h>
#include <algorithm>
#include <cmath>

namespace kami::protocol::sim {
void Circuit::maybe_fetch_material() noexcept {
    if(state_!=CircuitState::MovementConfirmed||texture_capability_url_.empty())return;
    try{
        // Drain every ready UUID independently. One slow CDN response no
        // longer serializes all PBR material fetch/decode work.
        for(std::size_t slot=0U;slot<material_workers_.size();++slot){
            auto& worker=material_workers_[slot];
            if(!worker.valid()||
               worker.wait_for(std::chrono::seconds(0))!=
                   std::future_status::ready)continue;

            const std::string worker_id(material_worker_slots_.id(slot));
            MaterialResult result{};
            try{
                result=worker.get();
            }catch(...){
                result.id=worker_id;
                result.status=0U;
                result.fetched=false;
                result.decoded=false;
            }
            (void)material_worker_slots_.release(slot,worker_id);
            if(result.id.empty())result.id=worker_id;
            if(result.id!=worker_id){
                ++material_failures_;
                continue;
            }

            const bool still_wanted=material_attempts_.contains(result.id);
            const bool terrain=std::find(
                region_metadata_.terrain_assets.begin(),
                region_metadata_.terrain_assets.end(),result.id)!=
                    region_metadata_.terrain_assets.end();
            // Legacy terrain IDs intentionally probe both asset types. Do
            // not replace an object's failure reason with an expected 404.
            if(!terrain||result.decoded){
                material_last_format_=result.format;
                material_last_http_=result.status;
                material_last_bytes_=result.bytes;
                material_last_decode_error_=result.error;
                if(!result.decoded){
                    if(result.fetched)++material_decode_failures_;
                    else ++material_http_failures_;
                }
            }

            // Do not resurrect material data after its object leaves the
            // current camera interest set.
            if(result.decoded&&still_wanted){
                loaded_materials_.insert_or_assign(
                    result.id,std::move(result.material));
                ++spatial_object_revision_;
                texture_scan_revision_=~std::uint64_t{0};
            }else if(still_wanted){
                auto& attempt=material_attempts_.at(result.id);
                if(!terrain||result.status==0U||result.status>=500U)
                    ++material_failures_;
                const bool transient=!result.fetched&&
                    load::Scheduler::retryable_http_status(
                        static_cast<std::uint16_t>(result.status));
                if(!transient){
                    attempt.tries=4U;
                    attempt.next={};
                }else if(attempt.tries<4U){
                    attempt.next=std::chrono::steady_clock::now()+
                        std::chrono::seconds(
                            load::Scheduler::retry_delay_seconds(
                                attempt.tries>0U?attempt.tries-1U:0U));
                }
            }
        }

        const auto scan_now=std::chrono::steady_clock::now();
        if(material_last_scan_!=std::chrono::steady_clock::time_point{}&&
           scan_now-material_last_scan_<
               std::chrono::milliseconds(maximum_world_detail_?50:250))return;
        material_last_scan_=scan_now;

        std::map<std::string,float> wanted;
        const auto scan=[&](const auto& objects){
            for(const auto& [id,t]:objects){
                (void)id;
                std::array<float,3> position{};
                if(!objectspatial::resolve_world_position(
                       t,spatial_objects_,linked_objects_,avatar_root_ids_,
                       position))continue;
                // Preserve the destination's tighter camera-aligned window;
                // concurrency must not expand network demand or cache churn.
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
        scan(spatial_objects_);
        scan(linked_objects_);
        if(region_metadata_.terrain_metadata){
            for(const auto& id:region_metadata_.terrain_assets)
                if(!id.empty())wanted.insert_or_assign(id,0.f);
        }

        std::vector<std::pair<std::string,float>> ranked(
            wanted.begin(),wanted.end());
        std::sort(ranked.begin(),ranked.end(),
            [](const auto& a,const auto& b){
                if(a.second!=b.second)return a.second<b.second;
                return a.first<b.first;
            });
        if(ranked.size()>pbr::kMaxMaterials)
            ranked.resize(pbr::kMaxMaterials);

        std::set<std::string> keep;
        for(const auto& [id,distance]:ranked){
            (void)distance;
            keep.insert(id);
        }
        for(auto it=loaded_materials_.begin();it!=loaded_materials_.end();){
            if(keep.contains(it->first)){++it;continue;}
            it=loaded_materials_.erase(it);
            ++spatial_object_revision_;
        }
        for(auto it=material_attempts_.begin();it!=material_attempts_.end();){
            if(!keep.contains(it->first))it=material_attempts_.erase(it);
            else ++it;
        }

        const auto now=std::chrono::steady_clock::now();
        // Fill both bounded slots in nearest-first order. UUID reservations
        // prevent duplicate work while an earlier request is still running.
        for(const auto& [id,distance]:ranked){
            (void)distance;
            if(loaded_materials_.contains(id)||
               material_worker_slots_.contains(id))continue;
            auto& attempt=material_attempts_[id];
            if(attempt.tries>=4U||now<attempt.next)continue;

            const auto slot=material_worker_slots_.reserve(id);
            if(!slot)continue;
            auto& worker=material_workers_[*slot];
            if(worker.valid()){
                (void)material_worker_slots_.release(*slot,id);
                continue;
            }

            ++attempt.tries;
            try{
                auto url=texture_capability_url_;
                worker=std::async(std::launch::async,
                    [id,url=std::move(url)]() mutable {
                        MaterialResult out{};
                        out.id=id;
                        struct CapabilityWiper {
                            std::wstring& value;
                            ~CapabilityWiper() noexcept {
                                if(!value.empty())
                                    SecureZeroMemory(value.data(),
                                        value.size()*sizeof(wchar_t));
                            }
                        } wipe{url};
                        try{
                            const auto response=caps::fetch_material_asset(url,id);
                            out.status=response.status;
                            out.fetched=response.fetched;
                            out.bytes=response.bytes.size();
                            out.format=pbr::asset_format(response.bytes);
                            out.decoded=response.fetched&&
                                pbr::decode_asset(response.bytes,out.material,
                                                  &out.error);
                        }catch(...){
                            out.status=0U;
                            out.fetched=false;
                            out.bytes=0U;
                            out.format=pbr::AssetFormat::Unknown;
                            out.error=pbr::DecodeError::None;
                            out.decoded=false;
                        }
                        return out;
                    });
            }catch(...){
                (void)material_worker_slots_.release(*slot,id);
                if(attempt.tries<4U){
                    attempt.next=std::chrono::steady_clock::now()+
                        std::chrono::seconds(
                            load::Scheduler::retry_delay_seconds(
                                attempt.tries>0U?attempt.tries-1U:0U));
                }
                ++material_failures_;
            }
        }
    }catch(...){
        ++material_failures_;
    }
}
}
