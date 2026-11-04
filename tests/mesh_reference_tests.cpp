#include "kami/protocol/mesh_reference.hpp"
#include <cstdint>
#include <vector>
using namespace kami::protocol;
int main(){
    std::vector<std::uint8_t> input{1,0x30,0,17,0,0,0};
    for(unsigned i=0;i<16;++i)input.push_back(static_cast<std::uint8_t>(i+1));
    input.push_back(5U);
    const auto uuid=mesh::mesh_asset_from_extras(input);
    if(uuid!="01020304-0506-0708-090a-0b0c0d0e0f10")return 1;
    input.back()=3U; // sculpt (not mesh) must never get a mesh job
    if(!mesh::mesh_asset_from_extras(input).empty())return 2;
    input.back()=0x85U; // mesh plus mirror flag
    if(mesh::mesh_asset_from_extras(input)!=uuid)return 3;
    auto malformed=input;malformed.pop_back();
    if(!mesh::mesh_asset_from_extras(malformed).empty())return 4;
    malformed=input;malformed[3]=200U;
    if(!mesh::mesh_asset_from_extras(malformed).empty())return 5;
    malformed=input;malformed[0]=2U;
    if(!mesh::mesh_asset_from_extras(malformed).empty())return 6;
    malformed=input;malformed.insert(malformed.end(),{1,2,3});
    if(!mesh::mesh_asset_from_extras(malformed).empty())return 7;
    malformed=input;
    for(unsigned i=7U;i<23U;++i)malformed[i]=0U;
    if(!mesh::mesh_asset_from_extras(malformed).empty())return 8;
    const auto inspected=mesh::inspect_extra_params(input);
    if(!inspected.valid||inspected.records!=1U||
       inspected.sculpt_records!=1U)return 9;
    if(mesh::inspect_extra_params(malformed).valid)return 10;
    std::vector<std::uint8_t> light{1U,0x20U,0U,16U,0U,0U,0U};
    light.insert(light.end(),16U,0U);
    const auto light_stats=mesh::inspect_extra_params(light);
    if(!light_stats.valid||light_stats.records!=1U||
       light_stats.sculpt_records!=0U||
       !light_stats.mesh_id.empty())return 11;
    return 0;
}
