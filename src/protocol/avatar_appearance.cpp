#include "kami/protocol/avatar_appearance.hpp"
#include <algorithm>
#include <span>

namespace kami::protocol::appearance {
bool decode_avatar_appearance(const lludp::Packet& packet,
                              Observation& out) noexcept {
    const std::span<const std::uint8_t> b(packet.body);
    // Low 158, Sender UUID + IsTrial U8 + ObjectData.TextureEntry Var2
    // + VisualParam count U8 + each ParamValue U8.
    if(b.size()<24U||b.size()>8192U||b[0]!=0xffU||
       b[1]!=0xffU||b[2]!=0U||b[3]!=158U)return false;
    std::size_t at=4U;
    Observation found{};
    std::copy_n(b.begin()+at,16U,found.avatar_id.begin());
    at+=16U;
    if(std::all_of(found.avatar_id.begin(),found.avatar_id.end(),
                   [](std::uint8_t x){return x==0U;}))return false;
    const auto trial=b[at++];
    if(trial>1U)return false;
    found.trial=(trial!=0U);
    const std::size_t entry_bytes=static_cast<std::size_t>(b[at])|
                                  (static_cast<std::size_t>(b[at+1U])<<8U);
    at+=2U;
    if(entry_bytes>b.size()-at||entry_bytes>4096U)return false;
    if(entry_bytes>=16U)
        found.texture_entry_present=std::any_of(
            b.begin()+at,b.begin()+at+16U,
            [](std::uint8_t x){return x!=0U;});
    found.texture_entry_size=static_cast<std::uint16_t>(entry_bytes);
    std::copy_n(b.begin()+at,entry_bytes,found.texture_entry.begin());
    at+=entry_bytes;
    if(at>=b.size())return false;
    const auto count=b[at++];
    if(static_cast<std::size_t>(count)>b.size()-at)return false;
    found.visual_parameter_count=count;
    std::copy_n(b.begin()+at,count,found.visual_parameters.begin());
    at+=count;
    // Newer SL templates may append fields; do not pretend to decode them.
    found.extra_data_present=(at!=b.size());
    out=found;
    return true;
}
} // namespace kami::protocol::appearance
