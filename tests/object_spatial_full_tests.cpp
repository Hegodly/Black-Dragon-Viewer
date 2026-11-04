#include "kami/protocol/object_spatial.hpp"
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

namespace {
using Byte=std::uint8_t;
using Bytes=std::vector<Byte>;
void u16(Bytes& b,std::uint16_t v) {
    b.push_back(static_cast<Byte>(v));
    b.push_back(static_cast<Byte>(v>>8U));
}
void u32(Bytes& b,std::uint32_t v) {
    for(unsigned i=0;i<4U;++i)b.push_back(
        static_cast<Byte>(v>>(i*8U)));
}
void f32(Bytes& b,float v) {
    std::uint32_t bits=0;
    std::memcpy(&bits,&v,sizeof(bits));
    u32(b,bits);
}
void var1(Bytes& b,const Bytes& data={}) {
    b.push_back(static_cast<Byte>(data.size()));
    b.insert(b.end(),data.begin(),data.end());
}
void var2(Bytes& b,const Bytes& data={}) {
    u16(b,static_cast<std::uint16_t>(data.size()));
    b.insert(b.end(),data.begin(),data.end());
}
Bytes block(std::uint32_t id,std::uint32_t parent=0,
            Byte pcode=9,float x=128.f) {
    Bytes b;
    u32(b,id); // LocalID
    b.push_back(0U); // State
    b.insert(b.end(),16U,0U); // UUID
    u32(b,1U); // CRC
    b.push_back(pcode); // Volume=9
    b.push_back(0U);b.push_back(0U); // material/action
    f32(b,2.f);f32(b,3.f);f32(b,4.f); // scale
    Bytes pos;
    f32(pos,x);f32(pos,140.f);f32(pos,35.f);
    pos.insert(pos.end(),48U,0U); // 60-byte full precision
    var1(b,pos);
    u32(b,parent);u32(b,0U); // ParentID and UpdateFlags
    b.insert(b.end(),23U,0U); // Prim path/profile shape
    var2(b);var1(b);var2(b);var2(b);var1(b);
    b.insert(b.end(),4U,0U); // TextColor
    var1(b);var1(b);var1(b); // MediaURL, particles, extra
    b.insert(b.end(),66U,0U); // sound + joint data
    return b;
}
kami::protocol::lludp::Packet packet(const std::vector<Bytes>& blocks) {
    kami::protocol::lludp::Packet p{};
    p.body.push_back(12U); // ObjectUpdate High 12
    p.body.insert(p.body.end(),10U,0U); // RegionData
    p.body.push_back(static_cast<Byte>(blocks.size()));
    for(const auto& b:blocks)
        p.body.insert(p.body.end(),b.begin(),b.end());
    return p;
}
}
int main(){
    using namespace kami::protocol;
    std::vector<objectspatial::Transform> out;
    auto live=packet({block(77U),block(78U,77U)});
    if(!objectspatial::decode_full_transforms(live,out)||
       out.size()!=1U||out[0].local_id!=77U||
       out[0].x!=128.f||out[0].y!=140.f||out[0].z!=35.f||
       out[0].sx!=2.f||out[0].sy!=3.f||out[0].sz!=4.f)return 1;
    // Full updates can have more than one variable-length field. Guard
    // against accidentally using an offset into the preceding block.
    auto second=packet({block(77U),block(80U,0,9U,222.f)});
    if(!objectspatial::decode_full_transforms(second,out)||
       out.size()!=2U||out[1].local_id!=80U||
       out[1].x!=222.f)return 2;
    auto bot=packet({block(81U,0,47U)});
    if(!objectspatial::decode_full_transforms(bot,out)||
       !out.empty())return 3;
    auto badpos=packet({block(82U,0,9U,800.f)});
    if(!objectspatial::decode_full_transforms(badpos,out)||
       !out.empty())return 4;
    auto invalid=live;
    invalid.body.pop_back();
    if(objectspatial::decode_full_transforms(invalid,out))return 5;
    invalid=live;
    invalid.body[52]=255U; // 1-byte motion payload length no longer fits
    if(objectspatial::decode_full_transforms(invalid,out))return 6;
    invalid=packet({block(83U)});
    invalid.body[0]=13U; // Different wire family, reject entirely
    if(objectspatial::decode_full_transforms(invalid,out))return 7;
    return 0;
}
