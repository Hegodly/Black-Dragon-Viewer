#include "kami/protocol/texture_decode.hpp"
#include "kami/protocol/caps.hpp"
#include <array>
#include <cmath>
#include <cstdint>
#include <string>
#include <vector>
int main(){
    // Original 2x2 solid red lossless J2K fixture generated with
    // Pillow/OpenJPEG 2.5.4. Exercises the real OpenJPEG decoder, not
    // an image-signature-only stand-in. No simulator account required.
    const std::string hex=
      "ff4fff51002f000000000002000000020000000000000000000000020000000200000000000000000003070101070101070101ff52000c00000001000104040001ff5c00074040484850ff640025000143726561746564206279204f70656e4a5045472076657273696f6e20322e352e34ff90000a00000000001d0001ff93cfb40400df800807df800807808080ffd9";
    std::vector<std::uint8_t> j2c;
    j2c.reserve(hex.size()/2U);
    const auto nibble=[](char c)->int {
        if(c>='0'&&c<='9')return c-'0';
        if(c>='a'&&c<='f')return c-'a'+10;
        return -1;
    };
    for(std::size_t i=0;i+1U<hex.size();i+=2U){
        const int a=nibble(hex[i]),b=nibble(hex[i+1U]);
        if(a<0||b<0)return 1;
        j2c.push_back(static_cast<std::uint8_t>((a<<4)|b));
    }
    if(!kami::protocol::caps::jpeg2000_codestream_header(j2c))return 2;
    kami::protocol::texture::DecodedTexture decoded{};
    if(!kami::protocol::texture::decode_jpeg2000(j2c,decoded)||
       decoded.width!=2U||decoded.height!=2U||
       decoded.rgba.size()!=16U)return 3;
    for(std::size_t i=0U;i<4U;++i){
        const auto* p=decoded.rgba.data()+i*4U;
        if(p[0]<252U||p[1]>3U||p[2]>3U||p[3]!=255U)
            return 4;
    }
    auto invalid=j2c;
    invalid[0]=0x10U;
    if(kami::protocol::caps::jpeg2000_codestream_header(invalid)||
       kami::protocol::texture::decode_jpeg2000(invalid,decoded))
        return 5;
    invalid=j2c;
    invalid.resize(18U);
    if(kami::protocol::texture::decode_jpeg2000(invalid,decoded))
        return 6;
    // Output remains the prior verified texture on any decode failure.
    if(decoded.rgba.size()!=16U||decoded.width!=2U)return 7;
    std::array<std::uint8_t,12U> jp2{
        0,0,0,12,0x6a,0x50,0x20,0x20,0x0d,0x0a,0x87,0x0a};
    if(!kami::protocol::caps::jpeg2000_codestream_header(jp2))return 8;
    jp2[10]=0U;
    if(kami::protocol::caps::jpeg2000_codestream_header(jp2))return 9;
    // R49 JPEG2000 preview cache must not retain a 16 MiB 2048x2048
    // image just to make a 256px DX12 atlas slice. Preserve aspect ratio,
    // RGBA pixel ordering, and transactional failure behavior.
    kami::protocol::texture::DecodedTexture full{};
    full.width=1024U;full.height=512U;
    full.rgba.resize(1024U*512U*4U);
    for(std::uint32_t y=0;y<full.height;++y)
        for(std::uint32_t x=0;x<full.width;++x){
            const auto at=4U*(std::size_t(y)*full.width+x);
            full.rgba[at]=x<512U?31U:190U;
            full.rgba[at+1U]=static_cast<std::uint8_t>(y&255U);
            full.rgba[at+2U]=69U;
            full.rgba[at+3U]=255U;
        }
    if(!kami::protocol::texture::downscale_preview(full,512U)||
       full.width!=512U||full.height!=256U||
       full.rgba.size()!=512U*256U*4U||
       full.rgba[0]!=31U||full.rgba[4U*(511U)]!=190U)
        return 10;
    const auto first=full.rgba;
    if(!kami::protocol::texture::downscale_preview(full,512U)||
       full.rgba!=first)return 11;
    if(kami::protocol::texture::downscale_preview(full,0U)||
       full.rgba!=first)return 12;
    full.rgba.pop_back();
    if(kami::protocol::texture::downscale_preview(full,512U))
        return 13;
    return 0;
}
