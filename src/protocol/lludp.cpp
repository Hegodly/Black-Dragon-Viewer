#include "kami/protocol/lludp.hpp"
#include <algorithm>
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
bool uncompress(std::span<const std::uint8_t> bytes,
                std::vector<std::uint8_t>& out) {
    out.clear();
    for(std::size_t i=0;i<bytes.size();++i) {
        const auto b=bytes[i];
        if(b!=0U){
            if(out.size()>=kMaxBody)return false;
            out.push_back(b);
        } else {
            if(++i>=bytes.size()||bytes[i]==0U||
               out.size()+bytes[i]>kMaxBody)return false;
            out.insert(out.end(),bytes[i],0U);
        }
    }
    return !out.empty();
}
bool compress(std::span<const std::uint8_t> body,
              std::vector<std::uint8_t>& out){
    out.clear();
    for(std::size_t i=0;i<body.size();) {
        if(body[i]!=0U) {
            out.push_back(body[i++]);
        } else {
            std::size_t count=0;
            while(i<body.size()&&body[i]==0U&&count<255U) {
                ++i;++count;
            }
            out.push_back(0U);
            out.push_back(static_cast<std::uint8_t>(count));
        }
    }
    return !out.empty();
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
    std::vector<std::uint8_t> wire;
    wire.reserve(packet.body.size()+packet.extra_header.size()+32U);
    wire.push_back(packet.flags);
    write_be32(wire,packet.sequence);
    wire.push_back(static_cast<std::uint8_t>(packet.extra_header.size()));
    wire.insert(wire.end(),packet.extra_header.begin(),packet.extra_header.end());
    std::vector<std::uint8_t> data;
    if(packet.flags&kZerocoded){
        if(!compress(packet.body,data))return false;
    } else {
        data=packet.body;
    }
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
