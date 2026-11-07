#include "kami/protocol/simulator_wire.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <utility>

namespace kami::protocol::sim {
namespace {
int hex(char c) noexcept {
    if(c>='0'&&c<='9')return c-'0';
    if(c>='a'&&c<='f')return c-'a'+10;
    if(c>='A'&&c<='F')return c-'A'+10;
    return -1;
}
void le32(std::vector<std::uint8_t>& to,std::uint32_t value){
    for(unsigned n=0;n<4;++n)to.push_back(static_cast<std::uint8_t>(value>>(n*8)));
}
void push_uuid(std::vector<std::uint8_t>& to,
               const std::array<std::uint8_t,16>& id) {
    to.insert(to.end(),id.begin(),id.end());
}
void le_float(std::vector<std::uint8_t>& to,float value) {
    static_assert(sizeof(float)==4);
    std::uint32_t bits=0;
    std::memcpy(&bits,&value,sizeof(bits));
    le32(to,bits);
}
void vector3(std::vector<std::uint8_t>& to,float x,float y,float z) {
    le_float(to,x);le_float(to,y);le_float(to,z);
}
bool accepted(const LoginResult& login) noexcept {
    return login.accepted()&&login.circuit_code!=0&&valid_handoff(login.handoff);
}
}
bool parse_uuid(std::string_view text,
                std::array<std::uint8_t,16>& out) noexcept {
    if(!valid_uuid(text))return false;
    std::array<std::uint8_t,16> tmp{};
    std::size_t n=0;
    for(std::size_t i=0;i<text.size();){
        if(text[i]=='-'){++i;continue;}
        if(i+1>=text.size()||n>=16)return false;
        const int hi=hex(text[i]),lo=hex(text[i+1]);
        if(hi<0||lo<0)return false;
        tmp[n++]=static_cast<std::uint8_t>((hi<<4)|lo);
        i+=2;
    }
    if(n!=16)return false;
    out=tmp;return true;
}
bool use_circuit_code(const LoginResult& login,std::uint32_t seq,
                      lludp::Packet& output) noexcept {
    if(!accepted(login)||seq==0)return false;
    std::array<std::uint8_t,16> agent{},session{};
    if(!parse_uuid(login.handoff.agent_id,agent)||
       !parse_uuid(login.handoff.session_id,session))return false;
    lludp::Packet value;
    value.flags=lludp::kReliable;
    value.sequence=seq;
    value.body={0xff,0xff,0x00,0x03};
    le32(value.body,login.circuit_code);
    push_uuid(value.body,session);
    push_uuid(value.body,agent);
    output=std::move(value);
    return true;
}
bool complete_agent_movement(const LoginResult& login,std::uint32_t seq,
                             lludp::Packet& output) noexcept {
    if(!accepted(login)||seq==0)return false;
    std::array<std::uint8_t,16> agent{},session{};
    if(!parse_uuid(login.handoff.agent_id,agent)||
       !parse_uuid(login.handoff.session_id,session))return false;
    lludp::Packet value;
    value.flags=lludp::kReliable;
    value.sequence=seq;
    value.body={0xff,0xff,0x00,0xf9};
    push_uuid(value.body,agent);
    push_uuid(value.body,session);
    le32(value.body,login.circuit_code);
    output=std::move(value);
    return true;
}
bool region_handshake_reply(const LoginResult& login,std::uint32_t seq,
                            lludp::Packet& output) noexcept {
    if(!accepted(login)||seq==0)return false;
    std::array<std::uint8_t,16> agent{},session{};
    if(!parse_uuid(login.handoff.agent_id,agent)||
       !parse_uuid(login.handoff.session_id,session))return false;
    lludp::Packet value;
    value.flags=lludp::kReliable;
    value.sequence=seq;
    // RegionHandshakeReply Low 149, AgentData and RegionInfo.Flags = 0.
    value.body={0xff,0xff,0x00,0x95};
    push_uuid(value.body,agent);
    push_uuid(value.body,session);
    le32(value.body,0);
    output=std::move(value);
    return true;
}
std::uint32_t movement_flags(bool forward,bool backward,bool right,bool left,
                             bool turn_right,bool turn_left,bool run,
                             bool jump) noexcept {
    std::uint32_t flags=0U;
    if(forward!=backward)flags|=forward?kAtPos:kAtNeg;
    if(right!=left)flags|=right?kLeftNeg:kLeftPos;
    if(turn_right!=turn_left)flags|=turn_right?kTurnRight:kTurnLeft;
    if(jump)flags|=kUpPos;
    if(run&&(flags&(kAtPos|kAtNeg)))flags|=kFastAt;
    return flags;
}
bool controlled_agent_update(const LoginResult& login,
                             const AgentPosition& position,
                             std::uint32_t seq,lludp::Packet& output,
                             std::uint32_t controls,
                             float draw_distance) noexcept {
    if((controls&~kAllowedMovement)!=0U)return false;
    if(!accepted(login)||!position.verified||seq<3U||
       !std::isfinite(position.x)||!std::isfinite(position.y)||
       !std::isfinite(position.z)||position.x<0.f||
       position.y<0.f||position.x>=4096.f||position.y>=4096.f||
       position.z< -4096.f||position.z>8192.f||
       !std::isfinite(draw_distance)||draw_distance<16.f||
       draw_distance>256.f)return false;
    std::array<std::uint8_t,16> agent{},session{};
    if(!parse_uuid(login.handoff.agent_id,agent)||
       !parse_uuid(login.handoff.session_id,session))return false;
    lludp::Packet value;
    value.flags=lludp::kZerocoded; // AgentUpdate High 4
    value.sequence=seq;
    value.body={0x04};
    push_uuid(value.body,agent);
    push_uuid(value.body,session);
    // Wire LLQuaternion = 3 little-endian F32s. (0,0,0) means w=1.
    vector3(value.body,0.f,0.f,0.f); // BodyRotation
    vector3(value.body,0.f,0.f,0.f); // HeadRotation
    value.body.push_back(0);        // State: neutral
    // Validated simulator-local coordinates; do not invent a region.
    vector3(value.body,position.x,position.y,position.z+1.6f);
    vector3(value.body,1.f,0.f,0.f); // CameraAtAxis
    vector3(value.body,0.f,1.f,0.f); // CameraLeftAxis
    vector3(value.body,0.f,0.f,1.f); // CameraUpAxis
    le_float(value.body,draw_distance);
    le32(value.body,controls);       // Validated movement or neutral
    value.body.push_back(0);        // Flags: no change
    if(value.body.size()!=115U)return false;
    output=std::move(value);
    return true;
}
bool neutral_agent_update(const LoginResult& login,
                          const AgentPosition& position,
                          std::uint32_t seq,lludp::Packet& output,
                          float draw_distance) noexcept {
    return controlled_agent_update(login,position,seq,output,0U,draw_distance);
}
bool request_missing_objects(const LoginResult& login,
                             std::span<const std::uint32_t> local_ids,
                             std::uint32_t seq,lludp::Packet& output) noexcept {
    if(!accepted(login)||seq<3U||local_ids.empty()||
       local_ids.size()>16U)return false;
    std::array<std::uint8_t,16> agent{},session{};
    if(!parse_uuid(login.handoff.agent_id,agent)||
       !parse_uuid(login.handoff.session_id,session))return false;
    lludp::Packet value{};
    value.flags=lludp::kReliable|lludp::kZerocoded;
    value.sequence=seq;
    value.body={0xff,0x03}; // RequestMultipleObjects Medium 3
    push_uuid(value.body,agent);
    push_uuid(value.body,session);
    value.body.push_back(static_cast<std::uint8_t>(local_ids.size()));
    for(std::uint32_t id:local_ids){
        if(id==0U)return false;
        value.body.push_back(0U); // CacheMissType 0: need full object
        le32(value.body,id);
    }
    output=std::move(value);
    return true;
}
bool move_inventory_item(const LoginResult& login,std::string_view item_id,
                         std::string_view folder_id,std::uint32_t seq,
                         lludp::Packet& output) noexcept {
    if(!accepted(login)||seq==0U)return false;
    std::array<std::uint8_t,16> agent{},session{},item{},folder{};
    if(!parse_uuid(login.handoff.agent_id,agent)||
       !parse_uuid(login.handoff.session_id,session)||
       !parse_uuid(item_id,item)||!parse_uuid(folder_id,folder))return false;
    // Official MoveInventoryItem Low 268. One variable InventoryData block,
    // Stamp=false (no creation-date rewrite), empty NewName (no rename).
    lludp::Packet value{};
    value.flags=lludp::kReliable|lludp::kZerocoded;
    value.sequence=seq;
    value.body={0xffU,0xffU,0x01U,0x0cU};
    push_uuid(value.body,agent);
    push_uuid(value.body,session);
    value.body.push_back(0U); // Stamp
    value.body.push_back(1U); // one InventoryData block
    push_uuid(value.body,item);
    push_uuid(value.body,folder);
    value.body.push_back(0U); // empty NewName
    if(value.body.size()!=71U)return false;
    output=std::move(value);
    return true;
}
bool packet_ack(std::span<const std::uint32_t> acks,std::uint32_t seq,
                lludp::Packet& output) noexcept {
    if(seq==0||acks.empty()||acks.size()>255)return false;
    lludp::Packet value;
    value.sequence=seq;
    value.body={0xff,0xff,0xff,0xfb,static_cast<std::uint8_t>(acks.size())};
    for(const auto ack:acks)le32(value.body,ack);
    output=std::move(value);return true;
}
bool complete_ping(std::uint8_t ping,std::uint32_t seq,
                   lludp::Packet& output) noexcept {
    if(seq==0)return false;
    lludp::Packet value;
    value.sequence=seq;value.body={0x02,ping};
    output=std::move(value);return true;
}
Inbound classify(const lludp::Packet& packet) noexcept {
    std::uint32_t id=0;unsigned bytes=0;
    if(!lludp::message_number(packet.body,id,bytes))return Inbound::Invalid;
    switch(id){
    case 0x01:return packet.body.size()>=6?Inbound::StartPing:Inbound::Invalid;
    case 0xffff0094U: {
        // Validate the complete fixed RegionInfo block, plus a length-
        // prefixed SimName. A few bytes of spoofed data are not a handshake.
        constexpr std::size_t minimum=4U+4U+1U+1U+16U+1U+4U+
                                      4U+16U+8U*16U+8U*4U;
        if(packet.body.size()<minimum)return Inbound::Invalid;
        const auto sim_name_length=packet.body[9];
        return sim_name_length>0U&&
               packet.body.size()>=minimum+sim_name_length?
               Inbound::RegionHandshake:Inbound::Invalid;
    }
    case 0xffff00faU:return packet.body.size()>=4+16+16+12+12+8+4?
                        Inbound::AgentMovementComplete:Inbound::Invalid;
    case 0xfffffffbU:return packet.body.size()>=5&&
                        packet.body.size()==5U+
                           static_cast<std::size_t>(packet.body[4])*4U?
                        Inbound::PacketAck:Inbound::Invalid;
    default:return Inbound::Other;
    }
}
bool decode_region_metadata(const lludp::Packet& packet,
                            RegionMetadata& out) noexcept {
    out={};
    if(classify(packet)!=Inbound::RegionHandshake)return false;
    const auto& bytes=packet.body;
    // Message id (4), RegionFlags (4), SimAccess (1), SimName length (1).
    constexpr std::size_t kNameAt=10U;
    if(bytes.size()<=kNameAt)return false;
    const std::size_t name_length=bytes[9];
    if(name_length<2||name_length>64||name_length>bytes.size()-kNameAt)
        return false;
    std::string name;
    name.reserve(name_length);
    for(std::size_t i=0;i<name_length;++i){
        const auto ch=bytes[kNameAt+i];
        if(ch==0){
            // NUL is valid only at the end of a length-delimited name.
            if(i+1!=name_length)return false;
            break;
        }
        if(ch<32||ch>126)return false;
        name.push_back(static_cast<char>(ch));
    }
    if(name.empty())return false;
    // SL RegionHandshake RegionInfo: SimOwner (16), IsEstateManager (1)
    // and then a little-endian IEEE754 WaterHeight (4).
    const std::size_t water_at=kNameAt+name_length+16U+1U;
    if(water_at>bytes.size()||bytes.size()-water_at<sizeof(float))
        return false;
    static_assert(sizeof(float)==4);
    float water=0.f;
    std::memcpy(&water,bytes.data()+water_at,sizeof(water));
    if(!std::isfinite(water)||water< -4096.f||water>8192.f)return false;
    out.name=std::move(name);
    out.water_height=water;
    out.verified=true;
    // RegionHandshake: BillableFactor, CacheID, TerrainBase[4], Detail[4],
    // then four corner start heights and four corner ranges.
    const auto detail=water_at+4U+4U+16U+64U;
    if(detail<=bytes.size()&&bytes.size()-detail>=96U){
        constexpr char hex[]="0123456789abcdef";
        bool valid=true;
        for(unsigned i=0U;i<4U;++i){
            std::string id;bool any=false;
            for(unsigned n=0;n<16U;++n){const auto ch=bytes[detail+i*16U+n];any=any||ch!=0U;
                if(n==4U||n==6U||n==8U||n==10U)id+='-';id+=hex[ch>>4U];id+=hex[ch&15U];}
            out.terrain_assets[i]=any?id:"";
            std::memcpy(&out.terrain_start[i],bytes.data()+detail+64U+i*4U,4U);
            std::memcpy(&out.terrain_range[i],bytes.data()+detail+80U+i*4U,4U);
            valid=valid&&std::isfinite(out.terrain_start[i])&&std::isfinite(out.terrain_range[i])&&
                out.terrain_start[i]>=-4096.f&&out.terrain_start[i]<=8192.f&&
                out.terrain_range[i]>=0.f&&out.terrain_range[i]<=8192.f;
        }
        out.terrain_metadata=valid;
    }
    return true;
}
bool decode_agent_movement_position(const lludp::Packet& packet,
                                    const LoginResult& login,
                                    AgentPosition& out) noexcept {
    out={};
    if(!movement_complete_identifies(packet,login))return false;
    // AgentMovementComplete Low 250: AgentID + SessionID, followed
    // by the three little-endian F32 Position components (offset 36).
    constexpr std::size_t at=4U+16U+16U;
    if(packet.body.size()<at+3U*sizeof(float))return false;
    AgentPosition position{};
    static_assert(sizeof(float)==4);
    std::memcpy(&position.x,packet.body.data()+at,sizeof(float));
    std::memcpy(&position.y,packet.body.data()+at+4U,sizeof(float));
    std::memcpy(&position.z,packet.body.data()+at+8U,sizeof(float));
    if(!std::isfinite(position.x)||!std::isfinite(position.y)||
       !std::isfinite(position.z)||position.x<0.f||position.y<0.f||
       position.x>=4096.f||position.y>=4096.f||
       position.z< -4096.f||position.z>8192.f)return false;
    position.verified=true;
    out=position;
    return true;
}
bool movement_complete_identifies(const lludp::Packet& packet,
                                  const LoginResult& login) noexcept {
    if(classify(packet)!=Inbound::AgentMovementComplete||
       !accepted(login))return false;
    std::array<std::uint8_t,16> agent{},session{};
    if(!parse_uuid(login.handoff.agent_id,agent)||
       !parse_uuid(login.handoff.session_id,session))return false;
    return std::equal(agent.begin(),agent.end(),packet.body.begin()+4)&&
           std::equal(session.begin(),session.end(),packet.body.begin()+20);
}
} // namespace kami::protocol::sim

