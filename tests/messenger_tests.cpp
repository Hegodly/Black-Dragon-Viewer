#include "kami/features/messenger.hpp"
int main() {
    using namespace kami::messenger;
    Model model;
    if(model.message_count()!=4||!model.can_send()||model.has_transport())return 1;
    if(!model.set_draft(L"    Hello local preview   "))return 2;
    if(!model.send_local(500))return 3;
    if(model.message_count()!=5||model.local_outbox_count()!=1||
       !model.draft().empty())return 4;
    const auto& last=model.messages().back();
    if(last.direction!=Direction::Outgoing||
       last.text!=L"Hello local preview"||!last.synthetic)return 5;
    if(!model.set_draft(L"    ")||model.send_local(501))return 6;
    if(model.set_draft(std::wstring(513,L'X')))return 7;
    if(model.set_draft(L"A\nB"))return 8;
    model.select(Channel::Nearby);
    if(model.can_send()||model.add_demo_reply(502)||model.send_local(502))return 9;
    model.select(Channel::Groups);
    if(model.can_send()||channel_title(model.active())!=L"Group Conversations")return 10;
    model.select(Channel::Preview);
    if(!model.add_demo_reply(503)||model.message_count()!=6)return 11;
    if(model.messages().back().direction!=Direction::Incoming||
       !model.messages().back().synthetic)return 12;
    for(const auto& item:model.messages())
        if(!item.synthetic)return 13;
    return 0;
}
