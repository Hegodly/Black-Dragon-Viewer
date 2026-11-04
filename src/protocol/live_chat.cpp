#include "kami/protocol/live_chat.hpp"
#include "kami/protocol/simulator_wire.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <windows.h>
#include <vector>

namespace kami::protocol::sim {
namespace {
bool to_wide(std::string_view utf8,std::wstring& out,std::size_t max_chars){
    if(utf8.empty()||utf8.size()>2048U||utf8.find('\0')!=std::string_view::npos)
        return false;
    const int needed=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,
        utf8.data(),static_cast<int>(utf8.size()),nullptr,0);
    if(needed<=0||static_cast<std::size_t>(needed)>max_chars)return false;
    std::wstring result(static_cast<std::size_t>(needed),L'\0');
    if(MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,utf8.data(),
          static_cast<int>(utf8.size()),result.data(),needed)!=needed)return false;
    for(auto& ch:result){
        if(ch==L'\r'||ch==L'\n'||ch==L'\t')ch=L' ';
        if(ch<0x20)return false;
    }
    out=std::move(result);
    return true;
}
bool to_utf8(std::wstring_view text,std::string& out){
    if(text.empty()||text.size()>512U)return false;
    const auto begin=text.find_first_not_of(L" \t");
    if(begin==std::wstring_view::npos)return false;
    const auto end=text.find_last_not_of(L" \t");
    text=text.substr(begin,end-begin+1U);
    for(const wchar_t ch:text)
        if(ch<0x20||ch==0x7f)return false;
    const int needed=WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,
        text.data(),static_cast<int>(text.size()),nullptr,0,nullptr,nullptr);
    if(needed<=0||needed>1022)return false;
    std::string result(static_cast<std::size_t>(needed),'\0');
    if(WideCharToMultiByte(CP_UTF8,WC_ERR_INVALID_CHARS,text.data(),
        static_cast<int>(text.size()),result.data(),needed,nullptr,nullptr)!=needed)
        return false;
    out=std::move(result);
    return true;
}
void le16(std::vector<std::uint8_t>& out,std::uint16_t v){
    out.push_back(static_cast<std::uint8_t>(v&0xffU));
    out.push_back(static_cast<std::uint8_t>(v>>8));
}
} // namespace
bool decode_simulator_chat(const lludp::Packet& packet,ChatLine& out){
    const auto& b=packet.body;
    // Low 139 + FromName Var1 + UUIDs + SourceType/ChatType/Audible
    // + 12-byte position + Message Var2.
    if(b.size()<4U+1U+16U+16U+3U+12U+2U||
       b[0]!=0xffU||b[1]!=0xffU||b[2]!=0U||b[3]!=139U)return false;
    std::size_t at=4U;
    const std::size_t name_bytes=b[at++];
    if(!name_bytes||name_bytes>128U||name_bytes>b.size()-at)return false;
    std::string_view name(reinterpret_cast<const char*>(b.data()+at),
                          name_bytes);
    if(name.back()=='\0')name.remove_suffix(1);
    at+=name_bytes;
    if(b.size()-at<16U+16U+3U+12U+2U)return false;
    at+=32U; // SourceID and OwnerID are not copied to UI/history.
    const auto source=b[at++],type=b[at++],audible=b[at++];
    if(source>2U||audible==255U||!((type<=3U)||type==6U||type==8U))
        return false;
    at+=12U; // region-local position, unnecessary for chat UI
    const std::size_t msg_size=static_cast<std::size_t>(b[at])|
                               (static_cast<std::size_t>(b[at+1U])<<8U);
    at+=2U;
    if(!msg_size||msg_size>2048U||b.size()-at!=msg_size)return false;
    std::string_view msg(reinterpret_cast<const char*>(b.data()+at),
                         msg_size);
    if(msg.back()=='\0')msg.remove_suffix(1);
    ChatLine result{};
    if(!to_wide(name,result.sender,128U)||
       !to_wide(msg,result.message,512U))return false;
    result.source_type=source;
    result.chat_type=type;
    out=std::move(result);
    return true;
}
bool build_nearby_chat(const LoginResult& login,std::wstring_view text,
                       std::uint32_t sequence,lludp::Packet& out){
    if(!login.accepted()||sequence<3U||!valid_handoff(login.handoff))
        return false;
    std::array<std::uint8_t,16> agent{},session{};
    if(!parse_uuid(login.handoff.agent_id,agent)||
       !parse_uuid(login.handoff.session_id,session))return false;
    std::string message;
    if(!to_utf8(text,message))return false;
    lludp::Packet built{};
    built.sequence=sequence;
    built.flags=lludp::kReliable;
    built.body={0xff,0xff,0x00,80}; // Low 80 ChatFromViewer
    built.body.insert(built.body.end(),agent.begin(),agent.end());
    built.body.insert(built.body.end(),session.begin(),session.end());
    le16(built.body,static_cast<std::uint16_t>(message.size()+1U));
    built.body.insert(built.body.end(),message.begin(),message.end());
    built.body.push_back(0U); // encoded SL string terminator
    built.body.push_back(1U); // CHAT_TYPE_NORMAL
    built.body.insert(built.body.end(),4U,0U); // channel 0
    out=std::move(built);
    return true;
}
} // namespace kami::protocol::sim
