#include "kami/protocol/avatar_appearance.hpp"
#include <vector>
int main(){
    kami::protocol::lludp::Packet p{};
    p.body={0xff,0xff,0x00,158};
    p.body.insert(p.body.end(),16U,0U);
    p.body[4]=0x41U;
    p.body.push_back(0U); // IsTrial
    p.body.push_back(16U);p.body.push_back(0U); // TE length
    p.body.insert(p.body.end(),16U,0U);
    p.body.back()=0x11U;
    p.body.push_back(3U);p.body.push_back(1U);
    p.body.push_back(2U);p.body.push_back(3U);
    kami::protocol::appearance::Observation out{};
    if(!kami::protocol::appearance::decode_avatar_appearance(p,out)||
       out.avatar_id[0]!=0x41U||out.visual_parameter_count!=3U||
       !out.texture_entry_present||out.extra_data_present||
       out.texture_entry_size!=16U||out.texture_entry[15]!=0x11U||
       out.visual_parameters[0]!=1U||out.visual_parameters[2]!=3U)return 1;
    auto extra=p;extra.body.push_back(0xFFU);
    if(!kami::protocol::appearance::decode_avatar_appearance(extra,out)||
       !out.extra_data_present)return 2;
    auto truncated=p;truncated.body.resize(23U);
    if(kami::protocol::appearance::decode_avatar_appearance(truncated,out))
        return 3;
    auto invalid=p;invalid.body[0]=0U;
    if(kami::protocol::appearance::decode_avatar_appearance(invalid,out))
        return 4;
    invalid=p;invalid.body[22]=0xffU;invalid.body[23]=0xffU;
    if(kami::protocol::appearance::decode_avatar_appearance(invalid,out))
        return 5;
    return 0;
}
