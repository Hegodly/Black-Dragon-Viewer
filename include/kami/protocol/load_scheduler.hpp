#pragma once
#include <cstdint>
#include <cstddef>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace kami::protocol::load {
// Texture downloads are independently fetched/decoded off the simulator poll thread.
inline constexpr std::size_t kMaxConcurrentTextureWorkers=4U;
// Execute a background asset operation without allowing worker exceptions
// to strand its scheduler entry in Downloading. A thrown fetch/decode is
// surfaced as a retryable transport failure; callers still apply bounded
// per-UUID backoff through Scheduler::worker_launch_failed/retry_failed.
template<class Result,class Work>
[[nodiscard]] Result capture_worker_result(std::string id,Work&& work) noexcept {
    Result result{};
    result.id=std::move(id);
    try{
        std::forward<Work>(work)(result);
    }catch(...){
        result.http_status=0U;
        result.bytes=0U;
        result.fetched=false;
        result.decoded=false;
    }
    return result;
}
enum class Kind : std::uint8_t {
    OwnAvatar, VisibleMesh, VisibleTexture, Animation,
    Attachment, NearbyMesh, DistantTexture, Other
};
enum class Stage : std::uint8_t {
    Queued, Downloading, DecodeQueued, Decoding, ReadyForGPU, Resident, Failed
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
    unsigned retry_count=0; // transient fetches only; bounded per asset
    bool retryable_fetch_failure=false; // decoding/corrupt payloads must never retry
};
struct Stats {
    std::size_t queued=0,downloading=0,decoding=0,ready_gpu=0,resident=0;
    std::uint64_t gpu_uploaded_bytes=0;
};
class Scheduler final {
public:
    static constexpr std::size_t kMaxAssets=16384U;
    static constexpr unsigned kMaxConcurrentDownloads=8U;
    static constexpr unsigned kMaxConcurrentDecodes=4U;
    static constexpr unsigned kMaxTransientRetries=3U;
    // Network failures, throttling and server-side errors may be retried;
    // successful and terminal client responses must not be retried.
    [[nodiscard]] static constexpr bool retryable_http_status(
        std::uint16_t status) noexcept {
        return status==0U||status==429U||status>=500U;
    }
    // Exponential quiet period shared by network and local worker-launch retries.
    [[nodiscard]] static constexpr unsigned retry_delay_seconds(
        unsigned retry_count) noexcept {
        return 1U << (retry_count<kMaxTransientRetries?retry_count:
                      kMaxTransientRetries);
    }
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
    // Worker result IDs must only advance THEIR OWN decode job. Picking
    // an unrelated queued decode can leave an item stuck in Decoding
    // forever when jobs complete out of priority order or are evicted.
    [[nodiscard]] bool begin_decode(std::string_view uuid) noexcept;
    [[nodiscard]] bool decode_finished(std::string_view uuid,bool success) noexcept;
    // Explicit bounded retry of a failed *transient HTTP* fetch.
    // Caller enforces per-asset backoff; corrupt asset decodes are not retried.
    [[nodiscard]] bool retry_failed(std::string_view uuid) noexcept;
    // Convert a local async worker-launch failure into a transient fetch
    // failure and return the bounded retry delay in seconds. The caller
    // schedules retry_failed() after that delay; no immediate retry occurs.
    [[nodiscard]] std::optional<unsigned> worker_launch_failed(
        std::string_view uuid) noexcept;
    // Forget an evicted/despawned asset so future visible use can fetch it
    // again. Never cancel an in-progress network or decode worker.
    [[nodiscard]] bool forget_finished(std::string_view uuid) noexcept;
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
