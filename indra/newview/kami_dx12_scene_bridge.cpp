#include "llviewerprecompiledheaders.h"

#include "kami_dx12_scene_bridge.h"

#include "llviewerobjectlist.h"
#include "llviewerobject.h"
#include "llviewercamera.h"
#include "lltextureentry.h"
#include "llvolume.h"
#include "llvosurfacepatch.h"
#include "llsurfacepatch.h"
#include "llsurface.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace kami::dx12 {
namespace {

constexpr std::size_t kMaxMeshes = 256;
constexpr std::size_t kMaxTerrainPatches = 64;
constexpr std::size_t kMaxGeometryBytes = 32u * 1024u * 1024u;
constexpr S32 kMaxPerFaceVertices = 1 << 20;
constexpr S32 kMaxPerFaceIndices = 1 << 21;

float dot3(const LLVector3& lhs, const LLVector3& rhs)
{
    return lhs.mV[0] * rhs.mV[0] +
           lhs.mV[1] * rhs.mV[1] +
           lhs.mV[2] * rhs.mV[2];
}

bool finite3(const LLVector3& v)
{
    return std::isfinite(v.mV[0]) &&
           std::isfinite(v.mV[1]) &&
           std::isfinite(v.mV[2]);
}

std::array<float, 16> d3dPerspective(const LLViewerCamera& camera)
{
    // Left-handed camera-space projection. HLSL uses mul(row_vector, matrix)
    // and the shader declares row_major worldViewProjection. Depth range [0,1].
    const float nearZ = std::max(0.05f, camera.getNear());
    const float farZ = std::max(nearZ + 1.f, camera.getFar());
    const float viewRadians = std::max(0.05f, std::min(3.05f, camera.getView()));
    const float aspect = std::max(0.1f, camera.getAspect());
    const float focal = 1.f / std::tan(viewRadians * 0.5f);
    const float zscale = farZ / (farZ - nearZ);
    return {
        focal / aspect, 0.f, 0.f, 0.f,
        0.f, focal, 0.f, 0.f,
        0.f, 0.f, zscale, 1.f,
        0.f, 0.f, -nearZ * zscale, 0.f
    };
}

// Capture a real decoded Second Life heightfield patch as D3D12 index geometry.
// Terrain paint, neighboring-patch LOD edge stitching and texture blending
// require separate implementations; this is an untextured GPU world geometry pass.
bool appendTerrainPatch(const LLSurfacePatch* patch,
                        const LLViewerCamera& camera,
                        const LLVector3& origin, const LLVector3& right,
                        const LLVector3& up, const LLVector3& forward,
                        const std::array<float, 16>& projection,
                        WorldFrame& scene, SceneCaptureStats& stats,
                        std::size_t& uploadedBytes)
{
    if (!patch || !patch->getSurface() || !patch->getDataZ() ||
        !patch->isHeightsGenerated())
        return false;
    if (stats.submittedTerrainPatches >= kMaxTerrainPatches)
        return false;

    const S32 patchWidth = patch->getSurface()->getGridsPerPatchEdge();
    if (patchWidth < 2 || patchWidth > 64)
        return false;
    const U32 stride = std::max(1u, patch->getRenderStride());
    if (stride > static_cast<U32>(patchWidth) ||
        patchWidth % static_cast<S32>(stride) != 0)
        return false;
    const U32 segments = static_cast<U32>(patchWidth) / stride;
    if (segments < 1u)
        return false;

    const LLVector3 center =
        patch->getPointAgent(static_cast<U32>(patchWidth / 2),
                             static_cast<U32>(patchWidth / 2));
    const F32 radius = patchWidth * patch->getSurface()->getMetersPerGrid() * 1.5f;
    if (!finite3(center) || !camera.sphereInFrustum(center, radius))
        return false;

    const std::size_t vertexCount = (segments + 1u) * (segments + 1u);
    const std::size_t indexCount = segments * segments * 6u;
    const std::size_t bytes = vertexCount * sizeof(WorldVertex) +
                              indexCount * sizeof(std::uint32_t);
    if (scene.meshes.size() >= kMaxMeshes ||
        bytes > kMaxGeometryBytes - uploadedBytes)
    {
        stats.hitBudget = true;
        return false;
    }

    WorldMesh mesh;
    mesh.worldViewProjection = projection;
    mesh.vertices.reserve(vertexCount);
    mesh.indices.reserve(indexCount);
    for (U32 y = 0; y <= segments; ++y)
    {
        for (U32 x = 0; x <= segments; ++x)
        {
            const LLVector3 world = patch->getPointAgent(x * stride, y * stride);
            if (!finite3(world))
                return false;
            const LLVector3 relative = world - origin;
            WorldVertex vertex;
            vertex.position = {
                dot3(relative, right),
                dot3(relative, up),
                dot3(relative, forward)
            };
            vertex.color = {0.22f, 0.42f, 0.18f, 1.f};
            mesh.vertices.push_back(vertex);
        }
    }

    for (U32 y = 0; y < segments; ++y)
    {
        for (U32 x = 0; x < segments; ++x)
        {
            const U32 row = y * (segments + 1u) + x;
            const U32 next = row + segments + 1u;
            mesh.indices.push_back(row);
            mesh.indices.push_back(row + 1u);
            mesh.indices.push_back(next);
            mesh.indices.push_back(row + 1u);
            mesh.indices.push_back(next + 1u);
            mesh.indices.push_back(next);
        }
    }

    stats.submittedVertices += mesh.vertices.size();
    stats.submittedIndices += mesh.indices.size();
    stats.submittedMeshes++;
    stats.submittedTerrainPatches++;
    uploadedBytes += bytes;
    scene.meshes.push_back(std::move(mesh));
    return true;
}

} // namespace

WorldFrame captureLiveVolumeWorld(const LLViewerCamera& camera,
                                 SceneCaptureStats* stats)
{
    SceneCaptureStats results{};
    WorldFrame scene;
    scene.clearColor = {0.025f, 0.035f, 0.05f, 1.f};

    const LLVector3 origin = camera.getOrigin();
    const LLVector3 right = -camera.getLeftAxis();
    const LLVector3 up = camera.getUpAxis();
    const LLVector3 forward = camera.getAtAxis();
    const auto projection = d3dPerspective(camera);
    if (!finite3(origin) || !finite3(right) || !finite3(up) ||
        !finite3(forward))
    {
        if (stats) *stats = results;
        return scene;
    }

    const S32 count = gObjectList.getNumObjects();
    std::size_t uploadedBytes = 0;
    for (S32 objectNumber = 0; objectNumber < count; ++objectNumber)
    {
        LLViewerObject* object = gObjectList.getObject(objectNumber);
        ++results.visitedObjects;
        if (!object || object->isDead())
            continue;

        if (object->getPCode() == LLViewerObject::LL_VO_SURFACE_PATCH)
        {
            LLVOSurfacePatch* terrain = static_cast<LLVOSurfacePatch*>(object);
            appendTerrainPatch(terrain->getPatch(), camera, origin,
                               right, up, forward, projection,
                               scene, results, uploadedBytes);
            if (results.hitBudget) break;
            continue;
        }

        if (object->getPCode() != LL_PCODE_VOLUME ||
            object->getAvatarAncestor())
            continue; // Avatar attachments require a separate skinned pass.

        const LLVolume* volume = object->getVolumeConst();
        if (!volume) continue; // Scene geometry may not be decoded yet.
        const LLVector3 scale = object->getScale();
        const LLVector3 position = object->getRenderPosition();
        const LLQuaternion rotation = object->getRenderRotation();
        if (!finite3(scale) || !finite3(position) || !rotation.isFinite())
            continue;

        // Use the existing viewer's frustum without depending on any GL draw
        // buffers. Do not snapshot out-of-range distant scene geometry.
        const float radius = std::max(0.1f, scale.length() * 0.6f);
        if (!camera.sphereInFrustum(position, radius))
            continue;
        ++results.visibleVolumes;

        for (S32 faceNumber = 0; faceNumber < volume->getNumVolumeFaces(); ++faceNumber)
        {
            if (scene.meshes.size() >= kMaxMeshes)
            {
                results.hitBudget = true;
                break;
            }
            const LLVolumeFace& face = volume->getVolumeFace(faceNumber);
            if (face.mWeights)
            {
                ++results.skippedRiggedFaces;
                continue;
            }
            if (!face.mPositions || !face.mIndices ||
                face.mNumVertices < 3 || face.mNumIndices < 3 ||
                face.mNumIndices % 3 != 0 ||
                face.mNumVertices > kMaxPerFaceVertices ||
                face.mNumIndices > kMaxPerFaceIndices)
                continue;

            LLColor4 diffuse(1.f, 1.f, 1.f, 1.f);
            if (faceNumber < object->getNumTEs())
            {
                if (const LLTextureEntry* entry =
                    object->getTE(static_cast<U8>(faceNumber)))
                    diffuse = entry->getColor();
            }
            // No alpha sorting or blending in this DX12 prototype. Skip
            // instead of falsely rendering translucent SL objects as opaque.
            if (!std::isfinite(diffuse.mV[3]) || diffuse.mV[3] < 0.999f)
            {
                ++results.skippedTranslucentFaces;
                continue;
            }

            const std::size_t required =
                static_cast<std::size_t>(face.mNumVertices) * sizeof(WorldVertex) +
                static_cast<std::size_t>(face.mNumIndices) * sizeof(std::uint32_t);
            if (required > kMaxGeometryBytes - uploadedBytes)
            {
                results.hitBudget = true;
                break;
            }

            WorldMesh mesh;
            mesh.worldViewProjection = projection;
            mesh.vertices.reserve(static_cast<std::size_t>(face.mNumVertices));
            mesh.indices.reserve(static_cast<std::size_t>(face.mNumIndices));

            bool valid = true;
            for (S32 vertex = 0; vertex < face.mNumVertices; ++vertex)
            {
                const F32* local = face.mPositions[vertex].getF32ptr();
                if (!std::isfinite(local[0]) ||
                    !std::isfinite(local[1]) ||
                    !std::isfinite(local[2]))
                {
                    valid = false;
                    break;
                }
                LLVector3 scaled(local[0] * scale.mV[0],
                                 local[1] * scale.mV[1],
                                 local[2] * scale.mV[2]);
                const LLVector3 relative =
                    (scaled * rotation + position) - origin;
                if (!finite3(relative))
                {
                    valid = false;
                    break;
                }

                WorldVertex output;
                output.position = {
                    dot3(relative, right),
                    dot3(relative, up),
                    dot3(relative, forward)
                };
                output.color = {
                    diffuse.mV[0], diffuse.mV[1],
                    diffuse.mV[2], 1.f
                };
                mesh.vertices.push_back(output);
            }
            if (!valid) continue;
            for (S32 index = 0; index < face.mNumIndices; ++index)
            {
                const U16 value = face.mIndices[index];
                if (value >= face.mNumVertices)
                {
                    valid = false;
                    break;
                }
                mesh.indices.push_back(value);
            }
            if (!valid) continue;

            results.submittedVertices += mesh.vertices.size();
            results.submittedIndices += mesh.indices.size();
            ++results.submittedMeshes;
            uploadedBytes += required;
            scene.meshes.push_back(std::move(mesh));
        }
        if (results.hitBudget) break;
    }

    if (stats) *stats = results;
    return scene;
}

} // namespace kami::dx12
