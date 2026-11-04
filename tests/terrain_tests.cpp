#include "kami/protocol/terrain.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <vector>

namespace {
class Encoder {
public:
    void bits(std::uint32_t number,unsigned count) {
        for(unsigned i=count;i>0;--i){
            const unsigned bit=(number>>(i-1U))&1U;
            if(position_%8U==0U)data_.push_back(0);
            data_.back()|=static_cast<std::uint8_t>(
                bit<<(7U-(position_&7U)));
            ++position_;
        }
    }
    void little(std::uint32_t number,unsigned bytes) {
        for(unsigned i=0;i<bytes;++i)
            bits((number>>(8U*i))&255U,8);
    }
    [[nodiscard]] std::vector<std::uint8_t> take() {return data_;}
private:
    std::vector<std::uint8_t> data_{};
    std::size_t position_=0;
};
kami::protocol::lludp::Packet test_packet(unsigned size=16U) {
    Encoder e{};
    e.little(size,2); // GroupHeader.stride
    e.bits(size,8);   // GroupHeader.patch_size
    e.bits(76,8);     // GroupHeader.layer_type = land
    e.bits(0x40,8);   // prequant 6, wordbits 2
    float offset=20.f;
    std::uint32_t ieee=0;
    std::memcpy(&ieee,&offset,sizeof(ieee));
    e.little(ieee,4);
    e.little(4,2);  // elevation range 4m
    const unsigned id=(3U<<5U)|4U; // patch index (3,4)
    e.bits(id&255U,8);
    e.bits(id>>8U,2);
    e.bits(2,2); // zero EOB: all DCT coefficients are zero
    e.bits(97,8); // END_OF_PATCHES
    auto data=e.take();
    kami::protocol::lludp::Packet p;
    p.body={11,76,static_cast<std::uint8_t>(data.size()),
            static_cast<std::uint8_t>(data.size()>>8U)};
    p.body.insert(p.body.end(),data.begin(),data.end());
    return p;
}
}
int main() {
    using namespace kami::protocol;
    for(const unsigned size:{16U,32U}){
        auto packet=test_packet(size);
        std::vector<terrain::Patch> patches;
        if(!terrain::decode_land_layer(packet,patches)||patches.size()!=1U)
            return 1;
        const auto& patch=patches[0];
        if(patch.size!=size||patch.x!=3U||patch.y!=4U)return 2;
        for(unsigned i=0;i<size*size;++i) {
            if(!std::isfinite(patch.elevation[i])||
               std::fabs(patch.elevation[i]-22.f)>0.0001f)return 3;
        }
        auto invalid=packet;
        invalid.body[1]=55; // wind must never enter terrain decoder
        if(terrain::decode_land_layer(invalid,patches)||
           !patches.empty())return 4;
        invalid=packet;
        invalid.body.pop_back(); // truncated
        if(terrain::decode_land_layer(invalid,patches)||
           !patches.empty())return 5;
        invalid=packet;
        invalid.body[2]=0xff; // invalid data length
        if(terrain::decode_land_layer(invalid,patches)||
           !patches.empty())return 6;
        invalid=packet;
        invalid.body[6]=17; // invalid patch size
        if(terrain::decode_land_layer(invalid,patches)||
           !patches.empty())return 7;
    }
    // Real layer frames may be zerocoded at UDP level before decoding.
    auto packet=test_packet();
    packet.flags=lludp::kZerocoded;
    packet.sequence=51;
    std::vector<std::uint8_t> wire;
    lludp::Packet roundTrip{};
    if(!lludp::encode(packet,wire)||!lludp::decode(wire,roundTrip))
        return 8;
    std::vector<terrain::Patch> output;
    if(!terrain::decode_land_layer(roundTrip,output)||
       output.size()!=1U)return 9;
    return 0;
}
