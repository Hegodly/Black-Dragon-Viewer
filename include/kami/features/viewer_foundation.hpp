#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace kami::viewer {
enum class Feature : std::uint8_t {
    Authentication, Simulator, Assets, AvatarAppearance,
    Inventory, NearbyChat, DirectMessages, Friends, Groups, WorldMap,
    Teleport, Parcels, Building, Gestures,
    Camera, CreatorStudio, PhotoStudio, SmartSort,
    OutfitDrafts, LandmarkDrafts, SceneBudgetAdvisor
};
enum class Availability : std::uint8_t {
    OriginalLocalReady, NetworkIntegrationRequired, AssetPipelineRequired
};
struct FeatureStatus {
    Feature feature;
    const wchar_t* label;
    Availability availability;
};
[[nodiscard]] const std::array<FeatureStatus,21>& feature_catalog() noexcept;
[[nodiscard]] Availability status(Feature feature) noexcept;

// Independently authored, offline first outfit staging. An outfit is just a
// user's selected *references*, never an inventory mutation or avatar wear.
struct OutfitEntry {
    std::string asset_id;
    std::wstring slot;
};
class OutfitDraft final {
public:
    [[nodiscard]] bool add(std::string asset_id,std::wstring slot);
    [[nodiscard]] bool remove(const std::string& asset_id);
    [[nodiscard]] std::size_t size() const noexcept {return entries_.size();}
    [[nodiscard]] const std::vector<OutfitEntry>& entries() const noexcept {
        return entries_;
    }
private:
    std::vector<OutfitEntry> entries_;
};

struct LandmarkDraft {
    std::wstring label;
    std::wstring region;
    float local_x=0.f,local_y=0.f,local_z=0.f;
};
class LandmarkLibrary final {
public:
    [[nodiscard]] bool add(LandmarkDraft value);
    [[nodiscard]] const std::vector<LandmarkDraft>& entries() const noexcept {
        return entries_;
    }
private:
    std::vector<LandmarkDraft> entries_;
};

struct SceneEstimate {
    std::uint64_t texture_bytes=0;
    std::uint64_t geometry_bytes=0;
    std::uint64_t total_mb=0;
    bool over_budget=false;
};
// KAMI-exclusive: conservative upload/asset planning independent of GPU SDKs.
// It estimates *uncompressed* texture memory, not SL upload fees or FPS.
[[nodiscard]] SceneEstimate estimate_scene_cost(
    unsigned width,unsigned height,unsigned textures,
    std::uint64_t triangles,unsigned gpu_budget_mb) noexcept;
} // namespace kami::viewer
