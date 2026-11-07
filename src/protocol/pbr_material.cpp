#include "kami/protocol/pbr_material.hpp"
#include "picojson.h"
#include "tinyxml2.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <limits>
#include <stdexcept>
namespace kami::protocol::pbr {
namespace {
using J=picojson::value;
constexpr std::uint32_t BC=1U,EC=2U,MF=4U,RF=8U,AM=16U,AC=32U,DS=64U;
const J* get(const J& j,const char* key){
    if(!j.is<picojson::object>())return nullptr;
    const auto& o=j.get<picojson::object>();const auto i=o.find(key);
    return i==o.end()||i->second.is<picojson::null>()?nullptr:&i->second;
}
float real(const J& j,float lo,float hi){
    if(!j.is<double>())throw std::runtime_error("number");
    const auto n=j.get<double>();
    if(!std::isfinite(n)||n<lo||n>hi)throw std::runtime_error("range");
    return static_cast<float>(n);
}
unsigned index(const J& j,unsigned limit){
    const auto n=real(j,0.f,float(limit));
    if(std::floor(n)!=n||n>=limit)throw std::runtime_error("index");
    return static_cast<unsigned>(n);
}
template<std::size_t N> void array(const J& j,std::array<float,N>& a,float lo,float hi){
    if(!j.is<picojson::array>()||j.get<picojson::array>().size()!=N)
        throw std::runtime_error("array");
    for(unsigned i=0;i<N;++i)a[i]=real(j.get<picojson::array>()[i],lo,hi);
}
std::string uuid(std::string_view s,bool override_id=false){
    if(s.starts_with("urn:uuid:"))s.remove_prefix(9U);
    if(s.size()!=36U)throw std::runtime_error("uuid");
    bool nonzero=false;
    for(unsigned i=0;i<36U;++i){
        const char c=s[i];
        if(i==8U||i==13U||i==18U||i==23U){if(c!='-')throw std::runtime_error("uuid");}
        else if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')||(c>='A'&&c<='F')))
            throw std::runtime_error("uuid");
        else if(c!='0')nonzero=true;
    }
    if(!nonzero)return {};
    std::string out(s);for(auto& c:out)if(c>='A'&&c<='F')c=char(c-'A'+'a');
    if(override_id&&out=="ffffffff-ffff-ffff-ffff-ffffffffffff")return "clear";
    return out;
}
std::string wire_uuid(std::span<const std::uint8_t> bytes){
    const char* hex="0123456789abcdef";std::string s;
    for(unsigned i=0;i<16U;++i){
        if(i==4U||i==6U||i==8U||i==10U)s+='-';
        s+=hex[bytes[i]>>4U];s+=hex[bytes[i]&15U];
    }return uuid(s);
}
void texture_info(const J& root,const J* info,Slot slot,Material& m){
    if(!info)return;
    const auto* at=get(*info,"index");if(!at)throw std::runtime_error("texture index");
    const auto* textures=get(root,"textures"),*images=get(root,"images");
    if(!textures||!images||!textures->is<picojson::array>()||!images->is<picojson::array>())
        throw std::runtime_error("texture table");
    const auto& ts=textures->get<picojson::array>();const auto& is=images->get<picojson::array>();
    const auto ti=index(*at,static_cast<unsigned>(ts.size()));
    const auto* source=get(ts[ti],"source");if(!source)throw std::runtime_error("image source");
    const auto ii=index(*source,static_cast<unsigned>(is.size()));
    const auto* uri=get(is[ii],"uri");if(!uri||!uri->is<std::string>())throw std::runtime_error("image uri");
    m.images[slot]=uuid(uri->get<std::string>());
    if(const auto* tc=get(*info,"texCoord"))if(index(*tc,2U)!=0U)throw std::runtime_error("texcoord1");
    if(const auto* ex=get(*info,"extensions"))if(const auto* uv=get(*ex,"KHR_texture_transform")){
        if(const auto* a=get(*uv,"offset"))array(*a,m.uv[slot].offset,-4096.f,4096.f);
        if(const auto* a=get(*uv,"scale"))array(*a,m.uv[slot].scale,-4096.f,4096.f);
        if(const auto* a=get(*uv,"rotation"))m.uv[slot].rotation=real(*a,-10000.f,10000.f);
        if(const auto* a=get(*uv,"texCoord"))if(index(*a,2U)!=0U)throw std::runtime_error("texcoord1");
    }
}
std::uint32_t be32(std::span<const std::uint8_t> b,std::size_t& at){
    if(at>b.size()||b.size()-at<4U)throw std::runtime_error("truncated");
    std::uint32_t n=0U;for(unsigned i=0;i<4U;++i)n=(n<<8U)|b[at++];return n;
}
std::string str(std::span<const std::uint8_t> b,std::size_t& at){
    const auto n=be32(b,at);if(n>b.size()-at||n>1024U*1024U)throw std::runtime_error("string");
    std::string s(reinterpret_cast<const char*>(b.data()+at),n);at+=n;return s;
}
// Bounded parser for the LLSD notation subset used by material overrides.
class Notation {
    std::string_view s;std::size_t at=0U,nodes=0U;
    void ws(){while(at<s.size()&&(s[at]==' '||s[at]=='\n'||s[at]=='\r'||s[at]=='\t'))++at;}
    char take(){ws();if(at==s.size())throw std::runtime_error("notation");return s[at++];}
    std::string quoted(){
        const char q=take();if(q!='\''&&q!='"')throw std::runtime_error("quote");
        std::string out;
        while(at<s.size()){
            auto c=s[at++];if(c==q)return out;
            if(c=='\\'){
                if(at==s.size())throw std::runtime_error("escape");c=s[at++];
                if(c=='n')c='\n';else if(c=='r')c='\r';else if(c=='t')c='\t';
                else if(c!='\\'&&c!='\''&&c!='"')throw std::runtime_error("escape");
            }out+=c;
        }throw std::runtime_error("quote");
    }
public:
    explicit Notation(std::string_view in):s(in){}
    J parse(unsigned depth=0U){
        if(depth>16U||++nodes>4096U)throw std::runtime_error("depth");ws();
        if(at==s.size())throw std::runtime_error("empty");const char c=s[at];
        if(c=='{'||c=='['){
            ++at;picojson::object o;picojson::array a;ws();const char end=c=='{'?'}':']';
            if(at<s.size()&&s[at]==end){++at;return c=='{'?J(o):J(a);}
            for(;;){
                if(c=='{'){const auto key=quoted();if(take()!=':')throw std::runtime_error("colon");
                    if(!o.emplace(key,parse(depth+1U)).second)throw std::runtime_error("duplicate");}
                else a.push_back(parse(depth+1U));
                const auto next=take();if(next==end)break;if(next!=',')throw std::runtime_error("comma");
            }return c=='{'?J(o):J(a);
        }
        if(c=='s'){
            ++at;if(at==s.size()||s[at++]!='(')throw std::runtime_error("string length");
            std::size_t length=0U;unsigned digits=0U;
            while(at<s.size()&&s[at]>='0'&&s[at]<='9'){
                if(++digits>7U)throw std::runtime_error("string length");
                length=length*10U+std::size_t(s[at++]-'0');
                if(length>1024U*1024U)throw std::runtime_error("string length");
            }
            if(!digits||at==s.size()||s[at++]!=')'||at==s.size())throw std::runtime_error("string length");
            const char quote=s[at++];if(quote!='\''&&quote!='"')throw std::runtime_error("quote");
            if(length>s.size()-at)throw std::runtime_error("string length");
            const std::string value(s.substr(at,length));at+=length;
            if(at==s.size()||s[at++]!=quote)throw std::runtime_error("quote");return J(value);
        }
        if(c=='\''||c=='"')return J(quoted());
        if(c=='!'){++at;return J();}
        if(c=='u'){++at;if(s.size()-at<36U)throw std::runtime_error("uuid");
            const auto id=std::string(s.substr(at,36U));at+=36U;return J(id);}
        if(s.substr(at,4U)=="true"){at+=4U;return J(true);}
        if(s.substr(at,5U)=="false"){at+=5U;return J(false);}
        if(c=='i'||c=='r')++at;
        const auto start=at;while(at<s.size()&&s[at]!=','&&s[at]!=']'&&s[at]!='}'&&
            s[at]!=' '&&s[at]!='\n'&&s[at]!='\r')++at;
        const auto word=std::string(s.substr(start,at-start));char* end=nullptr;
        const double n=std::strtod(word.c_str(),&end);
        if(word.empty()||end!=word.c_str()+word.size()||!std::isfinite(n))throw std::runtime_error("number");
        return J(n);
    }
    bool done(){ws();return at==s.size();}
};
Override override_value(const J& j){
    if(j.is<picojson::null>())return {};
    if(!j.is<picojson::object>())throw std::runtime_error("override");Override o;
    const auto vec=[&](const char* key,auto& dest,std::uint32_t bit){if(const auto* v=get(j,key)){array(*v,dest,0.f,1.f);o.fields|=bit;}};
    vec("bc",o.value.base,BC);vec("ec",o.value.emissive,EC);
    const auto number=[&](const char* key,float& dest,std::uint32_t bit){if(const auto* v=get(j,key)){dest=real(*v,0.f,1.f);o.fields|=bit;}};
    number("mf",o.value.metallic,MF);number("rf",o.value.roughness,RF);number("ac",o.value.cutoff,AC);
    if(const auto* v=get(j,"am")){o.value.alpha=index(*v,3U);o.fields|=AM;}
    if(const auto* v=get(j,"ds")){if(!v->is<bool>())throw std::runtime_error("bool");o.value.double_sided=v->get<bool>();o.fields|=DS;}
    if(const auto* v=get(j,"tex")){
        if(!v->is<picojson::array>()||v->get<picojson::array>().size()>4U)throw std::runtime_error("images");
        for(unsigned i=0;i<v->get<picojson::array>().size();++i){const auto& id=v->get<picojson::array>()[i];
            if(id.is<picojson::null>())continue;
            if(!id.is<std::string>())throw std::runtime_error("uuid");
            const auto name=uuid(id.get<std::string>(),true);
            if(!name.empty()){o.image_mask|=1U<<i;o.value.images[i]=name=="clear"?"":name;}
        }
    }
    if(const auto* v=get(j,"ti")){
        if(!v->is<picojson::array>()||v->get<picojson::array>().size()>4U)throw std::runtime_error("transforms");
        for(unsigned i=0;i<v->get<picojson::array>().size();++i){
            const auto& uv=v->get<picojson::array>()[i];
            if(const auto* a=get(uv,"o")){array(*a,o.value.uv[i].offset,-4096.f,4096.f);o.uv_mask|=1U<<(i*3U);}
            if(const auto* a=get(uv,"s")){array(*a,o.value.uv[i].scale,-4096.f,4096.f);o.uv_mask|=2U<<(i*3U);}
            if(const auto* a=get(uv,"r")){o.value.uv[i].rotation=real(*a,-10000.f,10000.f);o.uv_mask|=4U<<(i*3U);}
        }
    }return o;
}
}
bool decode_face_ids(std::span<const std::uint8_t> b,std::array<std::string,45>& output) noexcept {
    try{
        std::array<std::string,45> ids{};if(b.empty()){output=ids;return true;}
        std::size_t at=1U;const auto count=b[0];if(count>45U)return false;
        bool seen=false;
        for(unsigned i=0;i<count;++i){
            if(b.size()-at<6U)return false;
            const auto type=unsigned(b[at])|(unsigned(b[at+1U])<<8U);at+=2U;
            const auto n=std::uint32_t(b[at])|(std::uint32_t(b[at+1U])<<8U)|
                (std::uint32_t(b[at+2U])<<16U)|(std::uint32_t(b[at+3U])<<24U);at+=4U;
            if(n>b.size()-at)return false;
            if(type==0x80U){
                if(seen||n<1U||b[at]>14U||n!=1U+unsigned(b[at])*17U)return false;seen=true;
                std::array<bool,45> faces{};
                for(unsigned k=0;k<b[at];++k){const auto p=at+1U+k*17U;const auto face=b[p];
                    if(face>=45U||faces[face])return false;faces[face]=true;
                    ids[face]=wire_uuid(b.subspan(p+1U,16U));}
            }at+=n;
        }if(at!=b.size())return false;output=std::move(ids);return true;
    }catch(...){return false;}
}
bool decode_probe(std::span<const std::uint8_t> b,bool& found,float& ambiance,float& clip,std::uint8_t& flags) noexcept {
    try{
        bool present=false;float a=1.f,c=.1f;std::uint8_t f=0U;
        if(b.empty()){found=false;return true;}
        std::size_t at=1U;if(b[0]>32U)return false;
        for(unsigned i=0;i<b[0];++i){
            if(b.size()-at<6U)return false;
            const unsigned type=b[at]|(unsigned(b[at+1U])<<8U);
            const auto n=std::uint32_t(b[at+2U])|(std::uint32_t(b[at+3U])<<8U)|
                (std::uint32_t(b[at+4U])<<16U)|(std::uint32_t(b[at+5U])<<24U);at+=6U;
            if(n>b.size()-at)return false;
            if(type==0x90U){
                if(present||n!=9U)return false;present=true;
                std::memcpy(&a,b.data()+at,4U);std::memcpy(&c,b.data()+at+4U,4U);f=b[at+8U];
                if(!std::isfinite(a)||!std::isfinite(c)||a<0.f||a>100.f||c<0.f||c>1024.f)return false;
            }at+=n;
        }
        if(at!=b.size())return false;
        found=present;ambiance=a;clip=c;flags=f;return true;
    }catch(...){return false;}
}
bool decode_json(std::string_view json,Material& output) noexcept {
    try{
        if(json.empty()||json.size()>1024U*1024U)return false;
        J root;std::string err;
        const auto end=picojson::parse(root,json.begin(),json.end(),&err);
        if(!err.empty()||!std::all_of(end,json.end(),[](char c){return c==' '||c=='\r'||c=='\n'||c=='\t';}))return false;
        const auto* asset=get(root,"asset"),*materials=get(root,"materials");
        const auto* version=asset?get(*asset,"version"):nullptr;
        if(!version||!version->is<std::string>()||version->get<std::string>()!="2.0"||
           !materials||!materials->is<picojson::array>()||materials->get<picojson::array>().size()!=1U)return false;
        const auto& j=materials->get<picojson::array>()[0];Material m;
        if(!j.is<picojson::object>())return false;
        if(const auto* p=get(j,"pbrMetallicRoughness")){
            if(const auto* v=get(*p,"baseColorFactor"))array(*v,m.base,0.f,1.f);
            if(const auto* v=get(*p,"metallicFactor"))m.metallic=real(*v,0.f,1.f);
            if(const auto* v=get(*p,"roughnessFactor"))m.roughness=real(*v,0.f,1.f);
            texture_info(root,get(*p,"baseColorTexture"),BaseColor,m);
            texture_info(root,get(*p,"metallicRoughnessTexture"),MetallicRoughness,m);
        }
        texture_info(root,get(j,"normalTexture"),Normal,m);
        texture_info(root,get(j,"emissiveTexture"),Emissive,m);
        texture_info(root,get(j,"occlusionTexture"),Occlusion,m);
        if(const auto* v=get(j,"emissiveFactor"))array(*v,m.emissive,0.f,1.f);
        if(const auto* v=get(j,"normalTexture"))if(const auto* n=get(*v,"scale"))m.normal_scale=real(*n,0.f,16.f);
        if(const auto* v=get(j,"occlusionTexture"))if(const auto* n=get(*v,"strength"))m.occlusion_strength=real(*n,0.f,1.f);
        if(const auto* v=get(j,"alphaMode")){
            if(!v->is<std::string>())return false;const auto& mode=v->get<std::string>();
            if(mode=="MASK")m.alpha=1U;else if(mode=="BLEND")m.alpha=2U;else if(mode!="OPAQUE")return false;
        }
        if(const auto* v=get(j,"alphaCutoff"))m.cutoff=real(*v,0.f,1.f);
        if(const auto* v=get(j,"doubleSided")){if(!v->is<bool>())return false;m.double_sided=v->get<bool>();}
        if(const auto* ex=get(j,"extensions")){
            m.unlit=get(*ex,"KHR_materials_unlit")!=nullptr;
            if(const auto* e=get(*ex,"KHR_materials_emissive_strength"))if(const auto* n=get(*e,"emissiveStrength")){
                const float strength=real(*n,0.f,10000.f);for(auto& channel:m.emissive)channel*=strength;}
        }
        output=std::move(m);return true;
    }catch(...){return false;}
}
namespace {
bool space(std::uint8_t c){return c==' '||c=='\r'||c=='\n'||c=='\t';}
std::size_t envelope_start(std::span<const std::uint8_t> b){
    std::size_t at=b.size()>=3U&&b[0]==0xef&&b[1]==0xbb&&b[2]==0xbf?3U:0U;
    while(at<b.size()&&space(b[at]))++at;return at;
}
std::string header_name(std::span<const std::uint8_t> b,std::size_t at,std::size_t& after){
    after=at;
    if(at+2U>b.size()||b[at]!='<'||b[at+1U]!='?')return {};
    const auto begin=at+2U;auto end=begin;
    while(end+1U<b.size()&&end-begin<64U&&!(b[end]=='?'&&b[end+1U]=='>'))++end;
    if(end+1U>=b.size()||end-begin>=64U)return {};
    auto first=begin,last=end;while(first<last&&space(b[first]))++first;
    while(last>first&&space(b[last-1U]))--last;
    std::string name;for(auto i=first;i<last;++i){auto c=b[i];if(c>='A'&&c<='Z')c=c-'A'+'a';name+=char(c);}
    after=end+2U;return name;
}
bool xml_fields(std::span<const std::uint8_t> b,std::map<std::string,std::string>& fields){
    tinyxml2::XMLDocument document;
    if(document.Parse(reinterpret_cast<const char*>(b.data()),b.size())!=tinyxml2::XML_SUCCESS)return false;
    const auto* root=document.RootElement();
    if(!root||std::strcmp(root->Name(),"llsd")||root->NextSiblingElement())return false;
    // TinyXML2 never resolves external resources; also reject DTD/unknown
    // nodes rather than accepting external-entity declarations as data.
    const auto blank=[](const tinyxml2::XMLNode* n){
        if(n->ToComment())return true;
        const auto* text=n->ToText();if(!text)return false;
        for(const auto* c=text->Value();*c;++c)if(!space(std::uint8_t(*c)))return false;
        return true;
    };
    for(const auto* n=document.FirstChild();n;n=n->NextSibling())
        if(n!=root&&!n->ToDeclaration()&&!blank(n))return false;
    const auto* map=root->FirstChildElement();
    if(!map||std::strcmp(map->Name(),"map")||map->NextSiblingElement())return false;
    for(const auto* n=root->FirstChild();n;n=n->NextSibling())if(n!=map&&!blank(n))return false;
    const auto content=[](const tinyxml2::XMLElement* element,std::string& value){
        for(const auto* n=element->FirstChild();n;n=n->NextSibling()){
            if(n->ToComment())continue;const auto* text=n->ToText();if(!text)return false;
            value+=text->Value();if(value.size()>1024U*1024U)return false;
        }return true;
    };
    for(const auto* n=map->FirstChild();n;){
        if(blank(n)){n=n->NextSibling();continue;}
        const auto* key=n->ToElement();if(!key||std::strcmp(key->Name(),"key"))return false;
        std::string name,value;if(!content(key,name)||name.size()>64U)return false;
        n=n->NextSibling();while(n&&blank(n))n=n->NextSibling();
        const auto* string=n?n->ToElement():nullptr;
        if(!string||std::strcmp(string->Name(),"string")||!content(string,value)||fields.size()>=16U||
            !fields.emplace(name,value).second)return false;
        n=n->NextSibling();
    }return true;
}
}
AssetFormat asset_format(std::span<const std::uint8_t> b) noexcept {
    try{
        if(b.size()>=2U&&b[0]==0x1f&&b[1]==0x8b)return AssetFormat::Gzip;
        if(b.size()>=2U&&(b[0]&15U)==8U&&((unsigned(b[0])<<8U)|b[1])%31U==0U)return AssetFormat::Zlib;
        const auto at=envelope_start(b);std::size_t after=at;const auto name=header_name(b,at,after);
        if(name=="llsd/binary")return AssetFormat::Binary;
        if(name=="llsd/xml"||name.starts_with("xml "))return AssetFormat::XML;
        if(name=="llsd/notation")return AssetFormat::Notation;
        if(at==b.size())return AssetFormat::Unknown;
        if(b[at]=='<')return AssetFormat::XML;
        if(b[at]=='{'){
            if(b.size()-at>=2U&&b[at+1U]==0U)return AssetFormat::Binary;
            return AssetFormat::Notation;
        }return AssetFormat::Unknown;
    }catch(...){return AssetFormat::Unknown;}
}
bool decode_asset(std::span<const std::uint8_t> b,Material& output,DecodeError* error) noexcept {
    DecodeError stage=DecodeError::Size;
    const auto fail=[&](DecodeError reason){if(error)*error=reason;return false;};
    try{
        if(b.empty()||b.size()>1024U*1024U+4096U)return fail(stage);
        const auto format=asset_format(b);
        if(format==AssetFormat::Gzip||format==AssetFormat::Zlib)return fail(DecodeError::Compressed);
        stage=DecodeError::Header;auto at=envelope_start(b);std::size_t after=at;
        const auto header=header_name(b,at,after);
        if(at+2U<=b.size()&&b[at]=='<'&&b[at+1U]=='?'){
            if(header=="llsd/binary"||header=="llsd/xml"||header=="llsd/notation"){
                at=after;while(at<b.size()&&space(b[at]))++at;
            }else if(!header.starts_with("xml "))return fail(stage);
        }
        std::map<std::string,std::string> fields;
        if(format==AssetFormat::XML){
            stage=DecodeError::XML;if(!xml_fields(b.subspan(at),fields))return fail(stage);
        }else if(format==AssetFormat::Notation){
            stage=DecodeError::Notation;
            const std::string_view text(reinterpret_cast<const char*>(b.data()+at),b.size()-at);
            Notation reader(text);const auto map=reader.parse();
            if(!reader.done()||!map.is<picojson::object>()||map.get<picojson::object>().size()>16U)return fail(stage);
            for(const auto& [key,value]:map.get<picojson::object>()){
                if(!value.is<std::string>()||key.size()>64U)return fail(stage);fields.emplace(key,value.get<std::string>());
            }
        }else if(format==AssetFormat::Binary){
            stage=DecodeError::Map;if(at==b.size()||b[at++]!='{')return fail(stage);
            const auto n=be32(b,at);if(n>16U)return fail(stage);
            for(unsigned i=0;i<n;++i){stage=DecodeError::Key;
                if(at==b.size()||b[at++]!='k')return fail(stage);const auto key=str(b,at);
                stage=DecodeError::Value;if(at==b.size()||b[at++]!='s')return fail(stage);const auto value=str(b,at);
                if(!fields.emplace(key,value).second)return fail(stage);
            }
            stage=DecodeError::Trailing;
            if(at==b.size()||b[at++]!='}')return fail(stage);
            // Some asset gateways append an ASCII-NUL terminator/padding
            // after the *complete* LLSD binary map. Accept at most four
            // NUL bytes only once the root map and all lengths validated.
            // Reject unrelated trailing payload, another LLSD element,
            // and unterminated maps; do not silently truncate input.
            std::size_t padding=0U;
            while(at<b.size()&&(space(b[at])||b[at]==0U)){
                if(b[at]==0U&&++padding>4U)return fail(stage);
                ++at;
            }
            if(at!=b.size())return fail(stage);
        }else return fail(DecodeError::Envelope);
        if(fields["version"]!="1.0"&&fields["version"]!="1.1")return fail(DecodeError::Version);
        if(fields["type"]!="GLTF 2.0")return fail(DecodeError::Type);
        stage=DecodeError::GLTF;if(!decode_json(fields["data"],output))return fail(stage);
        if(error)*error=DecodeError::None;return true;
    }catch(...){return fail(stage);}
}
bool decode_override(std::span<const std::uint8_t> b,std::uint32_t& object,std::map<unsigned,Override>& out) noexcept {
    try{
        if(b.size()<6U||b.size()>8192U||b[0]!=31U||b[1]!=0x75U||b[2]!=0x41U)return false;
        const auto length=unsigned(b[3])|(unsigned(b[4])<<8U);if(length!=b.size()-5U)return false;
        std::string_view payload(reinterpret_cast<const char*>(b.data()+5U),length);
        if(!payload.empty()&&payload.back()==char(0))payload.remove_suffix(1U);
        constexpr std::string_view prefix="<?llsd/notation?>";
        if(payload.starts_with(prefix))payload.remove_prefix(prefix.size());
        Notation reader(payload);const auto root=reader.parse();if(!reader.done())return false;
        const auto* id=get(root,"id"),*te=get(root,"te"),*od=get(root,"od");
        if(!id||!te||!od||!te->is<picojson::array>()||!od->is<picojson::array>())return false;
        const double num=id->is<double>()?id->get<double>():0.;
        if(num==0.||num< -2147483648.||num>4294967295.||std::floor(num)!=num)return false;
        const auto& faces=te->get<picojson::array>();const auto& values=od->get<picojson::array>();
        if(faces.size()>45U||faces.size()!=values.size())return false;
        std::map<unsigned,Override> decoded;
        for(unsigned i=0;i<faces.size();++i)if(!decoded.emplace(index(faces[i],45U),override_value(values[i])).second)return false;
        object=static_cast<std::uint32_t>(num<0.?num+4294967296.:num);out=std::move(decoded);return true;
    }catch(...){return false;}
}
bool effective(std::string_view id,std::uint32_t object,unsigned face,const Materials& cache,const Overrides& overrides,Material& out){
    if(id.empty())return false;
    const auto it=cache.find(std::string(id));if(it==cache.end())return false;out=it->second;
    const auto obj=overrides.find(object);if(obj==overrides.end())return true;
    const auto entry=obj->second.find(face);if(entry==obj->second.end())return true;
    const auto& o=entry->second;const auto& v=o.value;
    if(o.fields&BC)out.base=v.base;if(o.fields&EC)out.emissive=v.emissive;
    if(o.fields&MF)out.metallic=v.metallic;if(o.fields&RF)out.roughness=v.roughness;
    if(o.fields&AM)out.alpha=v.alpha;if(o.fields&AC)out.cutoff=v.cutoff;if(o.fields&DS)out.double_sided=v.double_sided;
    for(unsigned i=0;i<4U;++i){
        if(o.image_mask&(1U<<i))out.images[i]=v.images[i];
        if(o.uv_mask&(1U<<(3U*i)))out.uv[i].offset=v.uv[i].offset;
        if(o.uv_mask&(2U<<(3U*i)))out.uv[i].scale=v.uv[i].scale;
        if(o.uv_mask&(4U<<(3U*i)))out.uv[i].rotation=v.uv[i].rotation;
    }
    return true;
}
}
