#include "kami/protocol/load_scheduler.hpp"
#include "kami/protocol/bootstrap.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <tuple>

namespace kami::protocol::load {
namespace {
int rank(Kind kind) noexcept {
    switch(kind){
    case Kind::OwnAvatar:return 0;
    case Kind::VisibleMesh:return 1;
    case Kind::VisibleTexture:return 2;
    case Kind::Animation:return 3;
    case Kind::Attachment:return 4;
    case Kind::NearbyMesh:return 5;
    case Kind::DistantTexture:return 6;
    default:return 7;
    }
}
bool better(const Asset& a,const Asset& b) noexcept {
    if(a.visible!=b.visible)return a.visible;
    const auto ap=rank(a.kind),bp=rank(b.kind);
    if(ap!=bp)return ap<bp;
    if(a.distance_m!=b.distance_m)return a.distance_m<b.distance_m;
    return a.queued_order<b.queued_order;
}
} // namespace
void Scheduler::set_online(bool value) noexcept {
    if(value==online_)return;
    clear();
    online_=value;
}
void Scheduler::clear() noexcept {
    entries_.clear();next_order_=1;uploaded_bytes_=0;
}
Asset* Scheduler::mutable_find(std::string_view id) noexcept {
    const auto it=std::find_if(entries_.begin(),entries_.end(),
         [&](const Asset& asset){return asset.id==id;});
    return it==entries_.end()?nullptr:&*it;
}
const Asset* Scheduler::find(std::string_view id) const noexcept {
    const auto it=std::find_if(entries_.begin(),entries_.end(),
         [&](const Asset& asset){return asset.id==id;});
    return it==entries_.end()?nullptr:&*it;
}
bool Scheduler::request(std::string_view id,Kind kind,float distance,
                        bool visible,std::uint32_t estimate){
    if(!online_||!valid_uuid(id)||kind>Kind::Other||
       !std::isfinite(distance)||distance<0.f||distance>100000.f||
       estimate>kMaxBytesPerAsset)return false;
    if(auto* prior=mutable_find(id)){
        // Do not enqueue duplicate HTTP/asset requests for repeat UUIDs.
        // A nearby visible demand may promote a pending low-priority job.
        if(prior->stage==Stage::Queued) {
            if(visible&&!prior->visible)prior->visible=true;
            if(rank(kind)<rank(prior->kind))prior->kind=kind;
            prior->distance_m=std::min(prior->distance_m,distance);
        }
        return true;
    }
    if(entries_.size()>=kMaxAssets||next_order_==0)return false;
    entries_.push_back({std::string(id),kind,distance,visible,estimate,0U,
                        Stage::Queued,next_order_++});
    return true;
}
std::optional<Asset> Scheduler::next_download(){
    if(!online_)return std::nullopt;
    unsigned active=0;
    for(const auto& entry:entries_)if(entry.stage==Stage::Downloading)++active;
    if(active>=kMaxConcurrentDownloads)return std::nullopt;
    Asset* next=nullptr;
    for(auto& entry:entries_)if(entry.stage==Stage::Queued &&
       (!next||better(entry,*next)))next=&entry;
    if(!next)return std::nullopt;
    next->stage=Stage::Downloading;
    return *next;
}
bool Scheduler::download_finished(std::string_view id,
                                  std::uint32_t bytes,bool success) noexcept {
    auto* asset=online_?mutable_find(id):nullptr;
    if(!asset||asset->stage!=Stage::Downloading)return false;
    if(!success||bytes>kMaxBytesPerAsset||bytes==0){
        asset->stage=Stage::Failed;return true;
    }
    asset->actual_bytes=bytes;
    asset->stage=Stage::Decoding;
    return true;
}
std::optional<Asset> Scheduler::next_decode() {
    // In the initial pipeline the download completion moves directly to
    // decoding; real workers take the resulting jobs off-thread.
    if(!online_)return std::nullopt;
    Asset* next=nullptr;
    for(auto& entry:entries_)if(entry.stage==Stage::Decoding&&
       (!next||better(entry,*next)))next=&entry;
    return next?std::optional<Asset>(*next):std::nullopt;
}
bool Scheduler::decode_finished(std::string_view id,bool success) noexcept {
    auto* asset=online_?mutable_find(id):nullptr;
    if(!asset||asset->stage!=Stage::Decoding)return false;
    asset->stage=success?Stage::ReadyForGPU:Stage::Failed;
    return true;
}
std::optional<Asset> Scheduler::next_gpu_upload(std::uint32_t budget) const noexcept {
    if(!online_||budget==0)return std::nullopt;
    const Asset* next=nullptr;
    for(const auto& entry:entries_)if(entry.stage==Stage::ReadyForGPU&&
       entry.actual_bytes<=budget&&(!next||better(entry,*next)))next=&entry;
    return next?std::optional<Asset>(*next):std::nullopt;
}
bool Scheduler::gpu_uploaded(std::string_view id) noexcept {
    auto* asset=online_?mutable_find(id):nullptr;
    if(!asset||asset->stage!=Stage::ReadyForGPU)return false;
    uploaded_bytes_+=asset->actual_bytes;
    asset->stage=Stage::Resident;
    return true;
}
Stats Scheduler::statistics() const noexcept {
    Stats s{};s.gpu_uploaded_bytes=uploaded_bytes_;
    if(!online_)return s;
    for(const auto& asset:entries_)switch(asset.stage){
    case Stage::Queued:++s.queued;break;
    case Stage::Downloading:++s.downloading;break;
    case Stage::Decoding:++s.decoding;break;
    case Stage::ReadyForGPU:++s.ready_gpu;break;
    case Stage::Resident:++s.resident;break;
    default:break;
    }
    return s;
}
} // namespace kami::protocol::load
