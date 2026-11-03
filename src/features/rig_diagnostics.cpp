#include "kami/features/rig_diagnostics.hpp"
#include <cmath>
#include <limits>

namespace kami::rig {
Audit inspect(std::span<const world::Vertex> vertices) noexcept {
    Audit result{};
    result.vertex_count=vertices.size();
    if(vertices.empty()||vertices.size()>2000000U) {
        result.status=Compatibility::Invalid;
        result.corrupt_vertices=vertices.size();
        return result;
    }
    for(const auto& v:vertices) {
        // Prevent malicious/invalid rig streams from reaching GPU SRV indexing.
        if(!std::isfinite(v.px)||!std::isfinite(v.py)||!std::isfinite(v.pz)||
           !std::isfinite(v.nx)||!std::isfinite(v.ny)||!std::isfinite(v.nz)||
           !std::isfinite(v.body_region) ||v.body_region<0.f||v.body_region>3.f) {
            ++result.corrupt_vertices;continue;
        }
        if(v.bone_weights==0U) {
            if(v.body_region>.5f)++result.corrupt_vertices;
            continue;
        }
        Influences influence{};
        if(!unpack_influences({v.bone_indices,v.bone_weights},influence)) {
            ++result.corrupt_vertices;continue;
        }
        ++result.weighted_vertices;
        for(std::size_t i=0;i<kInfluences;++i) {
            if(influence.weights[i]<=0.f)continue;
            ++result.joint_affected_vertices[influence.joints[i]];
        }
    }
    if(result.corrupt_vertices) {
        result.status=Compatibility::Invalid;return result;
    }
    if(!result.weighted_vertices) {
        result.status=Compatibility::Unrigged;return result;
    }
    result.left_pec=result.joint_affected_vertices[static_cast<int>(Joint::LeftPec)]>0;
    result.right_pec=result.joint_affected_vertices[static_cast<int>(Joint::RightPec)]>0;
    result.belly=result.joint_affected_vertices[static_cast<int>(Joint::Belly)]>0;
    result.butt=result.joint_affected_vertices[static_cast<int>(Joint::Butt)]>0;
    result.status=result.left_pec&&result.right_pec&&result.belly&&result.butt?
        Compatibility::PhysicsReady:Compatibility::Partial;
    return result;
}
const wchar_t* compatibility_label(Compatibility status) noexcept {
    switch(status) {
    case Compatibility::Unrigged:return L"NO PHYSICS WEIGHTS";
    case Compatibility::Partial:return L"PARTIALLY WEIGHTED";
    case Compatibility::PhysicsReady:return L"PHYSICS WEIGHTS READY";
    case Compatibility::Invalid:return L"INVALID RIG DATA";
    }
    return L"UNKNOWN RIG";
}
} // namespace kami::rig
