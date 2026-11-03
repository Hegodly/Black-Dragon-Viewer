#pragma once
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace kami::social {
enum class Receipt : std::uint8_t { Draft, Queued, Transmitted, Delivered, Read };
struct Message {
    std::uint64_t id = 0;
    std::wstring recipient;
    std::wstring body;
    Receipt receipt = Receipt::Draft;
};
class Outbox final {
public:
    [[nodiscard]] std::uint64_t compose(std::wstring recipient,std::wstring body);
    [[nodiscard]] bool queue(std::uint64_t id) noexcept;
    // Only the network adapter may acknowledge transmission/delivery/read.
    [[nodiscard]] bool confirm_by_transport(std::uint64_t id,Receipt status,
                                             bool authenticated_session) noexcept;
    [[nodiscard]] const std::vector<Message>& messages() const noexcept {return messages_;}
private:
    std::vector<Message> messages_;
    std::uint64_t next_ = 1;
};
struct ViewerAdvertisement {
    std::wstring viewer_name;
    bool participant_opted_in = false;
    bool authenticated_peer_metadata = false;
};
[[nodiscard]] std::wstring_view visible_viewer(const ViewerAdvertisement& value) noexcept;
} // namespace kami::social
