#include "kami/protocol/world_wire.hpp"
#include <cstdint>
#include <vector>

using namespace kami::protocol;
namespace {
lludp::Packet packet(unsigned id,std::vector<std::uint8_t> tail) {
    lludp::Packet p;
    p.body.push_back(static_cast<std::uint8_t>(id));
    p.body.insert(p.body.end(),tail.begin(),tail.end());
    return p;
}
}
int main(){
    auto layer=packet(11,{0x4c,4,0,1,2,3,4});
    auto l=worldwire::inspect(layer);
    if(l.kind!=worldwire::Kind::LayerData||!l.complete_frame||
       l.blocks!=1||l.payload_bytes!=4)return 1;
    layer.body[2]=0xff;
    if(worldwire::inspect(layer).kind!=worldwire::Kind::Invalid)return 2;
    auto cached=packet(14,{0,0,0,0,0,0,0,0,0,0,1,
        7,0,0,0, 9,0,0,0, 5,0,0,0});
    auto c=worldwire::inspect(cached);
    if(c.kind!=worldwire::Kind::ObjectCached||!c.complete_frame||
       c.first_local_id!=7||c.blocks!=1)return 3;
    auto compressed=packet(13,{0,0,0,0,0,0,0,0,0,0,1,
        0,0,0,0,3,0, 12,14,16});
    auto cc=worldwire::inspect(compressed);
    if(cc.kind!=worldwire::Kind::ObjectCompressed||!cc.complete_frame||
       cc.blocks!=1)return 4;
    compressed.body.pop_back();
    if(worldwire::inspect(compressed).kind!=worldwire::Kind::Invalid)
       return 5;
    auto terse=packet(15,{0,0,0,0,0,0,0,0,0,0,1,
        3,10,11,12,2,0,21,22});
    auto te=worldwire::inspect(terse);
    if(te.kind!=worldwire::Kind::ObjectTerse||!te.complete_frame)
        return 6;
    auto kill=packet(16,{2,9,0,0,0,10,0,0,0});
    auto k=worldwire::inspect(kill);
    if(k.kind!=worldwire::Kind::ObjectKilled||!k.complete_frame||
       k.blocks!=2||k.first_local_id!=9)return 7;
    // A full update is intentionally partial: do not claim decoded mesh.
    auto full=packet(12,std::vector<std::uint8_t>(11+25,0));
    full.body[11]=1;
    auto f=worldwire::inspect(full);
    if(f.kind!=worldwire::Kind::ObjectFull||f.complete_frame||
       f.blocks!=1)return 8;
    if(worldwire::inspect(packet(8,{1})).kind!=
       worldwire::Kind::Unrelated)return 9;
    return 0;
}
