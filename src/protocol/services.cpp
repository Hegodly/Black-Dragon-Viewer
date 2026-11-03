#include "kami/protocol/services.hpp"
#include <algorithm>
#include <cmath>
#include <unordered_set>

namespace kami::protocol {
const wchar_t* service_name(Service value) noexcept {
    switch(value){
    case Service::Inventory:return L"Inventory";
    case Service::NearbyChat:return L"Nearby chat";
    case Service::InstantMessage:return L"Instant messages";
    case Service::Friends:return L"Friends";
    case Service::Groups:return L"Groups";
    case Service::Teleport:return L"Teleport";
    case Service::WorldMap:return L"World map";
    case Service::Parcels:return L"Parcels";
    case Service::Building:return L"World editing";
    case Service::AvatarAppearance:return L"Avatar appearance";
    case Service::AssetDownload:return L"Asset download";
    case Service::Animation:return L"Animations";
    case Service::Sound:return L"Sound";
    case Service::Voice:return L"Voice";
    default:return L"Unknown";
    }
}
bool validate_intent(const Intent& item) noexcept {
    if(item.service>=Service::Count ||item.target_id.size()>128||
       item.text.size()>512)return false;
    for(const wchar_t ch:item.text)
        if(ch==L'\0'||ch==L'\r'||ch==L'\n')return false;
    // Never broadcast an empty chat. Explicit agent/group actions must
    // specify a UUID; parcel/build actions require a target or other
    // unambiguous server identifier and simulator-side permissions.
    if((item.service==Service::NearbyChat||
        item.service==Service::InstantMessage)&&item.text.empty())return false;
    if((item.service==Service::InstantMessage||
        item.service==Service::Building||
        item.service==Service::AvatarAppearance||
        item.service==Service::Animation)&&
       !valid_uuid(item.target_id))return false;
    return true;
}
std::uint64_t Services::prepare(Service kind,std::string target,
                                 std::wstring text) {
    if(!session_.online()||requests_.size()>=256||next_id_==0)return 0;
    Intent proposal{kind,std::move(target),std::move(text),
                    next_id_,session_.generation(),IntentState::Pending};
    if(!validate_intent(proposal))return 0;
    requests_.push_back(std::move(proposal));
    return next_id_++;
}
bool Services::mark_sent(std::uint64_t id) noexcept {
    if(!session_.online())return false;
    for(auto& request:requests_)if(request.id==id &&
       request.connection_generation==session_.generation()&&
       request.state==IntentState::Pending) {
        request.state=IntentState::Sent;return true;
    }
    return false;
}
bool Services::accept_server_result(std::uint64_t id,
                                    bool verified,bool success) noexcept {
    if(!session_.online()||!verified)return false;
    for(auto& request:requests_)if(request.id==id &&
       request.connection_generation==session_.generation()&&
       request.state==IntentState::Sent) {
        request.state=success?IntentState::ServerConfirmed:
                              IntentState::ServerRejected;
        return true;
    }
    return false;
}
void Services::clear() noexcept {requests_.clear();}
std::size_t Services::pending_count() const noexcept {
    if(!session_.online())return 0;
    std::size_t total=0;
    for(const auto& request:requests_)
        if(request.connection_generation==session_.generation()&&
           (request.state==IntentState::Pending||
            request.state==IntentState::Sent))++total;
    return total;
}
bool LiveData::apply_objects(std::vector<WorldObject> value,bool verified) {
    if(!verified||!session_.online()||value.size()>100000U)return false;
    std::unordered_set<std::string> ids;
    for(const auto& item:value) {
        if(!valid_uuid(item.object_id)||!std::isfinite(item.x)||
           !std::isfinite(item.y)||!std::isfinite(item.z)||
           std::abs(item.x)>100000.f||std::abs(item.y)>100000.f||
           std::abs(item.z)>100000.f||
           (!item.mesh_asset_id.empty()&&!valid_uuid(item.mesh_asset_id))||
           !ids.insert(item.object_id).second)return false;
    }
    objects_=std::move(value);
    objects_generation_=session_.generation();
    return true;
}
bool LiveData::apply_inventory(std::vector<InventoryEntry> value,bool verified) {
    if(!verified||!session_.online()||value.size()>200000U)return false;
    std::unordered_set<std::string> ids;
    for(const auto& item:value) {
        if(!valid_uuid(item.item_id)||
           (!item.parent_id.empty()&&!valid_uuid(item.parent_id))||
           (!item.asset_id.empty()&&!valid_uuid(item.asset_id))||
           item.name.size()>255||item.name.empty()||
           !ids.insert(item.item_id).second)return false;
    }
    inventory_=std::move(value);
    inventory_generation_=session_.generation();
    return true;
}
void LiveData::clear() noexcept {
    objects_.clear();inventory_.clear();
    objects_generation_=inventory_generation_=0;
}
} // namespace kami::protocol
