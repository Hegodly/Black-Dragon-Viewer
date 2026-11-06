#include "kami/protocol/load_scheduler.hpp"
#include <cmath>
#include <limits>
using namespace kami::protocol::load;
static constexpr auto a="12345678-90ab-4cde-8fab-000000000010";
static constexpr auto b="12345678-90ab-4cde-8fab-000000000011";
static constexpr auto c="12345678-90ab-4cde-8fab-000000000012";
static constexpr auto d="12345678-90ab-4cde-8fab-000000000013";
int main(){
    Scheduler queue;
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
    // A successful fetch followed by a corrupt/unsupported decode is
    // terminal. It must not enter the transient HTTP retry loop.
    if(!queue.request(a,Kind::VisibleMesh,1.f,true,64U)||
       !queue.next_download()||!queue.download_finished(a,64U,true)||
       !queue.begin_decode(a)||!queue.decode_finished(a,false)||
       queue.retry_failed(a)||queue.find(a)->stage!=Stage::Failed)
       return 33;
    // A transport failure is still eligible for bounded retry.
    if(!queue.request(b,Kind::VisibleTexture,1.f,true,64U)||
       !queue.next_download()||!queue.download_finished(b,0U,false)||
       !queue.retry_failed(b))return 34;
    return 0;
}
