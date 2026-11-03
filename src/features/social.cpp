#include "kami/features/social.hpp"
#include <utility>
namespace kami::social {
std::uint64_t Outbox::compose(std::wstring recipient,std::wstring body) {
    if(recipient.empty()||body.empty()||next_==0) return 0;
    const auto id=next_++;
    messages_.push_back({id,std::move(recipient),std::move(body),Receipt::Draft});
    return id;
}
bool Outbox::queue(std::uint64_t id) noexcept {
    for(auto& message:messages_) if(message.id==id && message.receipt==Receipt::Draft) {
        message.receipt=Receipt::Queued;
        return true;
    }
    return false;
}
bool Outbox::confirm_by_transport(std::uint64_t id,Receipt status,
                                  bool authenticated_session) noexcept {
    if(!authenticated_session || status<Receipt::Transmitted || status>Receipt::Read)
        return false;
    for(auto& message:messages_) if(message.id==id) {
        if(message.receipt<Receipt::Queued||status<message.receipt) return false;
        message.receipt=status;
        return true;
    }
    return false;
}
std::wstring_view visible_viewer(const ViewerAdvertisement& value) noexcept {
    return value.participant_opted_in && value.authenticated_peer_metadata ?
        std::wstring_view(value.viewer_name):std::wstring_view{};
}
} // namespace kami::social
