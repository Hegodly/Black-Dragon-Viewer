#include "kami/protocol/official_lludp_zerocode.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>
int main() {
    const std::array<std::uint8_t,6> body{0xfeU,0U,0U,0U,0U,0x13U};
    std::array<std::uint8_t,128> encoded{};
    const auto n=kami_sl_official_zerocode_encode(
        body.data(),static_cast<std::uint32_t>(body.size()),
        encoded.data(),static_cast<std::uint32_t>(encoded.size()));
    if(n<=0||n>=static_cast<int>(body.size()))return 1;
    std::array<std::uint8_t,128> decoded{};
    const auto decoded_bytes=kami_sl_official_zerocode_decode(
        encoded.data(),static_cast<std::uint32_t>(n),
        decoded.data(),static_cast<std::uint32_t>(decoded.size()));
    if(decoded_bytes!=static_cast<int>(body.size())||
       !std::equal(body.begin(),body.end(),decoded.begin()))return 2;
    if(kami_sl_official_zerocode_decode(
            encoded.data(),static_cast<std::uint32_t>(n),
            decoded.data(),1U)!=-1)return 3;
    const std::array<std::uint8_t,4> unhelpful{0xfeU,1U,2U,3U};
    if(kami_sl_official_zerocode_encode(
            unhelpful.data(),static_cast<std::uint32_t>(unhelpful.size()),
            encoded.data(),static_cast<std::uint32_t>(encoded.size()))!=0)
        return 4;
    // The official encoder emits chunks of at most 255 zeros.
    std::vector<std::uint8_t> long_run(700U,0U);long_run[0]=0xfeU;
    std::vector<std::uint8_t> compressed(long_run.size()*2U);
    const auto size=kami_sl_official_zerocode_encode(
        long_run.data(),static_cast<std::uint32_t>(long_run.size()),
        compressed.data(),static_cast<std::uint32_t>(compressed.size()));
    if(size<=0)return 5;
    std::vector<std::uint8_t> recovered(long_run.size());
    const auto expanded=kami_sl_official_zerocode_decode(
        compressed.data(),static_cast<std::uint32_t>(size),
        recovered.data(),static_cast<std::uint32_t>(recovered.size()));
    if(expanded!=static_cast<int>(long_run.size())||
       recovered!=long_run)return 6;
    const std::array<std::uint8_t,2> malformed{0U,0U};
    if(kami_sl_official_zerocode_decode(
        malformed.data(),2U,decoded.data(),
        static_cast<std::uint32_t>(decoded.size()))!=-1)return 7;
    if(kami_sl_official_zerocode_decode(nullptr,1U,decoded.data(),128U)!=-1)
        return 8;
    return 0;
}
