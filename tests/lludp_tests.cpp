#include "kami/protocol/lludp.hpp"
#include <array>
#include <cstdint>
#include <vector>
using namespace kami::protocol::lludp;
int main(){
    Packet p{};
    p.flags=kReliable|kAppendedAcks|kZerocoded;
    p.sequence=0x12345678U;
    p.extra_header={0x12U,0x34U};
    p.body={0xffU,0xffU,0xffU,0xfbU,1U,0U,0U,0U,7U,9U};
    p.appended_acks={0x01020304U,0xaabbccddU};
    std::vector<std::uint8_t> bytes{};
    if(!encode(p,bytes)||bytes.size()<15U)return 1;
    if(bytes[0]!=(kReliable|kAppendedAcks|kZerocoded)||
       bytes[5]!=2U||bytes[1]!=0x12U||bytes[4]!=0x78U)return 2;
    Packet read{};
    if(!decode(bytes,read)||read.sequence!=p.sequence||
       read.flags!=p.flags||read.extra_header!=p.extra_header||
       read.body!=p.body||read.appended_acks!=p.appended_acks)return 3;
    std::uint32_t id=0;unsigned n=0;
    if(!message_number(read.body,id,n)||id!=0xfffffffbU||n!=4U)return 4;
    if(!message_number(std::array<std::uint8_t,1>{0xfeU},id,n)||
       id!=0xfeU||n!=1U)return 5;
    if(!message_number(std::array<std::uint8_t,2>{0xffU,0x13U},id,n)||
       id!=0xff13U||n!=2U)return 6;
    if(message_number(std::array<std::uint8_t,1>{0xffU},id,n)||
       message_number(std::array<std::uint8_t,1>{0U},id,n)||
       message_number(std::array<std::uint8_t,3>{0xffU,0xffU,1U},id,n))
       return 7;
    auto bad=bytes;
    bad.pop_back();
    if(decode(bad,read))return 8; // corrupted ACK count
    bad=bytes;bad[5]=254U;
    if(decode(bad,read))return 9; // invalid extra header length
    bad=bytes;bad[0]|=0x01U;
    if(decode(bad,read))return 10; // forbidden reserved flags
    p.flags=kReliable;
    p.appended_acks.clear();
    p.body={1U,0U,0U,12U};
    if(!encode(p,bytes)||!decode(bytes,read)||read.body!=p.body)return 11;
    p.flags=kZerocoded;
    p.body={0xffU,0xffU,0x11U,0x08U,0U,0U,0U,0U};
    if(!encode(p,bytes)||!decode(bytes,read)||read.body!=p.body)return 12;
    bad={kZerocoded,0U,0U,0U,1U,0U,1U,0U}; // unterminated zero run
    if(decode(bad,read))return 13;
    bad={kZerocoded,0U,0U,0U,1U,0U,1U,0U,0U};
    if(decode(bad,read))return 14; // zero repetition count
    std::vector<std::uint8_t> oversized(8200U,0U);
    if(decode(oversized,read))return 15;
    p.flags=kAppendedAcks;p.body={0xfeU};p.appended_acks={5U};
    if(!encode(p,bytes)||!decode(bytes,read)||
       read.appended_acks.size()!=1U||read.appended_acks.front()!=5U)
       return 16;
    return 0;
}
