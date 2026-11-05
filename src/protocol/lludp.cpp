#include "kami/protocol/lludp.hpp"
#include "kami/protocol/official_lludp_zerocode.h"
#include <algorithm>
#include <array>
#include <limits>

namespace kami::protocol::lludp {
namespace {
constexpr std::size_t kMaxWire=8192U;
constexpr std::size_t kMaxBody=16384U;
std::uint32_t read_be32(std::span<const std::uint8_t> p,std::size_t at) noexcept {
    return (static_cast<std::uint32_t>(p[at])<<24)|
           (static_cast<std::uint32_t>(p[at+1])<<16)|
           (static_cast<std::uint32_t>(p[at+2])<<8)|
           static_cast<std::uint32_t>(p[at+3]);
}
void write_be32(std::vector<std::uint8_t>& to,std::uint32_t n){
    to.push_back(static_cast<std::uint8_t>(n>>24));
    to.push_back(static_cast<std::uint8_t>(n>>16));
    to.push_back(static_cast<std::uint8_t>(n>>8));
    to.push_back(static_cast<std::uint8_t>(n));
}
// Decode/encode zero-coded LLUDP message bytes using the official Linden
// Lab LLZeroCode algorithm in the separately replaceable LGPL codec DLL.
// The legacy KAMI circuit still handles message templates and ACKs until
// the official simulator backend is integrated.
bool uncompress(std::span<const std::uint8_t> bytes,
                std::vector<std::uint8_t>& out) {
    out.clear();
    if(bytes.empty()||bytes.size()>kMaxWire)return false;
    // Decode into bounded stack storage, not a new 16 KiB heap block
    // for every simulator packet. Copy only the valid decoded region.
    std::array<std::uint8_t,kMaxBody> scratch{};
    const auto written=kami_sl_official_zerocode_decode(
        bytes.data(),static_cast<std::uint32_t>(bytes.size()),
        scratch.data(),static_cast<std::uint32_t>(scratch.size()));
    if(written<=0)return false;
    out.assign(scratch.data(),scratch.data()+written);
    return true;
}
bool compress(std::span<const std::uint8_t> body,
              std::vector<std::uint8_t>& out,bool& compressed) {
    out.clear();
    compressed=false;
    if(body.empty()||body.size()>kMaxBody)return false;
    // Avoid a temporary heap allocation proportional to the maximum
    // encoded packet size. The DLL's bounded encoder owns validation.
    std::array<std::uint8_t,2U*kMaxBody+12U> scratch{};
    const auto written=kami_sl_official_zerocode_encode(
        body.data(),static_cast<std::uint32_t>(body.size()),
        scratch.data(),static_cast<std::uint32_t>(scratch.size()));
    if(written<0)return false;
    if(written==0){
        // Official writer refuses to set the zero-code bit when compression
        // doesn't save bytes. The old KAMI codec set it unconditionally.
        out.assign(body.begin(),body.end());
        return true;
    }
    compressed=true;
    out.assign(scratch.data(),scratch.data()+written);
    return true;
}
} // namespace
bool message_number(std::span<const std::uint8_t> body,
                    std::uint32_t& message_id,unsigned& id_bytes) noexcept {
    if(body.empty())return false;
    if(body[0]!=0xffU){
        if(body[0]==0U)return false;
        message_id=body[0];id_bytes=1;return true;
    }
    if(body.size()<2)return false;
    if(body[1]!=0xffU){
        if(body[1]==0U)return false;
        message_id=0xff00U|body[1];id_bytes=2;return true;
    }
    if(body.size()<4)return false;
    message_id=0xffff0000U|
        (static_cast<std::uint32_t>(body[2])<<8)|body[3];
    id_bytes=4;return true;
}
bool decode(std::span<const std::uint8_t> datagram,Packet& output) {
    if(datagram.size()<7U||datagram.size()>kMaxWire)return false;
    const auto flags=datagram[0];
    if((flags&0x0fU)!=0U)return false;
    const std::size_t extra=datagram[5],start=6U+extra;
    if(start>=datagram.size())return false;
    std::size_t data_end=datagram.size();
    std::vector<std::uint32_t> acks;
    if(flags&kAppendedAcks) {
        const auto count=datagram.back();
        const std::size_t ack_bytes=1U+4U*count;
        if(ack_bytes>datagram.size()-start-1U)return false;
        data_end-=ack_bytes;
        acks.reserve(count);
        for(std::size_t i=0;i<count;++i)
            acks.push_back(read_be32(datagram,data_end+4U*i));
    }
    if(data_end<=start)return false;
    std::vector<std::uint8_t> body;
    const auto encoded=datagram.subspan(start,data_end-start);
    if(flags&kZerocoded) {
        if(!uncompress(encoded,body))return false;
    } else {
        if(encoded.size()>kMaxBody)return false;
        body.assign(encoded.begin(),encoded.end());
    }
    std::uint32_t id=0;unsigned size=0;
    if(!message_number(body,id,size))return false;
    Packet result{};
    result.flags=flags;
    result.sequence=read_be32(datagram,1);
    result.extra_header.assign(datagram.begin()+6U,datagram.begin()+start);
    result.body=std::move(body);
    result.appended_acks=std::move(acks);
    output=std::move(result);
    return true;
}
bool encode(const Packet& packet,std::vector<std::uint8_t>& output) {
    if((packet.flags&0x0fU)!=0U||packet.extra_header.size()>255U||
       packet.body.empty()||packet.body.size()>kMaxBody||
       packet.appended_acks.size()>255U||
       ((!packet.appended_acks.empty())&&!(packet.flags&kAppendedAcks)))
       return false;
    std::uint32_t id=0;unsigned id_bytes=0;
    if(!message_number(packet.body,id,id_bytes))return false;
    std::vector<std::uint8_t> data;
    std::uint8_t actual_flags=packet.flags;
    if(packet.flags&kZerocoded){
        bool compressed=false;
        if(!compress(packet.body,data,compressed))return false;
        if(!compressed)actual_flags&=static_cast<std::uint8_t>(~kZerocoded);
    } else {
        data=packet.body;
    }
    std::vector<std::uint8_t> wire;
    wire.reserve(data.size()+packet.extra_header.size()+32U);
    wire.push_back(actual_flags);
    write_be32(wire,packet.sequence);
    wire.push_back(static_cast<std::uint8_t>(packet.extra_header.size()));
    wire.insert(wire.end(),packet.extra_header.begin(),packet.extra_header.end());
    wire.insert(wire.end(),data.begin(),data.end());
    if(packet.flags&kAppendedAcks){
        for(const auto ack:packet.appended_acks)write_be32(wire,ack);
        wire.push_back(static_cast<std::uint8_t>(packet.appended_acks.size()));
    }
    if(wire.size()>kMaxWire)return false;
    output=std::move(wire);
    return true;
}
} // namespace kami::protocol::lludp
