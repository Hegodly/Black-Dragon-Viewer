#include "kami/protocol/load_scheduler.hpp"
#include <cmath>
#include <cstdio>
#include <limits>
using namespace kami::protocol::load;
static constexpr auto a="12345678-90ab-4cde-8fab-000000000010";
static constexpr auto b="12345678-90ab-4cde-8fab-000000000011";
static constexpr auto c="12345678-90ab-4cde-8fab-000000000012";
static constexpr auto d="12345678-90ab-4cde-8fab-000000000013";
int main(){
    Scheduler queue;
    if(!Scheduler::retryable_http_status(0U)||
       !Scheduler::retryable_http_status(408U)||
       !Scheduler::retryable_http_status(429U)||
       !Scheduler::retryable_http_status(500U)||
       !Scheduler::retryable_http_status(502U)||
       !Scheduler::retryable_http_status(503U)||
       !Scheduler::retryable_http_status(504U)||
       Scheduler::retryable_http_status(200U)||
       Scheduler::retryable_http_status(400U)||
       Scheduler::retryable_http_status(401U)||
       Scheduler::retryable_http_status(403U)||
       Scheduler::retryable_http_status(404U)||
       Scheduler::retryable_http_status(501U))return 40;
    if(queue.request(a,Kind::VisibleTexture,5.f,true,100U))return 1;
    queue.set_online(true);
    if(queue.request("bad",Kind::Other,1.f,true,100U)||
       queue.request(a,Kind::VisibleTexture,
                     std::numeric_limits<float>::quiet_NaN(),true,100U)||
       queue.request(a,Kind::VisibleTexture,2.f,true,
                     Scheduler::kMaxBytesPerAsset+1U))return 2;
    if(!queue.request(a,Kind::DistantTexture,90.f,false,300U)||
       !queue.request(b,Kind::VisibleTexture,4.f,true,100U)||
       !queue.request(c,Kind::OwnAvatar,20.f,false,200U)||
       !queue.request(d,Kind::NearbyMesh,7.f,false,400U))return 3;
    if(!queue.request(a,Kind::VisibleMesh,1.f,true,300U)||
       queue.statistics().queued!=4)return 4;
    const auto first=queue.next_download();
    if(!first||first->id!=a||queue.statistics().downloading!=1)return 5;
    if(!queue.download_finished(a,150U,true)||
       queue.find(a)->stage!=Stage::DecodeQueued)return 6;
    const auto second=queue.next_download();
    if(!second||second->id!=b)return 7;
    const auto decode_a=queue.next_decode();
    if(!decode_a||decode_a->id!=a||queue.next_decode()||
       !queue.decode_finished(a,true)||queue.decode_finished(a,false)||
       !queue.next_gpu_upload(150U)||queue.next_gpu_upload(149U))
       return 8;
    if(!queue.gpu_uploaded(a)||queue.gpu_uploaded(a)||
       queue.statistics().resident!=1||
       queue.statistics().gpu_uploaded_bytes!=150U)return 9;
    if(!queue.download_finished(b,0U,false)||
       queue.find(b)->stage!=Stage::Failed)return 10;
    const auto third=queue.next_download(),fourth=queue.next_download();
    if(!third||!fourth||third->id!=c||fourth->id!=d)return 11;
    queue.set_online(false);
    if(queue.statistics().resident!=0||queue.find(a)!=nullptr||
       queue.next_download()||queue.request(a,Kind::OwnAvatar,1.f,true,1U))
       return 12;
    queue.set_online(true);
    if(!queue.request(c,Kind::OwnAvatar,0.f,true,1U))return 13;
    const auto last=queue.next_download();
    if(!last||last->id!=c)return 14;
    if(!queue.download_finished(c,0U,false)||queue.retry_failed(a))
        return 15;
    // A failed transient download may only be retried explicitly three
    // times, never requeued automatically or after a malformed decode.
    for(unsigned retry=1U;retry<=Scheduler::kMaxTransientRetries;++retry){
        if(!queue.retry_failed(c)||queue.find(c)->retry_count!=retry||
           queue.find(c)->stage!=Stage::Queued)return 16;
        const auto attempt=queue.next_download();
        if(!attempt||attempt->id!=c||
           !queue.download_finished(c,0U,false))return 17;
    }
    if(queue.retry_failed(c)||queue.next_download()||
       queue.find(c)->stage!=Stage::Failed)return 18;
    queue.clear();
    if(queue.find(c)||queue.statistics().queued!=0U)return 19;
    // A mesh referenced by a now-despawned linkset can be evicted,
    // leaving capacity for other assets. Never cancel an active worker.
    if(!queue.request(a,Kind::VisibleMesh,1.f,true,20U))return 20;
    auto active=queue.next_download();
    if(!active||active->id!=a||queue.forget_finished(a))return 21;
    if(!queue.download_finished(a,20U,true))return 22;
    auto decoded=queue.next_decode();
    if(!decoded||decoded->id!=a||
       queue.forget_finished(a)||!queue.decode_finished(a,true))
       return 23;
    if(!queue.next_gpu_upload(20U)||!queue.gpu_uploaded(a)||
       queue.statistics().gpu_uploaded_bytes!=20U)return 24;
    if(!queue.forget_finished(a)||queue.find(a)||
       queue.statistics().gpu_uploaded_bytes!=0U)return 25;
    if(!queue.request(a,Kind::VisibleMesh,2.f,true,20U)||
       !queue.next_download())return 26;
    // Worker completion may differ from priority order. A downloaded
    // visible texture must only transition its own UUID to Decoding.
    queue.clear();
    if(!queue.request(a,Kind::VisibleTexture,20.f,true,20U)||
       !queue.request(b,Kind::OwnAvatar,1.f,true,20U))return 27;
    auto high_priority=queue.next_download();
    auto lower_priority=queue.next_download();
    if(!high_priority||!lower_priority||
       high_priority->id!=b||lower_priority->id!=a)return 28;
    if(!queue.download_finished(b,20U,true)||
       !queue.download_finished(a,20U,true))return 29;
    if(!queue.begin_decode(a)||queue.begin_decode(a)||
       queue.begin_decode(d)||
       queue.find(b)->stage!=Stage::DecodeQueued||
       queue.find(a)->stage!=Stage::Decoding)return 30;
    if(!queue.decode_finished(a,true)||
       !queue.begin_decode(b)||!queue.decode_finished(b,true))
       return 31;
    queue.clear();
    if(queue.begin_decode(a))return 32;
    // Decoding failures and malformed successful responses are terminal.
    if(!queue.request(a,Kind::VisibleMesh,1.f,true,64U)||
       !queue.next_download()||!queue.download_finished(a,64U,true)||
       !queue.begin_decode(a)||!queue.decode_finished(a,false)||
       queue.retry_failed(a)||queue.find(a)->retryable_fetch_failure)return 33;
    if(!queue.request(b,Kind::VisibleTexture,1.f,true,64U)||
       !queue.next_download()||!queue.download_finished(b,0U,true)||
       queue.find(b)->actual_bytes!=0U||queue.retry_failed(b)||
       queue.find(b)->retryable_fetch_failure)return 34;
    if(!queue.request(d,Kind::VisibleTexture,1.f,true,64U)||
       !queue.next_download()||
       !queue.download_finished(d,Scheduler::kMaxBytesPerAsset+1U,true)||
       queue.find(d)->actual_bytes!=0U||queue.retry_failed(d)||
       queue.find(d)->retryable_fetch_failure)return 41;
    if(!queue.request(c,Kind::VisibleTexture,1.f,true,64U)||
       !queue.next_download()||!queue.download_finished(c,0U,false)||
       !queue.find(c)->retryable_fetch_failure||
       !queue.retry_failed(c)||queue.find(c)->retryable_fetch_failure||
       queue.find(c)->actual_bytes!=0U)return 35;
    // Dense region: terminal corrupt assets must not permanently block
    // a newly visible mesh when the bounded scheduler reaches capacity.
    queue.clear();
    for(std::size_t i=0;i<Scheduler::kMaxAssets;++i){
        char uuid[37]{};
        std::snprintf(uuid,sizeof(uuid),
            "12345678-90ab-4cde-8fab-%012llx",
            static_cast<unsigned long long>(i+256U));
        if(!queue.request(uuid,Kind::VisibleMesh,1.f,true,64U))
            return 36;
    }
    if(queue.request("12345678-90ab-4cde-8fab-ffffffffffff",
                     Kind::VisibleMesh,1.f,true,64U))return 37;
    const auto full_first=queue.next_download();
    if(!full_first||!queue.download_finished(full_first->id,0U,true))
        return 38;
    if(!queue.request("12345678-90ab-4cde-8fab-ffffffffffff",
                      Kind::VisibleMesh,1.f,true,64U)||
       queue.find(full_first->id)!=nullptr)return 39;
    // An asset can become important after its download and decode complete.
    // Refresh its demand before GPU upload so visible meshes outrank stale,
    // distant textures without issuing a duplicate asset request.
    queue.clear();
    if(!queue.request(a,Kind::DistantTexture,80.f,false,64U)||
       !queue.request(b,Kind::VisibleTexture,2.f,true,64U))
        return 42;
    const auto download_first=queue.next_download();
    const auto download_second=queue.next_download();
    if(!download_first||!download_second||download_first->id!=b||
       download_second->id!=a)return 43;
    if(!queue.download_finished(a,64U,true)||
       !queue.download_finished(b,64U,true)||
       !queue.begin_decode(a)||!queue.begin_decode(b)||
       !queue.decode_finished(a,true)||!queue.decode_finished(b,true))
        return 44;
    if(!queue.request(a,Kind::VisibleMesh,1.f,true,64U)||
       !queue.find(a)->visible||
       queue.find(a)->kind!=Kind::VisibleMesh)
        return 45;
    const auto promoted=queue.next_gpu_upload(64U);
    if(!promoted||promoted->id!=a)return 46;
    // Once uploaded, residency priority is stable: later distant requests
    // must not demote or hide an asset already resident in the GPU cache.
    if(!queue.gpu_uploaded(a))return 47;
    if(!queue.request(a,Kind::DistantTexture,90.f,false,64U)||
       queue.find(a)->stage!=Stage::Resident||
       queue.find(a)->kind!=Kind::VisibleMesh||
       !queue.find(a)->visible)return 48;
    // Local worker-launch failures use the same saturating exponential
    // backoff as transient HTTP errors and remain bounded to three retries.
    if(Scheduler::retry_delay_seconds(0U)!=1U||
       Scheduler::retry_delay_seconds(1U)!=2U||
       Scheduler::retry_delay_seconds(2U)!=4U||
       Scheduler::retry_delay_seconds(3U)!=8U||
       Scheduler::retry_delay_seconds(99U)!=8U)return 49;
    queue.clear();
    if(!queue.request(c,Kind::VisibleMesh,1.f,true,64U))return 50;
    const auto dispatch=queue.next_download();
    if(!dispatch||dispatch->id!=c||
       !queue.download_finished(c,0U,false)||
       !queue.find(c)->retryable_fetch_failure||
       !queue.retry_failed(c))return 51;
    const auto retried=queue.next_download();
    if(!retried||retried->id!=c)return 52;
    // Local worker-launch failures share the bounded retry schedule with
    // transient transport errors and cannot leave jobs stuck in Downloading.
    if(Scheduler::retry_delay_seconds(0U)!=1U||
       Scheduler::retry_delay_seconds(1U)!=2U||
       Scheduler::retry_delay_seconds(2U)!=4U||
       Scheduler::retry_delay_seconds(3U)!=8U||
       Scheduler::retry_delay_seconds(99U)!=8U)return 62;
    queue.clear();
    if(!queue.request(d,Kind::VisibleMesh,1.f,true,64U))return 63;
    for(unsigned failure=0U;failure<=Scheduler::kMaxTransientRetries;++failure){
        const auto job=queue.next_download();
        if(!job||job->id!=d)return 64;
        const auto delay=queue.worker_launch_failed(d);
        const auto* failed=queue.find(d);
        if(!failed||failed->stage!=Stage::Failed||
           !failed->retryable_fetch_failure)return 65;
        if(failure<Scheduler::kMaxTransientRetries){
            if(!delay||*delay!=Scheduler::retry_delay_seconds(failure)||
               !queue.retry_failed(d))return 66;
        }else if(delay||queue.retry_failed(d)||
                 failed->retry_count!=Scheduler::kMaxTransientRetries)
            return 67;
    }
    // JPEG2000 texture workers use the same bounded launch-failure recovery
    // independently of mesh jobs, including retry exhaustion.
    queue.clear();
    if(!queue.request(a,Kind::VisibleTexture,1.f,true,64U))return 69;
    for(unsigned failure=0U;failure<=Scheduler::kMaxTransientRetries;++failure){
        const auto job=queue.next_download();
        if(!job||job->id!=a)return 70;
        const auto delay=queue.worker_launch_failed(a);
        const auto* failed=queue.find(a);
        if(!failed||failed->stage!=Stage::Failed||
           !failed->retryable_fetch_failure)return 71;
        if(failure<Scheduler::kMaxTransientRetries){
            if(!delay||*delay!=Scheduler::retry_delay_seconds(failure)||
               !queue.retry_failed(a))return 72;
        }else if(delay||queue.retry_failed(a)||
                 failed->retry_count!=Scheduler::kMaxTransientRetries)
            return 73;
    }
    // HTTP status classification keeps authentication/permission failures
    // terminal while permitting only transient server/timeout responses.
    if(!Scheduler::retryable_http_status(0U)||
       !Scheduler::retryable_http_status(408U)||
       !Scheduler::retryable_http_status(429U)||
       !Scheduler::retryable_http_status(500U)||
       !Scheduler::retryable_http_status(502U)||
       !Scheduler::retryable_http_status(503U)||
       !Scheduler::retryable_http_status(504U)||
       Scheduler::retryable_http_status(200U)||
       Scheduler::retryable_http_status(400U)||
       Scheduler::retryable_http_status(401U)||
       Scheduler::retryable_http_status(403U)||
       Scheduler::retryable_http_status(404U)||
       Scheduler::retryable_http_status(501U))return 68;
    // A mesh that leaves the avatar's interest set while queued can be
    // forgotten immediately. An active HTTP worker remains owned until it
    // completes; its late result can then be discarded before decode.
    queue.clear();
    if(!queue.request(a,Kind::VisibleMesh,8.f,true,64U))return 74;
    if(!queue.forget_finished(a)||queue.find(a)!=nullptr)return 75;
    if(!queue.request(b,Kind::VisibleMesh,9.f,true,64U))return 76;
    const auto stale_worker=queue.next_download();
    if(!stale_worker||stale_worker->id!=b||queue.forget_finished(b))
        return 77;
    if(!queue.download_finished(b,64U,true)||
       queue.find(b)->stage!=Stage::DecodeQueued||
       !queue.forget_finished(b)||queue.find(b)!=nullptr)return 78;
    if(!queue.request(b,Kind::VisibleMesh,3.f,true,64U))return 79;
    const auto reentered=queue.next_download();
    if(!reentered||reentered->id!=b)return 80;
    return 0;
}
