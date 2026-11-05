#pragma once
#include <array>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace kami::world {
// A stable Texture2DArray (slice 0 is always procedural fallback).
// Find exactly the SL image layers that were reassigned, without uploading
// all 25 GPU subresources whenever one JPEG2000 image becomes visible.
[[nodiscard]] inline std::vector<std::uint16_t> changed_atlas_layers(
    const std::map<std::string,std::uint16_t>& previous,
    const std::map<std::string,std::uint16_t>& next,
    std::uint16_t max_layers=24U){
    if(max_layers==0U||max_layers>24U)return {};
    std::array<std::string,25U> before{},after{};
    for(const auto& [id,slot]:previous)
        if(slot>0U&&slot<=max_layers)before[slot]=id;
    for(const auto& [id,slot]:next)
        if(slot>0U&&slot<=max_layers)after[slot]=id;
    std::vector<std::uint16_t> changed;
    changed.reserve(max_layers);
    for(std::uint16_t slot=1U;slot<=max_layers;++slot)
        if(before[slot]!=after[slot])changed.push_back(slot);
    return changed;
}
} // namespace kami::world
