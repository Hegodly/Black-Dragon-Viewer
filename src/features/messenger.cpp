#include "kami/features/messenger.hpp"
#include <algorithm>
#include <cwctype>
#include <utility>

namespace kami::messenger {
Model::Model() {
    preview_.push_back({next_demo_id_++,Direction::System,
        L"KAMI MESSENGER  /  ORIGINAL DESIGN PREVIEW",0,true});
    preview_.push_back({next_demo_id_++,Direction::Incoming,
        L"Welcome to KAMI. This example isn't a real resident message.",9*60+30,true});
    preview_.push_back({next_demo_id_++,Direction::Outgoing,
        L"Clean spacing and centered bubble text look much better.",9*60+31,true});
    preview_.push_back({next_demo_id_++,Direction::Incoming,
        L"Try typing below. Local drafts never reach Second Life.",9*60+32,true});
}
void Model::select(Channel channel) noexcept {
    switch(channel) {
    case Channel::Preview: case Channel::Nearby: case Channel::Groups:
        active_=channel;break;
    }
}
bool Model::set_draft(std::wstring text) {
    if(text.size()>512)return false;
    for(const wchar_t ch:text) {
        if(ch==L'\0' || ch==L'\r' || ch==L'\n')return false;
    }
    draft_=std::move(text);
    return true;
}
bool Model::send_local(unsigned clock_minutes) {
    if(!can_send()||preview_.size()>=96)return false;
    const auto start=draft_.find_first_not_of(L" \t");
    if(start==std::wstring::npos)return false;
    const auto end=draft_.find_last_not_of(L" \t");
    const std::wstring message=draft_.substr(start,end-start+1);
    if(message.empty()||message.size()>512)return false;
    const std::uint64_t id=outbox_.compose(L"KAMI-LOCAL-DESIGN-PREVIEW",message);
    if(!id)return false;
    // Deliberately NOT queued or transmitted: no SL network adapter exists.
    preview_.push_back({id,Direction::Outgoing,message,
                        clock_minutes%1440U,true});
    draft_.clear();
    return true;
}
bool Model::add_demo_reply(unsigned clock_minutes) {
    if(!can_send()||preview_.size()>=96)return false;
    preview_.push_back({next_demo_id_++,Direction::Incoming,
        L"This is a simulated reply for layout testing only.",
        clock_minutes%1440U,true});
    return true;
}
std::wstring_view channel_title(Channel channel) noexcept {
    switch(channel) {
    case Channel::Preview:return L"Design Preview";
    case Channel::Nearby:return L"Nearby Chat";
    case Channel::Groups:return L"Group Conversations";
    }
    return L"Messages";
}
std::wstring_view channel_subtitle(Channel channel) noexcept {
    switch(channel) {
    case Channel::Preview:return L"Offline sample threads and local drafts";
    case Channel::Nearby:return L"Requires authenticated region connection";
    case Channel::Groups:return L"Requires live group and membership services";
    }
    return L"Offline";
}
} // namespace kami::messenger
