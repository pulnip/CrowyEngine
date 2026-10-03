#include "EditorPick.hpp"

#include <algorithm>
#include <cmath>
#include <utility>
#include <vector>

#include "EnumUtil.hpp"

namespace Crowy
{
    namespace
    {
        using Candidate = std::pair<f32, usize>;

        Vec3 toVec3(const Vec4& v) noexcept {
            return Vec3{v.x, v.y, v.z};
        }

        // the ray in the primitive's model space; t keeps its world meaning
        Ray3D intoModel(const Ray3D& ray, const Mat4& localToWorld) {
            const auto toModel = inverseAffine(localToWorld);
            const auto& o = ray.origin;
            const auto& d = ray.direction;

            return Ray3D{
                .origin = toVec3(toModel * Vec4{o.x, o.y, o.z, 1.0f}),
                .direction = toVec3(toModel * Vec4{d.x, d.y, d.z, 0.0f})
            };
        }

        std::optional<f32> nearestTriangle(const Ray3D& ray, const MeshData& mesh) {
            std::optional<f32> best;
            const auto& v = mesh.vertices;
            for(usize i = 0; i + 2 < mesh.indices.size(); i += 3) {
                const auto t = intersectRayTriangle(
                    ray,
                    v[mesh.indices[i]].position,
                    v[mesh.indices[i + 1]].position,
                    v[mesh.indices[i + 2]].position
                );
                if(t && (!best || *t < *best))
                    best = t;
            }

            return best;
        }
    }

    Ray3D rayThroughPixel(const EditorCamera& camera, Vec2 pixel, Vec2 viewport) {
        const auto ndcX = 2.0f * pixel.x / viewport.x - 1.0f;
        const auto ndcY = 1.0f - 2.0f * pixel.y / viewport.y;
        const auto aspect = viewport.x / viewport.y;

        const auto rotation = rotateMat(camera.Rotation());
        const auto right = toVec3(rotation[0]);
        const auto up = toVec3(rotation[1]);
        const auto forward = toVec3(rotation[2]);

        if(camera.lens.orthographic) {
            const auto half = camera.lens.orthoHalfHeight;

            return Ray3D{
                .origin = camera.position + right * (ndcX * half * aspect) + up * (ndcY * half),
                .direction = forward
            };
        }

        const auto tanHalf = std::tan(0.5f * camera.lens.fovY);

        return Ray3D{
            .origin = camera.position,
            .direction = normalize(right * (ndcX * tanHalf * aspect) + up * (ndcY * tanHalf) + forward)
        };
    }

    std::optional<PickHit> pickScene(const RenderScene& scene, const Ray3D& ray, const PickMeshes& meshes) {
        const auto rows = scene.Primitives().All();

        std::vector<Candidate> candidates;
        for(usize i = 0; i < rows.size(); ++i) {
            if(!hasFlag(rows[i].flags, PrimitiveFlags::Visible))
                continue;
            if(const auto t = intersectRayAABB3D(ray, rows[i].worldBounds))
                candidates.emplace_back(*t, i);
        }
        std::ranges::sort(candidates);

        std::optional<PickHit> best;
        for(const auto& [enter, row]: candidates) {
            if(best && enter > best->distance)
                break;

            const auto handle = scene.Primitives().HandleAt(row);
            const auto modelRay = intoModel(ray, rows[row].localToWorld);
            for(const auto* mesh: meshes(handle)) {
                const auto t = nearestTriangle(modelRay, *mesh);
                if(t && (!best || *t < best->distance))
                    best = PickHit{.primitive = handle, .distance = *t};
            }
        }

        return best;
    }
}
