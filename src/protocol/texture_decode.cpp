#include "kami/protocol/texture_decode.hpp"
#include "kami/protocol/caps.hpp"
#include <openjpeg.h>
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>

namespace kami::protocol::texture {
namespace {
struct Memory {
    std::span<const std::uint8_t> source;
    std::size_t at=0U;
};
OPJ_SIZE_T OPJ_CALLCONV read(void* destination,OPJ_SIZE_T bytes,
                            void* userdata) {
    auto* m=static_cast<Memory*>(userdata);
    if(!m||!destination||m->at>=m->source.size())
        return static_cast<OPJ_SIZE_T>(-1);
    const auto size=std::min<std::size_t>(bytes,m->source.size()-m->at);
    std::memcpy(destination,m->source.data()+m->at,size);
    m->at+=size;
    return size;
}
OPJ_OFF_T OPJ_CALLCONV skip(OPJ_OFF_T bytes,void* userdata){
    auto* m=static_cast<Memory*>(userdata);
    if(!m)return -1;
    if(bytes>=0){
        const auto n=static_cast<std::uint64_t>(bytes);
        if(n>m->source.size()-m->at)return -1;
        m->at+=static_cast<std::size_t>(n);
    }else{
        const auto n=static_cast<std::uint64_t>(-(bytes+1))+1U;
        if(n>m->at)return -1;
        m->at-=static_cast<std::size_t>(n);
    }
    return bytes;
}
OPJ_BOOL OPJ_CALLCONV seek(OPJ_OFF_T pos,void* userdata){
    auto* m=static_cast<Memory*>(userdata);
    if(!m||pos<0||static_cast<std::uint64_t>(pos)>m->source.size())
        return OPJ_FALSE;
    m->at=static_cast<std::size_t>(pos);
    return OPJ_TRUE;
}
void OPJ_CALLCONV ignore(const char*,void*) {}
std::uint8_t to_byte(std::int32_t raw,const opj_image_comp_t& comp){
    const std::uint32_t shift=comp.sgnd?1U<<(comp.prec-1U):0U;
    const auto maximum=(1U<<comp.prec)-1U;
    const auto converted=static_cast<std::int64_t>(raw)+shift;
    const auto clamped=std::clamp<std::int64_t>(
        converted,0,static_cast<std::int64_t>(maximum));
    return static_cast<std::uint8_t>(
        (clamped*255+maximum/2U)/maximum);
}
bool compatible(const opj_image_t& image,bool pixel_storage_ready){
    if(image.x1<=image.x0||image.y1<=image.y0||
       !image.comps||image.numcomps==0U||image.numcomps>4U)
        return false;
    const std::uint32_t width=image.x1-image.x0;
    const std::uint32_t height=image.y1-image.y0;
    if(width>2048U||height>2048U||
       static_cast<std::uint64_t>(width)*height>4U*1024U*1024U)
        return false;
    if(image.numcomps==2U)return false; // unsupported gray+alpha
    for(std::uint32_t c=0;c<image.numcomps;++c){
        const auto& comp=image.comps[c];
        if((pixel_storage_ready&&!comp.data)||comp.w!=width||comp.h!=height||
           comp.prec==0U||comp.prec>16U||
           comp.dx!=1U||comp.dy!=1U||
           comp.x0!=image.x0||comp.y0!=image.y0)
            return false;
    }
    return true;
}
} // namespace
bool decode_jpeg2000(std::span<const std::uint8_t> j2c,
                     DecodedTexture& output) noexcept {
    if(j2c.size()<16U||j2c.size()>6U*1024U*1024U||
       !caps::jpeg2000_codestream_header(j2c))return false;
    try{
        const OPJ_CODEC_FORMAT format=
            j2c[0]==0xffU?OPJ_CODEC_J2K:OPJ_CODEC_JP2;
        using Codec=std::unique_ptr<opj_codec_t,decltype(&opj_destroy_codec)>;
        using Stream=std::unique_ptr<opj_stream_t,decltype(&opj_stream_destroy)>;
        using Image=std::unique_ptr<opj_image_t,decltype(&opj_image_destroy)>;
        Codec codec(opj_create_decompress(format),&opj_destroy_codec);
        if(!codec)return false;
        opj_set_error_handler(codec.get(),ignore,nullptr);
        opj_set_warning_handler(codec.get(),ignore,nullptr);
        opj_set_info_handler(codec.get(),ignore,nullptr);
        opj_dparameters_t parameters{};
        opj_set_default_decoder_parameters(&parameters);
        if(!opj_setup_decoder(codec.get(),&parameters)||
           !opj_decoder_set_strict_mode(codec.get(),OPJ_TRUE))
            return false;
        Memory source{j2c};
        Stream stream(opj_stream_create(8192U,OPJ_TRUE),
                      &opj_stream_destroy);
        if(!stream)return false;
        opj_stream_set_user_data(stream.get(),&source,nullptr);
        opj_stream_set_user_data_length(stream.get(),j2c.size());
        opj_stream_set_read_function(stream.get(),read);
        opj_stream_set_skip_function(stream.get(),skip);
        opj_stream_set_seek_function(stream.get(),seek);
        opj_image_t* decoded=nullptr;
        if(!opj_read_header(stream.get(),codec.get(),&decoded))
            return false;
        Image image(decoded,&opj_image_destroy);
        if(!image||!compatible(*image,false))return false;
        if(!opj_decode(codec.get(),stream.get(),image.get())||
           !opj_end_decompress(codec.get(),stream.get())||
           !compatible(*image,true))return false;
        DecodedTexture ready{};
        ready.width=image->x1-image->x0;
        ready.height=image->y1-image->y0;
        const std::size_t pixels=
            static_cast<std::size_t>(ready.width)*ready.height;
        ready.rgba.resize(pixels*4U);
        for(std::size_t i=0;i<pixels;++i){
            auto* p=ready.rgba.data()+i*4U;
            if(image->numcomps==1U){
                p[0]=p[1]=p[2]=to_byte(image->comps[0].data[i],
                                        image->comps[0]);
                p[3]=255U;
            }else{
                for(std::size_t c=0;c<3U;++c)
                    p[c]=to_byte(image->comps[c].data[i],
                                 image->comps[c]);
                p[3]=image->numcomps==4U?
                    to_byte(image->comps[3].data[i],
                            image->comps[3]):255U;
            }
        }
        output=std::move(ready);
        return true;
    }catch(...){return false;}
}
} // namespace kami::protocol::texture
