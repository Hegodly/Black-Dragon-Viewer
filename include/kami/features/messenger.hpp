#pragma once
#include "kami/features/social.hpp"
#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace kami::messenger {
enum class Channel : std::uint8_t { Preview, Nearby, Groups };
enum class Direction : std::uint8_t { Outgoing, Incoming, System };
struct Bubble {
    std::uint64_t id=0;
    Direction direction=Direction::System;
    std::wstring text;
    unsigned clock_minutes=0;
    bool synthetic=true;
};
class Model final {
public:
    Model();
    [[nodiscard]] Channel active() const noexcept {return active_;}
    void select(Channel channel) noexcept;
    [[nodiscard]] bool can_send() const noexcept {return active_==Channel::Preview;}
    [[nodiscard]] bool set_draft(std::wstring text);
    [[nodiscard]] const std::wstring& draft() const noexcept {return draft_;}
    [[nodiscard]] bool send_local(unsigned clock_minutes);
    [[nodiscard]] bool add_demo_reply(unsigned clock_minutes);
    [[nodiscard]] std::size_t message_count() const noexcept {return preview_.size();}
    [[nodiscard]] const std::vector<Bubble>& messages() const noexcept {return preview_;}
    [[nodiscard]] std::size_t local_outbox_count() const noexcept {
        return outbox_.messages().size();
    }
    // This model never reports delivered/read, and never invokes SL transport.
    [[nodiscard]] bool has_transport() const noexcept {return false;}
private:
    Channel active_=Channel::Preview;
    std::wstring draft_;
    std::vector<Bubble> preview_;
    social::Outbox outbox_;
    std::uint64_t next_demo_id_=1000;
};
[[nodiscard]] std::wstring_view channel_title(Channel channel) noexcept;
[[nodiscard]] std::wstring_view channel_subtitle(Channel channel) noexcept;
} // namespace kami::messenger
