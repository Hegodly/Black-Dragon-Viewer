#include "kami/protocol/pbr_material.hpp"
#include "picojson.h"
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
    return i==o.end()?nullptr:&i->second;
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
bool decode_face_ids(std::span<const std::uint8_t> b,std::array<std::string,32>& output) noexcept {
    try{
        std::array<std::string,32> ids{};if(b.empty()){output=ids;return true;}
        std::size_t at=1U;const auto count=b[0];if(count>32U)return false;
        bool seen=false;
        for(unsigned i=0;i<count;++i){
            if(b.size()-at<6U)return false;
            const auto type=unsigned(b[at])|(unsigned(b[at+1U])<<8U);at+=2U;
            const auto n=std::uint32_t(b[at])|(std::uint32_t(b[at+1U])<<8U)|
                (std::uint32_t(b[at+2U])<<16U)|(std::uint32_t(b[at+3U])<<24U);at+=4U;
            if(n>b.size()-at)return false;
            if(type==0x80U){
                if(seen||n<1U||b[at]>14U||n!=1U+unsigned(b[at])*17U)return false;seen=true;
                std::array<bool,32> faces{};
                for(unsigned k=0;k<b[at];++k){const auto p=at+1U+k*17U;const auto face=b[p];
                    if(face>=32U||faces[face])return false;faces[face]=true;
                    ids[face]=wire_uuid(b.subspan(p+1U,16U));}
            }at+=n;
        }if(at!=b.size())return false;output=std::move(ids);return true;
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
bool decode_asset(std::span<const std::uint8_t> b,Material& output) noexcept {
    try{
        if(b.empty()||b.size()>1024U*1024U+4096U)return false;
        std::size_t at=0U;constexpr std::string_view prefix="<?llsd/binary?>";
        if(b.size()>=prefix.size()&&std::memcmp(b.data(),prefix.data(),prefix.size())==0){at=prefix.size();
            if(at<b.size()&&b[at]=='\r')++at;if(at<b.size()&&b[at]=='\n')++at;}
        if(at==b.size()||b[at++]!='{')return false;const auto n=be32(b,at);if(n>16U)return false;
        std::map<std::string,std::string> fields;
        for(unsigned i=0;i<n;++i){if(at==b.size()||b[at++]!='k')return false;const auto key=str(b,at);
            if(at==b.size()||b[at++]!='s')return false;const auto value=str(b,at);
            if(!fields.emplace(key,value).second)return false;}
        if(at==b.size()||b[at++]!='}'||at!=b.size())return false;
        if((fields["version"]!="1.0"&&fields["version"]!="1.1")||fields["type"]!="GLTF 2.0")return false;
        return decode_json(fields["data"],output);
    }catch(...){return false;}
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
        if(num<1.||num>4294967295.||std::floor(num)!=num)return false;
        const auto& faces=te->get<picojson::array>();const auto& values=od->get<picojson::array>();
        if(faces.size()>32U||faces.size()!=values.size())return false;
        std::map<unsigned,Override> decoded;
        for(unsigned i=0;i<faces.size();++i)if(!decoded.emplace(index(faces[i],32U),override_value(values[i])).second)return false;
        object=static_cast<std::uint32_t>(num);out=std::move(decoded);return true;
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
