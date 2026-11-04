#include "kami/protocol/mesh_lod.hpp"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

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
    return 0;
}
