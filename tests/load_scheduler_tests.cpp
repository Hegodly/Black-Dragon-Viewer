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
    return 0;
}
