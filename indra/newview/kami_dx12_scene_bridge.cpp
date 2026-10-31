#include "llviewerprecompiledheaders.h"

#include "kami_dx12_scene_bridge.h"

#include "llviewerobjectlist.h"
#include "llviewerobject.h"
#include "llviewercamera.h"
#include "lltextureentry.h"
#include "llvolume.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <utility>

namespace kami::dx12 {
namespace {

constexpr std::size_t kMaxMeshes = 256;
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
        if (!object || object->isDead() ||
            object->getPCode() != LL_PCODE_VOLUME ||
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
