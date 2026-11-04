#include "kami/protocol/world_wire.hpp"
#include <span>

namespace kami::protocol::worldwire {
namespace {
std::uint16_t le16(std::span<const std::uint8_t> bytes,
                   std::size_t at) noexcept {
    return static_cast<std::uint16_t>(bytes[at])|
           (static_cast<std::uint16_t>(bytes[at+1])<<8);
}
std::uint32_t le32(std::span<const std::uint8_t> bytes,
                   std::size_t at) noexcept {
    return static_cast<std::uint32_t>(bytes[at])|
           (static_cast<std::uint32_t>(bytes[at+1])<<8)|
           (static_cast<std::uint32_t>(bytes[at+2])<<16)|
           (static_cast<std::uint32_t>(bytes[at+3])<<24);
}
} // namespace
Sample inspect(const lludp::Packet& packet) noexcept {
    const std::span<const std::uint8_t> b(packet.body);
    std::uint32_t id=0;unsigned prefix=0;
    if(!lludp::message_number(b,id,prefix))return {Kind::Invalid};
    // Message ids match the official Linden Lab message_template.msg.
    if(prefix!=1U||id<11U||id>16U)return {};
    Sample value{};
    switch(id){
    case 11: {
        value.kind=Kind::LayerData;
        // LayerID.Type U8, LayerData.Data Variable 2.
        if(b.size()<4U)return {Kind::Invalid};
        const std::size_t len=le16(b,2);
        if(len==0U||len>4096U||b.size()!=4U+len)
            return {Kind::Invalid};
        value.blocks=1;value.payload_bytes=len;
        value.complete_frame=true;
        return value;
    }
    case 12: {
        value.kind=Kind::ObjectFull;
        // RegionData: RegionHandle U64, TimeDilation U16.
        // ObjectData: count byte + large variable-length message-template
        // blocks. This stage only recognizes the bounded outer header.
        if(b.size()<12U)return {Kind::Invalid};
        const unsigned count=b[11];
        if(count==0||count>64||b.size()<12U+count*25U)
            return {Kind::Invalid};
        value.blocks=count;
        value.payload_bytes=b.size()-12U;
        value.first_local_id=le32(b,12);
        value.has_local_id=true;
        // ObjectUpdate fields have not been fully decoded. Never upload
        // this partial data to the renderer or treat it as complete.
        return value;
    }
    case 13: {
        value.kind=Kind::ObjectCompressed;
        if(b.size()<12U)return {Kind::Invalid};
        const unsigned count=b[11];
        if(count==0U||count>64U)return {Kind::Invalid};
        std::size_t offset=12U;
        for(unsigned n=0;n<count;++n){
            // UpdateFlags U32 + Data Variable 2.
            if(offset>b.size()||b.size()-offset<6U)
                return {Kind::Invalid};
            const std::size_t len=le16(b,offset+4U);
            if(len==0||len>8192U||len>b.size()-offset-6U)
                return {Kind::Invalid};
            offset+=6U+len;
        }
        if(offset!=b.size())return {Kind::Invalid};
        value.blocks=count;value.payload_bytes=b.size()-12U;
        value.complete_frame=true;
        return value;
    }
    case 14: {
        value.kind=Kind::ObjectCached;
        if(b.size()<12U)return {Kind::Invalid};
        const unsigned count=b[11];
        if(count==0U||count>255U||
           b.size()!=12U+static_cast<std::size_t>(count)*12U)
            return {Kind::Invalid};
        value.blocks=count;value.payload_bytes=b.size()-12U;
        value.first_local_id=le32(b,12U);
        value.has_local_id=true;
        value.complete_frame=true;
        return value;
    }
    case 15: {
        value.kind=Kind::ObjectTerse;
        if(b.size()<12U)return {Kind::Invalid};
        const unsigned count=b[11];
        if(count==0U||count>64U)return {Kind::Invalid};
        std::size_t offset=12U;
        for(unsigned n=0;n<count;++n){
            // ObjectData.Data Variable 1 + TextureEntry Variable 2.
            if(offset>=b.size())return {Kind::Invalid};
            const std::size_t data_len=b[offset++];
            if(data_len==0U||data_len>b.size()-offset)return {Kind::Invalid};
            offset+=data_len;
            if(offset>b.size()||b.size()-offset<2U)return {Kind::Invalid};
            const std::size_t texture_len=le16(b,offset);
            offset+=2U;
            if(texture_len>b.size()-offset)return {Kind::Invalid};
            offset+=texture_len;
        }
        if(offset!=b.size())return {Kind::Invalid};
        value.blocks=count;value.payload_bytes=b.size()-12U;
        value.complete_frame=true;
        return value;
    }
    case 16: {
        value.kind=Kind::ObjectKilled;
        if(b.size()<2U)return {Kind::Invalid};
        const unsigned count=b[1];
        if(count==0U||b.size()!=2U+static_cast<std::size_t>(count)*4U)
            return {Kind::Invalid};
        value.blocks=count;value.payload_bytes=b.size()-2U;
        value.first_local_id=le32(b,2);
        value.has_local_id=true;
        value.complete_frame=true;
        return value;
    }
    default:return {};
    }
}
}
