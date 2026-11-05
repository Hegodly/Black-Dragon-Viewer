#include "kami/protocol/mesh_reference.hpp"
#include "kami/protocol/official_extra_params.h"
#include <algorithm>
#include <array>
#include <span>
#include <string>

namespace kami::protocol::mesh {
std::string mesh_asset_from_extras(std::span<const std::uint8_t> extras)
    noexcept {
    if(extras.size()>8192U)return {};
    KamiSlExtraParamsV1 summary{};
    summary.struct_size=sizeof(summary);
    const auto* bytes=extras.empty()?nullptr:extras.data();
    if(!kami_sl_decode_extra_params_v1(
            bytes,static_cast<std::uint32_t>(extras.size()),&summary)||
       summary.mesh_sculpt==0U)return {};
    constexpr char hex[]="0123456789abcdef";
    std::string result;
    result.reserve(36U);
    for(std::size_t i=0U;i<16U;++i){
        const auto ch=summary.mesh_asset_uuid[i];
        if(i==4U||i==6U||i==8U||i==10U)result.push_back('-');
        result.push_back(hex[ch>>4U]);
        result.push_back(hex[ch&15U]);
    }
    return result;
}
} // namespace kami::protocol::mesh
