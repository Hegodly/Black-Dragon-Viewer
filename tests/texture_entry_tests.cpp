#include "kami/protocol/texture_entry.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>
int main(){
    using kami::protocol::texture::Base;
    using kami::protocol::texture::decode_base;
    std::vector<std::uint8_t> b(16U,0x12U);
    b.push_back(0U); // no face-specific image UUIDs
    b.insert(b.end(),{255U,128U,0U,0U}); // packed inverted R,G,B,A
    Base parsed{};
    if(!decode_base(b,parsed)||parsed.image_id[0]!=0x12U||
       parsed.red!=0.f||std::abs(parsed.green-127.f/255.f)>.0001f||
       parsed.blue!=1.f||parsed.alpha!=1.f)return 1;
    std::vector<std::uint8_t> faces(16U,0x10U);
    faces.push_back(0x81U);faces.push_back(0x00U); // bit #7
    faces.insert(faces.end(),16U,0xa0U); // face-specific UUID
    faces.push_back(0U); // image exceptions terminated
    faces.insert(faces.end(),{0U,0U,0U,0U}); // white opaque default
    if(!decode_base(faces,parsed)||parsed.red!=1.f||
       parsed.green!=1.f||parsed.alpha!=1.f||
       parsed.image_id[0]!=0x10U)return 2;
    auto invalid=faces;
    invalid[16]=0x80U;invalid[17]=0x80U;
    if(decode_base(invalid,parsed))return 3;
    invalid=b;invalid.pop_back();invalid.pop_back();
    if(decode_base(invalid,parsed))return 4;
    invalid=b;invalid[16]=0x01U;
    if(decode_base(invalid,parsed))return 5;
    invalid.assign(16U,0U);
    invalid.push_back(0x80U); // unterminated face mask
    if(decode_base(invalid,parsed))return 6;
    if(decode_base({},parsed))return 7;
    Base id{};
    for(unsigned i=0;i<16U;++i)
        id.image_id[i]=static_cast<std::uint8_t>(i+1U);
    if(kami::protocol::texture::default_image_uuid(id)!=
       "01020304-0506-0708-090a-0b0c0d0e0f10")return 8;
    id={};
    if(!kami::protocol::texture::default_image_uuid(id).empty())return 9;
    return 0;
}
