#pragma once
#include "kami/protocol/transport.hpp"
#include <cstdint>
#include <string>
#include <string_view>

namespace kami::protocol {
// Independent transport-to-renderer lifecycle. Only network adapters should
// pass authenticated evidence into this model; a fabricated local test state
// must never be treated as an online user session.
enum class BootstrapPhase : std::uint8_t {
    Offline, EndpointChecked, LoginReplyValidated, CircuitEstablished,
    CapabilitiesEstablished, RegionSynced
};
struct LoginHandoff {
    std::string agent_id;
    std::string session_id;
    std::string simulator_host;
    std::uint16_t simulator_port=0;
    std::wstring seed_capability;
};
struct RegionIdentity {
    std::string region_id;
    std::string agent_id;
    float x=0.f,y=0.f,z=0.f;
};
[[nodiscard]] bool valid_uuid(std::string_view value) noexcept;
[[nodiscard]] bool valid_handoff(const LoginHandoff& data) noexcept;
class Bootstrap final {
public:
    void reset() noexcept;
    [[nodiscard]] BootstrapPhase phase() const noexcept {return phase_;}
    [[nodiscard]] std::uint64_t generation() const noexcept {return generation_;}
    [[nodiscard]] bool endpoint_checked(ProbeResult result) noexcept;
    [[nodiscard]] bool login_reply(LoginHandoff info,bool verified_login_reply);
    [[nodiscard]] bool circuit_ack(std::string_view agent_id,
                                   bool verified_simulator_message) noexcept;
    [[nodiscard]] bool capability_reply(std::wstring_view capability,
                                        bool verified_seed_response) noexcept;
    [[nodiscard]] bool region_snapshot(RegionIdentity location,
                                       bool verified_region_event) noexcept;
    [[nodiscard]] bool authenticated() const noexcept {
        return phase_>=BootstrapPhase::LoginReplyValidated;
    }
    [[nodiscard]] bool online() const noexcept {
        return phase_==BootstrapPhase::RegionSynced;
    }
private:
    BootstrapPhase phase_=BootstrapPhase::Offline;
    std::uint64_t generation_=0;
    LoginHandoff handoff_{};
};
} // namespace kami::protocol
