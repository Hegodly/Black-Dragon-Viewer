#include "kami/protocol/bootstrap.hpp"
#include <algorithm>
#include <cmath>
#include <cctype>

namespace kami::protocol {
namespace {
bool host_valid(std::string_view host) noexcept {
    if(host.empty()||host.size()>253||host.front()=='.'||host.back()=='.')
        return false;
    for(const char c:host)if(!(std::isalnum(static_cast<unsigned char>(c))||
                              c=='-'||c=='.'))return false;
    return host.find('.')!=std::string_view::npos;
}
} // namespace
bool valid_uuid(std::string_view value) noexcept {
    if(value.size()!=36)return false;
    for(std::size_t i=0;i<36;++i){
        const char c=value[i];
        if(i==8||i==13||i==18||i==23){
            if(c!='-')return false;
        } else if(!std::isxdigit(static_cast<unsigned char>(c)))return false;
    }
    return value!="00000000-0000-0000-0000-000000000000";
}
bool valid_handoff(const LoginHandoff& data) noexcept {
    if(!valid_uuid(data.agent_id)||!valid_uuid(data.session_id)||
       !host_valid(data.simulator_host)||
       data.simulator_port==0||
       !valid_capability_url(data.seed_capability))return false;
    return true;
}
void Bootstrap::reset() noexcept {
    // All simulator tokens and capability URLs are per-login secrets.
    handoff_={};
    phase_=BootstrapPhase::Offline;
    ++generation_;
}
bool Bootstrap::endpoint_checked(ProbeResult result) noexcept {
    if(phase_!=BootstrapPhase::Offline || !result.secure_route_reached())return false;
    phase_=BootstrapPhase::EndpointChecked;
    return true;
}
bool Bootstrap::login_reply(LoginHandoff info,bool verified_login_reply) {
    if(phase_!=BootstrapPhase::EndpointChecked||!verified_login_reply||
       !valid_handoff(info))return false;
    handoff_=std::move(info);
    phase_=BootstrapPhase::LoginReplyValidated;
    return true;
}
bool Bootstrap::circuit_ack(std::string_view agent,bool verified_simulator_message) noexcept {
    if(phase_!=BootstrapPhase::LoginReplyValidated||
       !verified_simulator_message ||agent!=handoff_.agent_id)return false;
    phase_=BootstrapPhase::CircuitEstablished;
    return true;
}
bool Bootstrap::capability_reply(std::wstring_view url,
                                  bool verified_seed_response) noexcept {
    if(phase_!=BootstrapPhase::CircuitEstablished||!verified_seed_response||
       !valid_capability_url(url))return false;
    phase_=BootstrapPhase::CapabilitiesEstablished;
    return true;
}
bool Bootstrap::region_snapshot(RegionIdentity region,
                                 bool verified_region_event) noexcept {
    if(phase_!=BootstrapPhase::CapabilitiesEstablished||!verified_region_event||
       !valid_uuid(region.region_id)||region.agent_id!=handoff_.agent_id||
       !std::isfinite(region.x)||!std::isfinite(region.y)||
       !std::isfinite(region.z)||region.x<0||region.y<0||
       region.x>=4096.f||region.y>=4096.f)return false;
    phase_=BootstrapPhase::RegionSynced;
    return true;
}
} // namespace kami::protocol
