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
    if(!parsed.face_image_override[7U]||
       parsed.face_image_override[8U]||
       kami::protocol::texture::face_image_uuid(parsed,7U)!=
          "a0a0a0a0-a0a0-a0a0-a0a0-a0a0a0a0a0a0"||
       kami::protocol::texture::face_image_uuid(parsed,8U)!=
          "10101010-1010-1010-1010-101010101010")return 10;
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
    // Authentic TE field order (packTEField in official Second Life):
    // UUID, inverted color, scale S/T F32, offset S/T S16, rot S16.
    // A zero 7-bit face mask separates each field, and an exception
    // modifies only its numbered face rather than all other surfaces.
    std::vector<std::uint8_t> mapped(16U,0x31U);
    const auto add=[&](std::initializer_list<std::uint8_t> x){
        mapped.insert(mapped.end(),x.begin(),x.end());
    };
    add({0U});                  // image UUID exceptions end
    add({0U,0U,0U,0U});         // white/opaque default
    add({0U});                  // color exceptions end
    add({0U,0U,0U,0x40U});      // scale U = 2.0
    add({0x04U});               // face 2 override
    add({0U,0U,0x80U,0x3fU});   // scale U for face 2 = 1.0
    add({0U});                  // end scale U
    add({0U,0U,0x00U,0x3fU});   // scale V = .5
    add({0U});
    add({0U,0x20U});            // U offset = 8192/32767
    add({0U});
    add({0U,0xc0U});            // V offset = -16384/32767
    add({0U});
    add({0U,0x20U});            // rotation = +pi/2
    add({0U});
    if(!decode_base(mapped,parsed)||
       std::abs(parsed.uv[0].repeat_u-2.f)>1.e-5f||
       std::abs(parsed.uv[1].repeat_u-2.f)>1.e-5f||
       std::abs(parsed.uv[2].repeat_u-1.f)>1.e-5f||
       std::abs(parsed.uv[0].repeat_v-.5f)>1.e-5f||
       std::abs(parsed.uv[0].offset_u-.2500076f)>1.e-4f||
       std::abs(parsed.uv[0].offset_v+.5000153f)>1.e-4f||
       std::abs(parsed.uv[0].rotation-1.5707963f)>1.e-4f||
       parsed.rgba[0][0]!=1.f)return 11;
    auto truncated=mapped;
    truncated.pop_back(); // missing final TE rotation mask
    if(decode_base(truncated,parsed))return 12;
    // Keep the original output unchanged on an invalid TE decode.
    const float original_repeat=parsed.uv[2].repeat_u;
    if(decode_base(truncated,parsed)||
       parsed.uv[2].repeat_u!=original_repeat)return 13;
    return 0;
}
