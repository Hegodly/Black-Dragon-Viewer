#include "kami/protocol/mesh_lod.hpp"
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <map>
#include <string>
#include <utility>

namespace kami::protocol::mesh {
namespace {
struct Value {
    enum class Kind {Null,Boolean,Number,String,Binary,Array,Map} kind=Kind::Null;
    bool boolean=false;
    double number=0.;
    std::string text;
    std::vector<std::uint8_t> bytes;
    std::vector<Value> list;
    std::map<std::string,Value> entries;
    const Value* get(const char* k) const noexcept {
        if(kind!=Kind::Map)return nullptr;
        const auto it=entries.find(k);
        return it==entries.end()?nullptr:&it->second;
    }
};
class Reader {
public:
    explicit Reader(std::span<const std::uint8_t> b) noexcept:b_(b){}
    [[nodiscard]] std::size_t offset() const noexcept{return at_;}
    [[nodiscard]] bool end() const noexcept{return at_==b_.size();}
    [[nodiscard]] bool advance(std::size_t n) noexcept {
        if(at_>b_.size()||n>b_.size()-at_)return false;
        at_+=n;return true;
    }
    [[nodiscard]] bool u32(std::uint32_t& value) noexcept {
        if(at_>b_.size()||b_.size()-at_<4U)return false;
        value=(std::uint32_t(b_[at_])<<24U)|
              (std::uint32_t(b_[at_+1U])<<16U)|
              (std::uint32_t(b_[at_+2U])<<8U)|b_[at_+3U];
        at_+=4U;return true;
    }
    [[nodiscard]] bool parse(Value& v,unsigned depth=0U) {
        if(depth>16U||++nodes_>24000U||!advance(1U))return false;
        const auto tag=b_[at_-1U];
        if(tag=='!')return true;
        if(tag=='1'||tag=='0'){
            v.kind=Value::Kind::Boolean;v.boolean=tag=='1';return true;
        }
        if(tag=='i'){
            std::uint32_t word=0;
            if(!u32(word))return false;
            v.kind=Value::Kind::Number;
            v.number=static_cast<std::int32_t>(word);
            return true;
        }
        if(tag=='r'||tag=='d'){
            if(b_.size()-at_<8U)return false;
            std::uint64_t bits=0;
            for(unsigned i=0;i<8U;++i)bits=(bits<<8U)|b_[at_+i];
            at_+=8U;
            if(tag=='r'){
                std::memcpy(&v.number,&bits,sizeof(bits));
                if(!std::isfinite(v.number))return false;
                v.kind=Value::Kind::Number;
            }
            return true;
        }
        if(tag=='u')return advance(16U);
        if(tag=='s'||tag=='l'||tag=='b'){
            std::uint32_t length=0;
            if(!u32(length)||length>8U*1024U*1024U||
               b_.size()-at_<length)return false;
            if(tag=='b'){
                v.kind=Value::Kind::Binary;
                v.bytes.assign(b_.begin()+at_,b_.begin()+at_+length);
            }else{
                v.kind=Value::Kind::String;
                v.text.assign(reinterpret_cast<const char*>(b_.data()+at_),
                              length);
            }
            at_+=length;return true;
        }
        if(tag=='['||tag=='{'){
            std::uint32_t count=0;
            if(!u32(count)||count>8192U)return false;
            v.kind=tag=='['?Value::Kind::Array:Value::Kind::Map;
            if(tag=='['){
                v.list.reserve(count);
                for(std::uint32_t i=0;i<count;++i){
                    Value node;
                    if(!parse(node,depth+1U))return false;
                    v.list.push_back(std::move(node));
                }
            }else{
                for(std::uint32_t i=0;i<count;++i){
                    if(b_.size()-at_<1U||b_[at_++]!='k')return false;
                    std::uint32_t length=0;
                    if(!u32(length)||length==0U||length>256U||
                       b_.size()-at_<length)return false;
                    std::string key(reinterpret_cast<const char*>(b_.data()+at_),
                                    length);
                    at_+=length;
                    Value node;
                    if(!parse(node,depth+1U)||
                       !v.entries.emplace(std::move(key),std::move(node)).second)
                        return false;
                }
            }
            return at_<b_.size()&&b_[at_++]==(tag=='['?']':'}');
        }
        return false;
    }
private:
    std::span<const std::uint8_t> b_;
    std::size_t at_=0U,nodes_=0U;
};
bool read_llsd(std::span<const std::uint8_t> bytes,Value& out,
               std::size_t& consumed) {
    if(bytes.empty())return false;
    // Linden Lab's LLSDSerialize::toBinary (used for SL mesh headers
    // and zipped LOD blocks) emits a RAW binary map/array, not the
    // optional "<?llsd/binary?>" transport header. Requiring that
    // prefix rejected valid GetMesh/ViewerAsset HTTP 200 responses.
    // Continue to accept previously supported prefixed fixtures.
    constexpr char binary[]="<?llsd/binary?>";
    constexpr char deprecated[]="<? LLSD/Binary ?>";
    std::size_t at=0U;
    if(bytes.size()>=sizeof(binary)-1U &&
       std::memcmp(bytes.data(),binary,sizeof(binary)-1U)==0)
        at=sizeof(binary)-1U;
    else if(bytes.size()>=sizeof(deprecated)-1U &&
            std::memcmp(bytes.data(),deprecated,sizeof(deprecated)-1U)==0)
        at=sizeof(deprecated)-1U;
    if(at!=0U){
        if(at<bytes.size()&&bytes[at]=='\r')++at;
        if(at<bytes.size()&&bytes[at]=='\n')++at;
    }
    // Mesh root is a map; a decoded LOD is an array. No arbitrary
    // primitive/value prefixes in this untrusted HTTP mesh path.
    if(at>=bytes.size()||
       (bytes[at]!='{'&&bytes[at]!='['))return false;
    Reader reader(bytes.subspan(at));
    if(!reader.parse(out))return false;
    consumed=at+reader.offset();
    return true;
}
bool number(const Value* value,double& n) noexcept {
    if(!value||value->kind!=Value::Kind::Number||
       !std::isfinite(value->number))return false;
    n=value->number;return true;
}
bool vec3(const Value* value,std::array<float,3>& out) noexcept {
    if(!value||value->kind!=Value::Kind::Array||
       value->list.size()!=3U)return false;
    for(unsigned i=0;i<3U;++i){
        double n=0.;
        if(!number(&value->list[i],n)||n<-.501||n>.501)return false;
        out[i]=static_cast<float>(n);
    }
    return true;
}
bool position_domain(const Value& node,std::array<float,3>& lo,
                     std::array<float,3>& hi) noexcept {
    lo={-.5f,-.5f,-.5f};hi={.5f,.5f,.5f};
    const auto* domain=node.get("PositionDomain");
    if(!domain)return true;
    if(!vec3(domain->get("Min"),lo)||!vec3(domain->get("Max"),hi))
        return false;
    for(unsigned i=0;i<3U;++i)if(hi[i]<lo[i])return false;
    return true;
}
bool uv_domain(const Value& face,std::array<float,2>& lo,
               std::array<float,2>& hi) noexcept {
    lo={0.f,0.f};hi={1.f,1.f};
    const auto* domain=face.get("TexCoord0Domain");
    if(!domain)return true;
    if(domain->kind!=Value::Kind::Map)return false;
    const auto* lower=domain->get("Min");
    const auto* upper=domain->get("Max");
    if(!lower||!upper||lower->kind!=Value::Kind::Array||
       upper->kind!=Value::Kind::Array||lower->list.size()!=2U||
       upper->list.size()!=2U)return false;
    for(unsigned i=0;i<2U;++i){
        double a=0.,b=0.;
        if(!number(&lower->list[i],a)||!number(&upper->list[i],b)||
           a<-4096.||b>4096.||a>b)return false;
        lo[i]=static_cast<float>(a);
        hi[i]=static_cast<float>(b);
    }
    return true;
}
std::uint16_t le16(const std::vector<std::uint8_t>& b,std::size_t at) noexcept {
    return std::uint16_t(b[at])|(std::uint16_t(b[at+1U])<<8U);
}
bool inflate(std::span<const std::uint8_t> compressed,
             std::vector<std::uint8_t>& result){
    if(compressed.size()>4U*1024U*1024U||
       compressed.size()<6U)return false;
    constexpr int max_output=8*1024*1024;
    result.resize(max_output);
    int bytes=-1;
    if(compressed[0]==0x1fU&&compressed[1]==0x8bU){
        if(compressed.size()<18U||compressed[2]!=8U)return false;
        const auto flags=compressed[3];
        if(flags&0xe0U)return false;
        std::size_t at=10U;
        if(flags&0x04U){
            if(compressed.size()-at<2U)return false;
            const auto length=std::size_t(compressed[at])|
                               (std::size_t(compressed[at+1U])<<8U);
            at+=2U;
            if(length>compressed.size()-at)return false;
            at+=length;
        }
        const auto nul=[&]() -> bool {
            for(unsigned i=0;i<1024U&&at<compressed.size();++i)
                if(compressed[at++]==0U)return true;
            return false;
        };
        if((flags&0x08U)&&!nul())return false;
        if((flags&0x10U)&&!nul())return false;
        if(flags&0x02U){if(compressed.size()-at<2U)return false;at+=2U;}
        if(at+8U>compressed.size())return false;
        bytes=stbi_zlib_decode_noheader_buffer(
             reinterpret_cast<char*>(result.data()),max_output,
             reinterpret_cast<const char*>(compressed.data()+at),
             static_cast<int>(compressed.size()-at-8U));
    }else{
        bytes=stbi_zlib_decode_buffer(
             reinterpret_cast<char*>(result.data()),max_output,
             reinterpret_cast<const char*>(compressed.data()),
             static_cast<int>(compressed.size()));
    }
    if(bytes<=0)return false;
    result.resize(static_cast<std::size_t>(bytes));
    return true;
}
bool decode_faces(const Value& root,LOD& out) {
    if(root.kind!=Value::Kind::Array||root.list.size()>32U)return false;
    LOD result{};
    for(std::size_t face_number=0U;face_number<root.list.size();++face_number){
        const auto& face=root.list[face_number];
        if(face.kind!=Value::Kind::Map)return false;
        if(const auto* empty=face.get("NoGeometry")){
            if(empty->kind!=Value::Kind::Boolean||!empty->boolean)return false;
            continue;
        }
        // Skin weights do not invalidate the mesh geometry itself. Decode
        // its faces for the static world path; avatar skinning remains a
        // separate DX12 task. Skipping every weighted face silently dropped
        // legitimate attachment and rigged-mesh geometry.
        // Do not discard the face solely because Weights is present.
        const auto* position=face.get("Position");
        const auto* triangles=face.get("TriangleList");
        if(!position||!triangles||
           position->kind!=Value::Kind::Binary||
           triangles->kind!=Value::Kind::Binary)return false;
        const auto& points=position->bytes;
        const auto& indices=triangles->bytes;
        const auto* texcoords=face.get("TexCoord0");
        if(texcoords&&texcoords->kind!=Value::Kind::Binary)return false;
        if(points.size()<18U||points.size()%6U!=0U||
           points.size()/6U>65536U||
           indices.size()<6U||indices.size()%6U!=0U||
           indices.size()/6U>60000U)return false;
        std::array<float,3> lo{},hi{};
        if(!position_domain(face,lo,hi))return false;
        std::array<float,2> uv_lo{},uv_hi{};
        if(texcoords&&
           (texcoords->bytes.size()!=points.size()/6U*4U||
            !uv_domain(face,uv_lo,uv_hi)))return false;
        const std::size_t total=indices.size()/6U;
        if(total+result.triangles.size()>60000U)return false;
        std::vector<std::array<float,3>> positions;
        positions.reserve(points.size()/6U);
        for(std::size_t i=0;i<points.size();i+=6U){
            std::array<float,3> point{};
            for(unsigned axis=0;axis<3U;++axis){
                const auto value=le16(points,i+axis*2U);
                point[axis]=lo[axis]+(hi[axis]-lo[axis])*
                                     (float(value)/65535.f);
            }
            positions.push_back(point);
        }
        std::vector<std::array<float,2>> uv_points;
        if(texcoords){
            uv_points.reserve(points.size()/6U);
            for(std::size_t i=0;i<texcoords->bytes.size();i+=4U){
                const auto u=le16(texcoords->bytes,i);
                const auto v=le16(texcoords->bytes,i+2U);
                const float uu=uv_lo[0]+(uv_hi[0]-uv_lo[0])*
                    (float(u)/65535.f);
                const float vv=uv_lo[1]+(uv_hi[1]-uv_lo[1])*
                    (float(v)/65535.f);
                uv_points.push_back({uu,1.f-vv});
            }
        }
        for(std::size_t i=0;i<indices.size();i+=6U){
            const auto a=le16(indices,i),b=le16(indices,i+2U),
                       c=le16(indices,i+4U);
            if(a>=positions.size()||b>=positions.size()||
               c>=positions.size())return false;
            // Collapsed triangles occur in reduced LODs. They contribute no
            // surface; retain the other valid triangles in this mesh.
            if(a==b||a==c||b==c)continue;
            Triangle triangle{};
            triangle.face_index=static_cast<std::uint8_t>(face_number);
            for(unsigned idx=0;idx<3U;++idx){
                const auto& point=positions[idx==0U?a:idx==1U?b:c];
                for(unsigned axis=0;axis<3U;++axis)
                    triangle.pos[3U*idx+axis]=point[axis];
                if(texcoords){
                    const auto& uv=uv_points[idx==0U?a:idx==1U?b:c];
                    triangle.uv[2U*idx]=uv[0];
                    triangle.uv[2U*idx+1U]=uv[1];
                    triangle.has_uv=true;
                }
            }
            result.triangles.push_back(triangle);
        }
    }
    if(result.triangles.empty())return false;
    out=std::move(result);
    return true;
}
} // namespace
bool decode_mesh_lod(std::span<const std::uint8_t> asset,LOD& out) noexcept {
    if(asset.empty()||asset.size()>8U*1024U*1024U)return false;
    try{
        Value header;
        std::size_t header_bytes=0U;
        if(!read_llsd(asset,header,header_bytes)||
           header.kind!=Value::Kind::Map)return false;
        // Real SL mesh assets may omit any of their four optional LOD
        // blocks. A valid low/lowest static LOD is still genuine geometry:
        // reject nonzero malformed offsets/lengths, then try the next
        // bounded and fully decoded asset block if a populated higher
        // LOD has invalid or unsupported geometry.
        constexpr std::array<const char*,4U> levels{
            "high_lod","medium_lod","low_lod","lowest_lod"
        };
        // Validate metadata for every declared LOD before selecting a preferred
        // block. Otherwise a valid high LOD could mask malformed medium/low
        // offsets and bypass the existing corruption regression test.
        for(const char* name:levels){
            const Value* block=header.get(name);
            if(!block)continue;
            if(block->kind!=Value::Kind::Map)return false;
            double offset=0.,length=0.;
            if(!number(block->get("offset"),offset)||
               !number(block->get("size"),length))return false;
            if(length==0.)continue; // SL missing-LOD sentinel
            if(offset<0.||length<6.||offset>double(asset.size())||
               length>4.*1024.*1024.)return false;
            const auto off=static_cast<std::size_t>(offset);
            const auto len=static_cast<std::size_t>(length);
            if(double(off)!=offset||double(len)!=length||
               header_bytes>asset.size()||off>asset.size()-header_bytes||
               len>asset.size()-header_bytes-off)return false;
        }
        for(const char* name:levels){
            const Value* block=header.get(name);
            if(!block)continue;
            if(block->kind!=Value::Kind::Map)return false;
            double offset=0.,length=0.;
            if(!number(block->get("offset"),offset)||
               !number(block->get("size"),length))return false;
            // SL uses size=0 as a missing-LOD sentinel, often with
            // offset=-1. Do not treat this absent block as corruption.
            if(length==0.)continue;
            if(offset<0.||length<6.||
               offset>double(asset.size())||length>4.*1024.*1024.)
                return false;
            const auto off=static_cast<std::size_t>(offset);
            const auto len=static_cast<std::size_t>(length);
            if(double(off)!=offset||double(len)!=length||
               header_bytes>asset.size()||off>asset.size()-header_bytes||
               len>asset.size()-header_bytes-off)return false;
            std::vector<std::uint8_t> uncompressed;
            if(!inflate(asset.subspan(header_bytes+off,len),uncompressed))
                continue;
            Value lod;
            std::size_t parsed=0U;
            if(!read_llsd(uncompressed,lod,parsed)||
               parsed!=uncompressed.size())continue;
            LOD result{};
            if(!decode_faces(lod,result))continue;
            result.decoded_bytes=uncompressed.size();
            out=std::move(result);
            return true;
        }
        return false;
    }catch(...){return false;}
}
} // namespace kami::protocol::mesh
