#include "ModelLoader.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <format>
#include <limits>
#include <memory>
#include <stdexcept>
#include <vector>

#include <ufbx.h>

#include "LinearAlgebra.hpp"
#include "StringUtil.hpp"

namespace
{
    using namespace Crowy;

    using SceneOwner = std::unique_ptr<ufbx_scene, decltype(&ufbx_free_scene)>;

    // a slot's corners, one vertex per triangle corner until indexing
    struct SlotBuilder {
        Str material;
        std::vector<Vertex> corners;
    };

    constexpr StrView toView(ufbx_string str) noexcept {
        return StrView{str.data, str.length};
    }

    constexpr Vec3 toVec3(ufbx_vec3 v) noexcept {
        return Vec3{
            static_cast<f32>(v.x),
            static_cast<f32>(v.y),
            static_cast<f32>(v.z)
        };
    }

    constexpr Vec3 componentMin(Vec3 a, Vec3 b) noexcept {
        return Vec3{std::min(a.x, b.x), std::min(a.y, b.y), std::min(a.z, b.z)};
    }

    constexpr Vec3 componentMax(Vec3 a, Vec3 b) noexcept {
        return Vec3{std::max(a.x, b.x), std::max(a.y, b.y), std::max(a.z, b.z)};
    }

    AABB3D boundsOf(const std::vector<Vertex>& vertices) {
        if(vertices.empty())
            return AABB3D{.center = zeros(), .halfScale = zeros()};

        constexpr auto Huge = std::numeric_limits<f32>::max();
        auto low = Vec3{Huge, Huge, Huge};
        auto high = Vec3{-Huge, -Huge, -Huge};
        for(const auto& vertex: vertices) {
            low = componentMin(low, vertex.position);
            high = componentMax(high, vertex.position);
        }

        return AABB3D{
            .center = 0.5f * (low + high),
            .halfScale = 0.5f * (high - low)
        };
    }

    AABB3D unionOf(const AABB3D& a, const AABB3D& b) {
        const auto low =
            componentMin(a.center - a.halfScale, b.center - b.halfScale);
        const auto high =
            componentMax(a.center + a.halfScale, b.center + b.halfScale);

        return AABB3D{
            .center = 0.5f * (low + high),
            .halfScale = 0.5f * (high - low)
        };
    }

    SceneOwner loadScene(const std::filesystem::path& path) {
        ufbx_load_opts opts{};
        opts.target_axes = ufbx_axes_left_handed_y_up;
        opts.target_unit_meters = 1.0f;
        // the conversion lands in the vertices, not in a root transform a
        // caller could forget
        opts.space_conversion = UFBX_SPACE_CONVERSION_MODIFY_GEOMETRY;
        // the mirror leaves the index order alone, which turns FBX's
        // counter-clockwise fronts into MeshData's clockwise ones
        opts.handedness_conversion_axis = UFBX_MIRROR_AXIS_Z;
        opts.generate_missing_normals = true;
        opts.normalize_normals = true;

        const auto utf8Path = toUTF8String(path);
        ufbx_error error{};
        auto* scene = ufbx_load_file(utf8Path.c_str(), &opts, &error);
        if(scene == nullptr) {
            char message[1024];
            ufbx_format_error(message, sizeof(message), &error);

            throw std::runtime_error(
                std::format("cannot load model {}: {}", path, message)
            );
        }

        return SceneOwner(scene, ufbx_free_scene);
    }

    // which slot a part of a node's mesh draws into; the node's own
    // materials win, since one mesh may be instanced with different ones
    u32 slotOf(
        const ufbx_node& node,
        const ufbx_mesh& mesh,
        usize partIndex,
        u32 noMaterialSlot
    ) {
        if(partIndex < node.materials.count && node.materials.data[partIndex])
            return node.materials.data[partIndex]->typed_id;
        if(partIndex < mesh.materials.count && mesh.materials.data[partIndex])
            return mesh.materials.data[partIndex]->typed_id;

        return noMaterialSlot;
    }

    Vertex makeCorner(
        const ufbx_mesh& mesh,
        u32 index,
        const ufbx_matrix& toModel,
        const ufbx_matrix& normalToModel
    ) {
        // zeroed so ufbx_generate_indices, which compares with memcmp, sees
        // equal corners as equal
        Vertex vertex{};

        vertex.position =
            toVec3(ufbx_transform_position(&toModel, mesh.vertex_position[index]));
        if(mesh.vertex_normal.exists) {
            vertex.normal = normalize(toVec3(
                ufbx_transform_direction(&normalToModel, mesh.vertex_normal[index])
            ));
        }
        if(mesh.vertex_uv.exists) {
            const auto uv = mesh.vertex_uv[index];
            // FBX puts (0, 0) at the bottom-left, MeshData at the top-left
            vertex.texCoord = Vec2{
                static_cast<f32>(uv.x),
                1.0f - static_cast<f32>(uv.y)
            };
        }

        return vertex;
    }

    // Appends a node's triangles to the slots, moved into model space; a
    // node whose transform mirrors keeps clockwise fronts by swapping two
    // corners of each triangle.
    void addNode(const ufbx_node& node, std::vector<SlotBuilder>& slots) {
        const auto& mesh = *node.mesh;
        if(!mesh.vertex_position.exists)
            return;

        const auto& toModel = node.geometry_to_world;
        const auto normalToModel = ufbx_matrix_for_normals(&toModel);
        const bool mirrors = ufbx_matrix_determinant(&toModel) < 0.0;
        const auto noMaterialSlot = static_cast<u32>(slots.size() - 1);

        // ufbx_triangulate_face needs room for the mesh's largest face
        std::vector<u32> faceCorners(
            3 * std::max<usize>(mesh.max_face_triangles, 1)
        );

        for(usize partIndex = 0; partIndex < mesh.material_parts.count; ++partIndex) {
            const auto& part = mesh.material_parts.data[partIndex];
            if(part.num_triangles == 0)
                continue;

            auto& corners =
                slots[slotOf(node, mesh, partIndex, noMaterialSlot)].corners;
            for(const auto faceIndex: part.face_indices) {
                // 0 for the point and line faces FBX also calls faces
                const auto triangles = ufbx_triangulate_face(
                    faceCorners.data(),
                    faceCorners.size(),
                    &mesh,
                    mesh.faces.data[faceIndex]
                );
                for(u32 t = 0; t < triangles; ++t) {
                    const auto* triangle = faceCorners.data() + 3 * t;
                    const std::array<u32, 3> order = mirrors
                        ? std::array<u32, 3>{triangle[0], triangle[2], triangle[1]}
                        : std::array<u32, 3>{triangle[0], triangle[1], triangle[2]};
                    for(const auto corner: order)
                        corners.push_back(makeCorner(mesh, corner, toModel, normalToModel));
                }
            }
        }
    }

    // The usual per-triangle tangent averaged per vertex, orthogonalized
    // against the vertex normal; runs after indexing, so vertices split by
    // a UV seam keep frames of their own.
    void generateTangents(std::vector<Vertex>& vertices, const std::vector<u32>& indices) {
        std::vector<Vec3> alongU(vertices.size(), zeros());
        std::vector<Vec3> alongV(vertices.size(), zeros());

        for(usize i = 0; i + 2 < indices.size(); i += 3) {
            const auto i0 = indices[i];
            const auto i1 = indices[i + 1];
            const auto i2 = indices[i + 2];

            const auto edge1 = vertices[i1].position - vertices[i0].position;
            const auto edge2 = vertices[i2].position - vertices[i0].position;
            const auto duv1 = vertices[i1].texCoord - vertices[i0].texCoord;
            const auto duv2 = vertices[i2].texCoord - vertices[i0].texCoord;

            // a triangle whose UVs collapse carries no direction
            const auto det = duv1.x * duv2.y - duv2.x * duv1.y;
            if(det == 0.0f)
                continue;

            const auto r = 1.0f / det;
            const auto tangent = r * (duv2.y * edge1 - duv1.y * edge2);
            const auto bitangent = r * (duv1.x * edge2 - duv2.x * edge1);
            for(const auto index: {i0, i1, i2}) {
                alongU[index] += tangent;
                alongV[index] += bitangent;
            }
        }

        for(usize i = 0; i < vertices.size(); ++i) {
            auto& vertex = vertices[i];
            const auto normal = vertex.normal;

            auto tangent = alongU[i] - normal * dot(normal, alongU[i]);
            if(normSquared(tangent) < 1e-12f) {
                // no usable UV gradient: any axis orthogonal to the normal
                const auto axis = std::abs(normal.x) < 0.9f ? unitX() : unitY();
                tangent = cross(axis, normal);
            }
            tangent = normalize(tangent);

            // MeshData's increasing v is w * cross(normal, tangent)
            const auto handedness =
                dot(cross(normal, tangent), alongV[i]) < 0.0f ? -1.0f : 1.0f;
            vertex.tangent = toVec4(tangent, handedness);
        }
    }

    ModelSlot finishSlot(SlotBuilder&& builder, const std::filesystem::path& path) {
        auto vertices = std::move(builder.corners);
        std::vector<u32> indices(vertices.size());

        ufbx_vertex_stream stream{
            .data = vertices.data(),
            .vertex_count = vertices.size(),
            .vertex_size = sizeof(Vertex)
        };
        ufbx_error error{};
        // compacts the stream in place and keeps the triangle order
        const auto unique = ufbx_generate_indices(
            &stream,
            1,
            indices.data(),
            indices.size(),
            nullptr,
            &error
        );
        if(error.type != UFBX_ERROR_NONE) {
            throw std::runtime_error(std::format(
                "cannot index material '{}' of {}: {}",
                builder.material,
                path,
                toView(error.description)
            ));
        }
        vertices.resize(unique);
        generateTangents(vertices, indices);

        ModelSlot slot{.material = std::move(builder.material)};
        slot.bounds = boundsOf(vertices);
        slot.mesh.vertices = std::move(vertices);
        slot.mesh.indices = std::move(indices);

        return slot;
    }
}

namespace Crowy
{
    ModelData LoadModel(const std::filesystem::path& path) {
        const auto scene = loadScene(path);

        // typed_id indexes scene->materials, so a slot per material in that
        // order, and one more for faces with none
        std::vector<SlotBuilder> builders;
        builders.reserve(scene->materials.count + 1);
        for(const auto* material: scene->materials)
            builders.push_back(SlotBuilder{.material = Str(toView(material->name))});
        builders.push_back(SlotBuilder{});

        for(const auto* node: scene->nodes) {
            if(node->mesh != nullptr)
                addNode(*node, builders);
        }

        ModelData model;
        for(auto& builder: builders) {
            if(builder.corners.empty())
                continue;

            auto slot = finishSlot(std::move(builder), path);
            model.bounds = model.slots.empty()
                ? slot.bounds
                : unionOf(model.bounds, slot.bounds);
            model.slots.push_back(std::move(slot));
        }

        return model;
    }
}
