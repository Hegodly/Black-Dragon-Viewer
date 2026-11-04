#include "kami/protocol/mesh_reference.hpp"
#include <algorithm>
#include <array>

namespace kami::protocol::mesh {
namespace {
std::uint16_t le16(std::span<const std::uint8_t> p,std::size_t at) noexcept {
    return static_cast<std::uint16_t>(p[at])|
        static_cast<std::uint16_t>(p[at+1U])<<8U;
}
std::uint32_t le32(std::span<const std::uint8_t> p,std::size_t at) noexcept {
    return static_cast<std::uint32_t>(p[at])|
        static_cast<std::uint32_t>(p[at+1U])<<8U|
        static_cast<std::uint32_t>(p[at+2U])<<16U|
        static_cast<std::uint32_t>(p[at+3U])<<24U;
}
std::string uuid(std::span<const std::uint8_t> raw) {
    constexpr char hex[]="0123456789abcdef";
    std::string result;result.reserve(36U);
    for(std::size_t i=0;i<16U;++i){
        if(i==4U||i==6U||i==8U||i==10U)result.push_back('-');
        result.push_back(hex[raw[i]>>4U]);
        result.push_back(hex[raw[i]&15U]);
    }
    return result;
}
} // namespace
ExtraParamsInspection inspect_extra_params(
    std::span<const std::uint8_t> extras) noexcept {
    ExtraParamsInspection out{};
    if(extras.empty()||extras.size()>8192U)return out;
    const unsigned count=extras[0];
    if(count>32U)return out;
    std::size_t at=1U;
    for(unsigned i=0;i<count;++i){
        if(at>extras.size()||extras.size()-at<6U)return {};
        const std::uint16_t kind=le16(extras,at);
        const std::size_t length=le32(extras,at+2U);
        at+=6U;
        if(length>extras.size()-at||length>4096U)return {};
        const auto value=extras.subspan(at,length);
        ++out.records;
        if(kind==0x30U){
            if(length!=17U)return {};
            ++out.sculpt_records;
            if((value[16]&0x3fU)==5U&&
               std::any_of(value.begin(),value.begin()+16U,
                           [](std::uint8_t x){return x!=0U;}))
                out.mesh_id=uuid(value.first(16U));
        }
        at+=length;
    }
    if(at!=extras.size())return {};
    out.valid=true;
    return out;
}
std::string mesh_asset_from_extras(std::span<const std::uint8_t> extras)
    noexcept {
    return inspect_extra_params(extras).mesh_id;
}
} // namespace kami::protocol::mesh
