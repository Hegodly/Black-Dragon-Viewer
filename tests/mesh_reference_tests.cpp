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
    auto checked=mesh::inspect_extra_params(input);
    if(!checked.valid||checked.records!=1U||
       checked.sculpt_records!=1U||checked.mesh_records!=1U||
       checked.mesh_id!=uuid)return 9;
    auto empty=mesh::inspect_extra_params({0U});
    if(!empty.valid||empty.records!=0U||!empty.mesh_id.empty())
        return 10;
    auto twice=input;
    twice[0]=2U;
    twice.insert(twice.end(),{0x10U,0U,0U,0U,0U,0U});
    checked=mesh::inspect_extra_params(twice);
    if(!checked.valid||checked.records!=2U||
       checked.mesh_records!=1U)return 11;
    twice.push_back(9U); // trailing bytes invalidate entire object blob
    if(mesh::inspect_extra_params(twice).valid)return 12;
    return 0;

