#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace kami::protocol::load {
enum class Kind : std::uint8_t {
    OwnAvatar, VisibleMesh, VisibleTexture, Animation,
    Attachment, NearbyMesh, DistantTexture, Other
};
enum class Stage : std::uint8_t {
    Queued, Downloading, Decoding, ReadyForGPU, Resident, Failed
};
struct Asset {
    std::string id;
    Kind kind=Kind::Other;
    float distance_m=0.f;
    bool visible=false;
    std::uint32_t estimated_bytes=0;
    std::uint32_t actual_bytes=0;
    Stage stage=Stage::Queued;
    std::uint64_t queued_order=0;
};
struct Stats {
    std::size_t queued=0,downloading=0,decoding=0,ready_gpu=0,resident=0;
    std::uint64_t gpu_uploaded_bytes=0;
};
class Scheduler final {
public:
    static constexpr std::size_t kMaxAssets=8192U;
    static constexpr unsigned kMaxConcurrentDownloads=8U;
    static constexpr unsigned kMaxConcurrentDecodes=4U;
    static constexpr std::uint32_t kMaxBytesPerAsset=64U*1024U*1024U;
    static constexpr std::uint32_t kDefaultUploadBudget=4U*1024U*1024U;
    // No authenticated network data? Do not schedule any work.
    explicit Scheduler(bool session_online=false) noexcept:online_(session_online){}
    void set_online(bool online) noexcept;
    void clear() noexcept;
    [[nodiscard]] bool request(std::string_view uuid,Kind kind,
                               float distance_m,bool visible,
                               std::uint32_t estimated_bytes);
    [[nodiscard]] std::optional<Asset> next_download();
    [[nodiscard]] bool download_finished(std::string_view uuid,
                                         std::uint32_t actual_bytes,
                                         bool success) noexcept;
    [[nodiscard]] std::optional<Asset> next_decode();
    [[nodiscard]] bool decode_finished(std::string_view uuid,bool success) noexcept;
    [[nodiscard]] std::optional<Asset> next_gpu_upload(
        std::uint32_t remaining_upload_budget) const noexcept;
    [[nodiscard]] bool gpu_uploaded(std::string_view uuid) noexcept;
    [[nodiscard]] const Asset* find(std::string_view uuid) const noexcept;
    [[nodiscard]] Stats statistics() const noexcept;
private:
    [[nodiscard]] Asset* mutable_find(std::string_view uuid) noexcept;
    bool online_=false;
    std::uint64_t next_order_=1;
    std::uint64_t uploaded_bytes_=0;
    std::vector<Asset> entries_;
};
} // namespace kami::protocol::load
