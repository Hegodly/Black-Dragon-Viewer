#pragma once
#include <algorithm>
#include <cstddef>
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
class MeshResidencyPlanner {
public:
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
            (void)id;used+=asset.decoded_bytes;
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
