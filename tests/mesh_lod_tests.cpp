#include "kami/protocol/mesh_lod.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>


namespace {
using Bytes=std::vector<std::uint8_t>;
void be32(Bytes& b,std::uint32_t n) {
    for(int shift=24;shift>=0;shift-=8)b.push_back((n>>shift)&255U);
}
void key(Bytes& b,const char* s) {
    b.push_back('k');std::uint32_t n=0;while(s[n])++n;
    be32(b,n);b.insert(b.end(),s,s+n);
}
void blob(Bytes& b,const Bytes& data) {
    b.push_back('b');be32(b,static_cast<std::uint32_t>(data.size()));
    b.insert(b.end(),data.begin(),data.end());
}
Bytes mesh_fixture(std::uint32_t vertices,const Bytes& indices,
                   const char* lod_name="high_lod") {
    Bytes points(vertices*6U,0U);
    if(vertices>1U)points[6]=points[7]=255U;
    if(vertices>2U)points[(vertices-1U)*6U+2U]=
        points[(vertices-1U)*6U+3U]=255U;
    Bytes lod{'[',0,0,0,1,'{',0,0,0,2};
    key(lod,"Position");blob(lod,points);
    key(lod,"TriangleList");blob(lod,indices);
    lod.push_back('}');lod.push_back(']');
    // RFC 1950 zlib wrapper with stored DEFLATE blocks, avoiding a new
    // compression dependency in this parser regression test.
    Bytes zipped{0x78,0x01};
    for(std::size_t at=0;at<lod.size();) {
        const auto n=std::min<std::size_t>(65535U,lod.size()-at);
        zipped.push_back(at+n==lod.size()?1U:0U);
        zipped.push_back(n&255U);zipped.push_back((n>>8U)&255U);
        const auto inverse=static_cast<std::uint16_t>(~n);
        zipped.push_back(inverse&255U);zipped.push_back(inverse>>8U);
        zipped.insert(zipped.end(),lod.begin()+at,lod.begin()+at+n);at+=n;
    }
    std::uint32_t a=1,b=0;
    for(const auto byte:lod){a=(a+byte)%65521U;b=(b+a)%65521U;}
    be32(zipped,(b<<16U)|a);
    Bytes asset{'{',0,0,0,1};
    key(asset,lod_name);asset.insert(asset.end(),{'{',0,0,0,2});
    key(asset,"offset");asset.push_back('i');be32(asset,0);
    key(asset,"size");asset.push_back('i');
    be32(asset,static_cast<std::uint32_t>(zipped.size()));
    asset.push_back('}');asset.push_back('}');
    asset.insert(asset.end(),zipped.begin(),zipped.end());return asset;
}
}

// Deterministic tiny SECOND LIFE-shaped binary LLSD mesh asset:
// binary header with high_lod(offset=0,size=79), zlib payload containing
// one submesh with three quantized positions and indices [0,1,2].
int main() {
    using namespace kami::protocol::mesh;
    const std::vector<std::uint8_t> bytes={
0x3c,0x3f,0x6c,0x6c,0x73,0x64,0x2f,0x62,0x69,0x6e,0x61,0x72,0x79,0x3f,0x3e,0x0a,
0x7b,0x00,0x00,0x00,0x01,0x6b,0x00,0x00,0x00,0x08,0x68,0x69,0x67,0x68,0x5f,0x6c,
0x6f,0x64,0x7b,0x00,0x00,0x00,0x02,0x6b,0x00,0x00,0x00,0x06,0x6f,0x66,0x66,0x73,
0x65,0x74,0x69,0x00,0x00,0x00,0x00,0x6b,0x00,0x00,0x00,0x04,0x73,0x69,0x7a,0x65,
0x69,0x00,0x00,0x00,0x4f,0x7d,0x7d,0x78,0x9c,0xb3,0xb1,0xcf,0xc9,0x29,0x4e,0xd1,
0x4f,0xca,0xcc,0x4b,0x2c,0xaa,0xb4,0xb7,0xe3,0x8a,0x66,0x60,0x60,0x60,0xac,0x06,
0x12,0x4c,0xd9,0x40,0x82,0x23,0x20,0xbf,0x38,0xb3,0x24,0x33,0x3f,0x2f,0x09,0xc8,
0x11,0x62,0x00,0x83,0xff,0xff,0x11,0x34,0x48,0x0d,0x4f,0x48,0x51,0x66,0x62,0x5e,
0x7a,0x4e,0xaa,0x4f,0x66,0x71,0x09,0x48,0x1d,0x1b,0xd0,0x04,0x06,0x26,0x86,0xda,
0x58,0x00,0x04,0x06,0x15,0x05};
    LOD good{};
    if(!decode_mesh_lod(bytes,good)||good.triangles.size()!=1U||
       good.decoded_bytes!=92U)return 1;
    const auto& tri=good.triangles[0].pos;
    if(std::abs(tri[0]+0.5f)>0.001f||
       std::abs(tri[1]+0.5f)>0.001f||
       std::abs(tri[3]-0.5f)>0.001f||
       std::abs(tri[7]-0.5f)>0.001f)return 2;
    auto bad=bytes;
    bad.resize(50U);
    if(decode_mesh_lod(bad,good))return 3;
    bad=bytes;bad[0]=0U;
    if(decode_mesh_lod(bad,good))return 4;
    bad=bytes;bad[67]=0xffU; // corrupted compressed deflate stream
    if(decode_mesh_lod(bad,good))return 5;

    // Real SL mesh headers may include a medium_lod map with size 0.
    // The required high_lod must remain usable, without accepting an
    // invalid medium_lod with nonzero size or a malformed stream.
    std::vector<std::uint8_t> missing_medium=bytes;
    const std::vector<std::uint8_t> marker{0x7d,0x7d,0x78,0x9c};
    const auto close=std::search(missing_medium.begin(),missing_medium.end(),
                                 marker.begin(),marker.end());
    if(close==missing_medium.end()||missing_medium[20]!=1U)return 6;
    const auto at=std::size_t(close-missing_medium.begin())+1U;
    missing_medium[20]=2U; // root LLSD map: high_lod + medium_lod
    const std::vector<std::uint8_t> medium={
        'k',0,0,0,10,'m','e','d','i','u','m','_','l','o','d',
        '{',0,0,0,2,
        'k',0,0,0,6,'o','f','f','s','e','t','i',
        0xff,0xff,0xff,0xff, // offset -1 for missing medium
        'k',0,0,0,4,'s','i','z','e','i',
        0,0,0,0, // size 0 is the missing LOD sentinel
        '}'
    };
    missing_medium.insert(missing_medium.begin()+at,
                          medium.begin(),medium.end());
    if(!decode_mesh_lod(missing_medium,good)||
       good.triangles.size()!=1U||good.decoded_bytes!=92U)return 7;
    bad=missing_medium;
    bad[at+medium.size()-2U]=7U; // nonzero size with missing offset
    if(decode_mesh_lod(bad,good))return 8;
    bad=missing_medium;
    bad[67U+medium.size()]=0xffU; // damaged deflate stream after new header
    if(decode_mesh_lod(bad,good))return 9;
    // SL's official LLSDSerialize::toBinary emits raw binary LLSD:
    // no "<?llsd/binary?>" prefix on the mesh header or zipped LOD.
    // A genuine mesh HTTP 200 response previously failed immediately.
    auto raw=bytes;
    if(raw.size()<17U||raw[15U]!='\n'||raw[16U]!='{')return 10;
    raw.erase(raw.begin(),raw.begin()+16U);
    if(!decode_mesh_lod(raw,good)||good.triangles.size()!=1U||
       good.decoded_bytes!=92U)return 11;
    // Forward-compatible deprecated binary header remains supported.
    auto deprecated=raw;
    constexpr char old_header[]="<? LLSD/Binary ?>\n";
    deprecated.insert(deprecated.begin(),old_header,
                      old_header+sizeof(old_header)-1U);
    if(!decode_mesh_lod(deprecated,good)||good.triangles.size()!=1U)
        return 12;
    // Random raw buffers are never accepted as mesh LLSD.
    auto nonsense=raw;
    nonsense[0]=0x30U;
    if(decode_mesh_lod(nonsense,good))return 13;

    // Official raw binary mesh asset with one static face, three UVs
    // and TexCoord0Domain. Confirms UV interpolation and DX12 V flip.
    // The old KAMI decoder silently left all triangle UVs at zero.
    const std::vector<std::uint8_t> uv_asset={
    0x7b,0x00,0x00,0x00,0x01,0x6b,0x00,0x00,0x00,0x08,0x68,0x69,0x67,0x68,0x5f,0x6c,0x6f,0x64,
    0x7b,0x00,0x00,0x00,0x02,0x6b,0x00,0x00,0x00,0x06,0x6f,0x66,0x66,0x73,0x65,0x74,0x69,0x00,
    0x00,0x00,0x00,0x6b,0x00,0x00,0x00,0x04,0x73,0x69,0x7a,0x65,0x69,0x00,0x00,0x00,0x7d,0x7d,
    0x7d,0x78,0x9c,0x8b,0x66,0x60,0x60,0x60,0xac,0x06,0x12,0x2c,0xd9,0x40,0x82,0x23,0x20,0xbf,
    0x38,0xb3,0x24,0x33,0x3f,0x2f,0x09,0xc8,0x11,0x62,0x00,0x83,0xff,0xff,0x11,0x34,0x48,0x0d,
    0x67,0x48,0x6a,0x85,0x73,0x7e,0x7e,0x51,0x8a,0x01,0x48,0x11,0x0f,0x42,0xc9,0xff,0xff,0x20,
    0x69,0x7e,0xb8,0xb4,0x4b,0x7e,0x6e,0x62,0x66,0x1e,0xc8,0x6c,0x26,0x90,0x04,0xb3,0x6f,0x66,
    0x5e,0x34,0x88,0x53,0xc4,0x00,0x05,0x70,0x46,0x2c,0x44,0x3e,0xb1,0x02,0x22,0x6f,0xff,0x01,
    0x2a,0x0f,0x63,0xc4,0xd6,0x82,0x14,0xf0,0x84,0x14,0x65,0x26,0xe6,0xa5,0xe7,0xa4,0xfa,0x64,
    0x16,0x97,0x80,0xec,0x66,0x03,0x3a,0x9d,0x81,0x89,0xa1,0x36,0x16,0x00,0xc4,0xbd,0x27,0xbc
    };
    LOD uv_lod{};
    if(!decode_mesh_lod(uv_asset,uv_lod)||
       uv_lod.triangles.size()!=1U||
       !uv_lod.triangles[0].has_uv)return 14;
    const auto& uv=uv_lod.triangles[0].uv;
    if(std::abs(uv[0])>.001f||std::abs(uv[1]-1.f)>.001f||
       std::abs(uv[2]-1.f)>.001f||std::abs(uv[3]-1.f)>.001f||
       std::abs(uv[4])>.001f||std::abs(uv[5])>.001f)
        return 15;
    // A valid mesh lacking UV is still renderable via fallback layer zero.
    if(!decode_mesh_lod(bytes,good)||good.triangles[0].has_uv)
        return 16;
    // One collapsed triangle must not discard the following valid face.
    auto mixed=mesh_fixture(3U,{0,0,0,0,1,0,0,0,1,0,2,0});
    if(!decode_mesh_lod(mixed,good)||good.triangles.size()!=1U)return 17;
    // Invalid references remain hard failures, with output unchanged.
    const auto prior=good.triangles[0].pos;
    if(decode_mesh_lod(mesh_fixture(3U,{0,0,1,0,3,0}),good)||
       good.triangles.size()!=1U||good.triangles[0].pos!=prior)return 18;
    if(decode_mesh_lod(mesh_fixture(3U,{0,0,0,0,1,0}),good))return 19;
    // An unsigned 16-bit index addresses 65536 vertices, including 65535.
    if(!decode_mesh_lod(mesh_fixture(65536U,{0,0,1,0,255,255}),good)||
       good.triangles.size()!=1U)return 20;
    if(decode_mesh_lod(mesh_fixture(65537U,{0,0,1,0,255,255}),good))return 21;
    // R67: four official LOD keys. A mesh with no high/medium block
    // still has legitimate static geometry at low or lowest detail.
    const Bytes single_face{0,0,1,0,2,0};
    auto low_only=mesh_fixture(3U,single_face,"low_lod");
    auto lowest_only=mesh_fixture(3U,single_face,"lowest_lod");
    if(!decode_mesh_lod(low_only,good)||good.triangles.size()!=1U||
       !decode_mesh_lod(lowest_only,good)||good.triangles.size()!=1U)
        return 22;
    // Do not regress a present valid medium LOD.
    auto medium_only=mesh_fixture(3U,single_face,"medium_lod");
    if(!decode_mesh_lod(medium_only,good)||good.triangles.size()!=1U)
        return 23;
    // The earlier 20k-triangle per-LOD cutoff rejected supported mesh
    // content even though its checked 16-bit indices and 8MB inflation
    // buffer were valid. Keep the new ceiling strictly bounded at 60k.
    Bytes twenty_k_plus;twenty_k_plus.reserve(20001U*6U);
    for(unsigned i=0;i<20001U;++i)
        twenty_k_plus.insert(twenty_k_plus.end(),single_face.begin(),single_face.end());
    auto dense_lod=mesh_fixture(3U,twenty_k_plus,"medium_lod");
    if(!decode_mesh_lod(dense_lod,good)||good.triangles.size()!=20001U)
        return 24;
    // Still reject offsets, invalid vertex indices and oversized LODs.
    Bytes oversized;oversized.reserve(60001U*6U);
    for(unsigned i=0;i<60001U;++i)
        oversized.insert(oversized.end(),single_face.begin(),single_face.end());
    const auto too_dense=mesh_fixture(3U,oversized,"medium_lod");
    if(decode_mesh_lod(too_dense,good)||good.triangles.size()!=20001U)
        return 25;
    return 0;
}
