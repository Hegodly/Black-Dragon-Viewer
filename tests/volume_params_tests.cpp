#include "kami/protocol/volume_params.hpp"
#include <array>
#include <cmath>
#include <cstdio>
using kami::protocol::volume::Parameters;
int main(){
    std::array<std::uint8_t,23> h13{};
    h13[0]=0x10U;h13[16]=0x01U;
    h13[5]=50U;h13[6]=150U;
    h13[1]=0U;h13[2]=0U;h13[3]=0U;h13[4]=0U;
    Parameters p{};
    if(!kami::protocol::volume::decode(h13,false,p)||p.full_layout||
       p.path_curve!=0x10U||p.profile_curve!=0x01U||
       std::abs(p.scale_x-1.5f)>.0001f||
       std::abs(p.scale_y-.5f)>.0001f)return 1;
    std::array<std::uint8_t,23> h12{};
    h12[0]=0x10U;h12[1]=0x01U;
    h12[6]=50U;h12[7]=150U;
    Parameters q{};
    if(!kami::protocol::volume::decode(h12,true,q)||!q.full_layout||
       q.path_curve!=0x10U||q.profile_curve!=0x01U||
       std::abs(q.scale_x-1.5f)>.0001f||
       std::abs(q.scale_y-.5f)>.0001f)return 2;
    h13[1]=0x10U;h13[2]=0x27U; // 1.0 path begin
    h13[3]=0x20U;h13[4]=0x4eU; // 2.0 -> invalid end clamped to 1
    if(kami::protocol::volume::decode(h13,false,p))return 3;
    h13[1]=0U;h13[2]=0U;h13[3]=0U;h13[4]=0U;
    h13[5]=201U;
    if(kami::protocol::volume::decode(h13,false,p))return 4;
    std::array<std::uint8_t,22> short_data{};
    if(kami::protocol::volume::decode(short_data,false,p))return 5;
    return 0;
}
