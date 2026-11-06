#pragma once
#include <algorithm>
#include <cstddef>
#include <cmath>
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

namespace kami::world {
// CPU-side residency planner. This is not yet a DX12 GPU allocation cache.
// Instances reference shared asset identities; no per-instance asset duplication.
struct ResidentMeshAsset {
    std::size_t triangles=0;
    std::size_t decoded_bytes=0;
    std::uint64_t last_seen_generation=0;
};
struct ResidentMeshInstance {
    std::uint32_t local_id=0;
    std::uint32_t parent_id=0;
    std::string asset_id;
};
// Deterministic per-instance draw admission for the upcoming persistent
// DX12 per-asset path. A candidate references an asset once, not a copied
// triangle stream. The caller supplies camera-relative squared distance.
struct MeshDrawCandidate {
    std::uint32_t local_id=0;
    std::string asset_id;
    float distance_squared=0.f;
};
struct MeshDrawSchedule {
    std::vector<MeshDrawCandidate> admitted;
    std::size_t triangles=0;
    std::size_t deferred=0;
};
class MeshResidencyPlanner {
public:
    // Never split a mesh mid-object: partial draws cause missing walls.
    // Stable distance order prevents frame-to-frame draw-list flicker.
    [[nodiscard]] MeshDrawSchedule schedule_draws(
        std::vector<MeshDrawCandidate> candidates,
        std::size_t triangle_budget) const {
        // NaN and infinity cannot participate in a strict weak ordering.
        // Reject corrupt camera distances before sorting the draw list.
        const auto invalid=std::remove_if(candidates.begin(),candidates.end(),
            [](const auto& candidate){
                return !std::isfinite(candidate.distance_squared)||
                       candidate.distance_squared<0.f;
            });
        const std::size_t invalid_count=static_cast<std::size_t>(
            std::distance(invalid,candidates.end()));
        candidates.erase(invalid,candidates.end());
        std::sort(candidates.begin(),candidates.end(),
            [](const auto& a,const auto& b){
                if(a.distance_squared!=b.distance_squared)
                    return a.distance_squared<b.distance_squared;
                return a.local_id<b.local_id;
            });
        MeshDrawSchedule result;
        result.deferred=invalid_count;
        std::set<std::uint32_t> scheduled;
        for(const auto& candidate:candidates){
            // Claim an instance before asset validation: duplicate candidates
            // must not consume the draw budget or cause duplicate geometry.
            if(candidate.local_id==0||
               !scheduled.insert(candidate.local_id).second){
                ++result.deferred;
                continue;
            }
            const auto instance=instances_.find(candidate.local_id);
            const auto asset=assets_.find(candidate.asset_id);
            if(candidate.asset_id.empty()||
               instance==instances_.end()||
               instance->second.asset_id!=candidate.asset_id||
               asset==assets_.end()||asset->second.triangles==0){
                ++result.deferred;
                continue;
            }
            const auto count=asset->second.triangles;
            if(count>triangle_budget-result.triangles){
                ++result.deferred;
                continue;
            }
            result.admitted.push_back(candidate);
            result.triangles+=count;
        }
        return result;
    }
    void begin_generation() noexcept { ++generation_; }
    void see_asset(const std::string& id,std::size_t triangles,
                   std::size_t bytes) {
        if(id.empty())return;
        auto& asset=assets_[id];
        asset.triangles=triangles;
        asset.decoded_bytes=bytes;
        asset.last_seen_generation=generation_;
    }
    void see_instance(std::uint32_t local_id,std::uint32_t parent_id,
                      const std::string& asset_id) {
        if(local_id==0||asset_id.empty())return;
        instances_[local_id]={local_id,parent_id,asset_id};
        seen_instances_.insert(local_id);
    }
    void end_generation() {
        for(auto it=instances_.begin();it!=instances_.end();){
            if(!seen_instances_.contains(it->first))it=instances_.erase(it);
            else ++it;
        }
        seen_instances_.clear();
    }
    // Region transition drops stale simulator-local IDs; decoded asset
    // metadata remains reusable across regions until explicitly evicted.
    void clear_instances() noexcept {
        instances_.clear();seen_instances_.clear();
    }
    void evict_unreferenced(std::size_t max_decoded_bytes) {
        std::set<std::string> referenced;
        for(const auto& [id,instance]:instances_){
            (void)id;referenced.insert(instance.asset_id);
        }
        std::size_t used=0;
        for(const auto& [id,asset]:assets_){
            (void)id;
            // Saturate rather than wrap on corrupt or oversized cache metadata.
            if(asset.decoded_bytes>static_cast<std::size_t>(-1)-used)
                used=static_cast<std::size_t>(-1);
            else used+=asset.decoded_bytes;
        }
        while(used>max_decoded_bytes){
            auto oldest=assets_.end();
            for(auto it=assets_.begin();it!=assets_.end();++it){
                if(referenced.contains(it->first))continue;
                if(oldest==assets_.end()||
                   it->second.last_seen_generation<
                   oldest->second.last_seen_generation)oldest=it;
            }
            if(oldest==assets_.end())break; // live references are pinned
            used-=oldest->second.decoded_bytes;
            assets_.erase(oldest);
        }
    }
    [[nodiscard]] std::size_t asset_count() const noexcept {return assets_.size();}
    [[nodiscard]] std::size_t instance_count() const noexcept {return instances_.size();}
    [[nodiscard]] std::size_t referenced_asset_count() const {
        std::set<std::string> ids;
        for(const auto& [id,instance]:instances_){
            (void)id;ids.insert(instance.asset_id);
        }
        return ids.size();
    }
private:
    std::uint64_t generation_=0;
    std::map<std::string,ResidentMeshAsset> assets_;
    std::map<std::uint32_t,ResidentMeshInstance> instances_;
    std::set<std::uint32_t> seen_instances_;
};
} // namespace kami::world
