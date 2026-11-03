#include "kami/features/viewer_foundation.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace kami::viewer {
namespace {
constexpr std::array<FeatureStatus,21> catalog{{
 {Feature::Authentication,L"Second Life account login",Availability::NetworkIntegrationRequired},
 {Feature::Simulator,L"Region and movement",Availability::NetworkIntegrationRequired},
 {Feature::Assets,L"Mesh, texture and animation streaming",Availability::AssetPipelineRequired},
 {Feature::AvatarAppearance,L"Avatar appearance and wearables",Availability::AssetPipelineRequired},
 {Feature::Inventory,L"Inventory and folders",Availability::NetworkIntegrationRequired},
 {Feature::NearbyChat,L"Nearby chat",Availability::NetworkIntegrationRequired},
 {Feature::DirectMessages,L"Instant messages",Availability::NetworkIntegrationRequired},
 {Feature::Friends,L"Friends and presence",Availability::NetworkIntegrationRequired},
 {Feature::Groups,L"Group messaging and roles",Availability::NetworkIntegrationRequired},
 {Feature::WorldMap,L"World map and places",Availability::NetworkIntegrationRequired},
 {Feature::Teleport,L"Teleport request execution",Availability::NetworkIntegrationRequired},
 {Feature::Parcels,L"Land and parcel settings",Availability::NetworkIntegrationRequired},
 {Feature::Building,L"World build/edit operations",Availability::NetworkIntegrationRequired},
 {Feature::Gestures,L"Gestures and animations",Availability::AssetPipelineRequired},
 {Feature::Camera,L"KAMI native camera",Availability::OriginalLocalReady},
 {Feature::CreatorStudio,L"KAMI Creator Studio local OBJ inspection",Availability::OriginalLocalReady},
 {Feature::PhotoStudio,L"KAMI Photo Studio native DX12 capture",Availability::OriginalLocalReady},
 {Feature::SmartSort,L"Inventory Smart Sort preview",Availability::OriginalLocalReady},
 {Feature::OutfitDrafts,L"KAMI offline outfit planner",Availability::OriginalLocalReady},
 {Feature::LandmarkDrafts,L"KAMI local landmark notebook",Availability::OriginalLocalReady},
 {Feature::SceneBudgetAdvisor,L"KAMI geometry and texture memory estimate",Availability::OriginalLocalReady}
}};
}
const std::array<FeatureStatus,21>& feature_catalog() noexcept {return catalog;}
Availability status(Feature feature) noexcept {
    for(const auto& item:catalog)if(item.feature==feature)return item.availability;
    return Availability::NetworkIntegrationRequired;
}
bool OutfitDraft::add(std::string asset_id,std::wstring slot) {
    if(asset_id.empty()||asset_id.size()>128||slot.empty()||slot.size()>96||
       entries_.size()>=256)return false;
    if(std::find_if(entries_.begin(),entries_.end(),[&](const auto& entry){
        return entry.asset_id==asset_id;
    })!=entries_.end())return false;
    entries_.push_back({std::move(asset_id),std::move(slot)});
    return true;
}
bool OutfitDraft::remove(const std::string& asset_id) {
    const auto it=std::remove_if(entries_.begin(),entries_.end(),[&](const auto& entry){
        return entry.asset_id==asset_id;
    });
    if(it==entries_.end())return false;
    entries_.erase(it,entries_.end());
    return true;
}
bool LandmarkLibrary::add(LandmarkDraft value) {
    if(value.label.empty()||value.label.size()>128||value.region.empty()||
       value.region.size()>128||entries_.size()>=512||
       !std::isfinite(value.local_x)||!std::isfinite(value.local_y)||
       !std::isfinite(value.local_z)||value.local_x<0.f||value.local_x>=256.f||
       value.local_y<0.f||value.local_y>=256.f)return false;
    if(std::find_if(entries_.begin(),entries_.end(),[&](const auto& item){
        return item.label==value.label && item.region==value.region;
    })!=entries_.end())return false;
    entries_.push_back(std::move(value));
    return true;
}
SceneEstimate estimate_scene_cost(unsigned width,unsigned height,unsigned textures,
                                  std::uint64_t triangles,unsigned gpu_budget_mb) noexcept {
    constexpr std::uint64_t kMB=1024ULL*1024ULL;
    const std::uint64_t a=static_cast<std::uint64_t>(width)*height;
    const std::uint64_t raw=a*4ULL;
    const std::uint64_t texture=raw>0&&textures<=
        std::numeric_limits<std::uint64_t>::max()/raw ?
        raw*textures:std::numeric_limits<std::uint64_t>::max();
    // KAMI Vertex is 52 bytes currently; indices/materials are not charged here.
    const std::uint64_t vertex=triangles<=std::numeric_limits<std::uint64_t>::max()/156ULL?
        triangles*156ULL:std::numeric_limits<std::uint64_t>::max();
    const std::uint64_t total=texture>std::numeric_limits<std::uint64_t>::max()-vertex ?
        std::numeric_limits<std::uint64_t>::max():texture+vertex;
    return {texture,vertex,total/kMB,
            gpu_budget_mb!=0 && total>static_cast<std::uint64_t>(gpu_budget_mb)*kMB};
}
} // namespace kami::viewer
