#include "kami/protocol/terrain.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <numbers>
#include <span>
#include <vector>

namespace kami::protocol::terrain {
namespace {
class Bits final {
public:
    explicit Bits(std::span<const std::uint8_t> bytes):bytes_(bytes){}
    [[nodiscard]] bool read(unsigned count,std::uint32_t& v) noexcept {
        v=0;
        if(count>32U||count>bytes_.size()*8U-position_)return false;
        for(unsigned n=0;n<count;++n){
            const auto b=bytes_[position_>>3U];
            v=(v<<1U)|((b>>(7U-(position_&7U)))&1U);
            ++position_;
        }
        return true;
    }
    [[nodiscard]] bool little(unsigned byte_count,std::uint32_t& out) noexcept {
        out=0;
        for(unsigned i=0;i<byte_count;++i){
            std::uint32_t part=0;
            if(!read(8,part))return false;
            out|=part<<(8U*i);
        }
        return true;
    }
    // Linden's ten-bit patch index is serialized as a low byte,
    // then the two low bits of the next byte (bit-packed).
    [[nodiscard]] bool patch_id(std::uint32_t& out) noexcept {
        std::uint32_t low=0,high=0;
        if(!read(8,low)||!read(2,high))return false;
        out=low|(high<<8U);
        return true;
    }
private:
    std::span<const std::uint8_t> bytes_;
    std::size_t position_=0;
};
[[nodiscard]] bool inverse_dct(const std::array<std::int32_t,1024>& scan,
                                unsigned size,std::uint8_t quant,
                                float offset,std::uint16_t range,
                                Patch& patch) noexcept {
    if((size!=16U&&size!=32U)||!std::isfinite(offset)||
       offset < -4096.f||offset>8192.f)return false;
    constexpr double kAlpha=0.7071067811865475244;
    std::array<double,1024> freq{},tmp{};
    // Build the frequency positions from the same zigzag traversal
    // defined by the Second Life patch wire protocol.
    int x=0,y=0;bool diagonal=false,forward=true;
    for(unsigned n=0;n<size*size;++n){
        if(x<0||y<0||x>=static_cast<int>(size)||
           y>=static_cast<int>(size))return false;
        freq[static_cast<unsigned>(y)*size+static_cast<unsigned>(x)]=
            static_cast<double>(scan[n])*
            (1.0+2.0*(static_cast<double>(x)+static_cast<double>(y)));
        if(!diagonal){
            if(forward){
                if(x<static_cast<int>(size)-1)++x;else ++y;
                forward=false;diagonal=true;
            }else{
                if(y<static_cast<int>(size)-1)++y;else ++x;
                forward=true;diagonal=true;
            }
        }else if(forward){
            ++x;--y;
            if(x==static_cast<int>(size)-1||y==0)diagonal=false;
        }else{
            --x;++y;
            if(x==0||y==static_cast<int>(size)-1)diagonal=false;
        }
    }
    std::array<double,1024> spatial{};
    std::array<std::array<double,32>,32> cosine{};
    for(unsigned u=0;u<size;++u)
        for(unsigned p=0;p<size;++p)
            cosine[u][p]=std::cos((2.0*p+1.0)*u*
                std::numbers::pi/(2.0*size));
    for(unsigned row=0;row<size;++row){
        for(unsigned col=0;col<size;++col){
            double v=kAlpha*freq[col];
            for(unsigned u=1;u<size;++u)
                v+=freq[u*size+col]*cosine[u][row];
            tmp[row*size+col]=v;
        }
    }
    const unsigned prequant=(quant>>4U)+2U;
    const double scale=static_cast<double>(range)/
        static_cast<double>(1U<<prequant);
    const double center=static_cast<double>(offset)+
        scale*static_cast<double>(1U<<(prequant-1U));
    for(unsigned row=0;row<size;++row){
        for(unsigned col=0;col<size;++col){
            double v=kAlpha*tmp[row*size];
            for(unsigned u=1;u<size;++u)
                v+=tmp[row*size+u]*cosine[u][col];
            v=(v*(2.0/size))*scale+center;
            if(!std::isfinite(v)||v< -4096.0||v>8192.0)return false;
            spatial[row*size+col]=v;
        }
    }
    for(unsigned i=0;i<size*size;++i)
        patch.elevation[i]=static_cast<float>(spatial[i]);
    return true;
}
} // namespace

bool decode_land_layer(const lludp::Packet& packet,
                       std::vector<Patch>& out) {
    out.clear();
    const auto& body=packet.body;
    if(body.size()<9U||body[0]!=11U||body[1]!=76U)return false;
    const unsigned bytes=static_cast<unsigned>(body[2])|
        (static_cast<unsigned>(body[3])<<8U);
    if(bytes<5U||bytes>4096U||body.size()!=4U+bytes)return false;
    Bits reader(std::span<const std::uint8_t>(body.data()+4U,bytes));
    std::uint32_t stride=0,size=0,type=0;
    if(!reader.little(2,stride)||!reader.read(8,size)||
       !reader.read(8,type)||type!=76U||
       (size!=16U&&size!=32U)||stride<size||stride>4096U)return false;
    std::vector<Patch> decoded;
    decoded.reserve(16);
    for(unsigned count=0;count<64U;++count) {
        std::uint32_t quant=0;
        if(!reader.read(8,quant))return false;
        if(quant==97U){
            if(decoded.empty())return false;
            out=std::move(decoded);
            return true;
        }
        std::uint32_t offset_bits=0,range=0,id=0;
        if(!reader.little(4,offset_bits)||!reader.little(2,range)||
           !reader.patch_id(id))return false;
        float offset=0.f;
        static_assert(sizeof(offset)==sizeof(offset_bits));
        std::memcpy(&offset,&offset_bits,sizeof(offset));
        Patch patch{};
        patch.size=static_cast<std::uint8_t>(size);
        patch.x=static_cast<std::uint8_t>(id>>5U);
        patch.y=static_cast<std::uint8_t>(id&31U);
        // Typical SL regions contain 16x16 normal patches, at most 32
        // patches per axis. Do not synthesize out-of-range terrain.
        if(patch.x>=32U||patch.y>=32U)return false;
        const unsigned wordbits=(quant&15U)+2U;
        std::array<std::int32_t,1024> scan{};
        for(unsigned n=0;n<size*size;++n){
            std::uint32_t nonzero=0;
            if(!reader.read(1,nonzero))return false;
            if(nonzero==0U)continue;
            std::uint32_t value=0;
            if(!reader.read(1,value))return false;
            if(value==0U)break; // rest of the patch is zero (EOB)
            std::uint32_t sign=0,magnitude=0;
            if(!reader.read(1,sign)||
               !reader.read(wordbits,magnitude))return false;
            scan[n]=sign? -static_cast<std::int32_t>(magnitude):
                          static_cast<std::int32_t>(magnitude);
        }
        if(!inverse_dct(scan,size,static_cast<std::uint8_t>(quant),
                        offset,static_cast<std::uint16_t>(range),patch))
            return false;
        decoded.push_back(std::move(patch));
    }
    return false; // unbounded packet with no valid end marker
}
} // namespace kami::protocol::terrain
