#pragma once

#include "concepts.hpp"
#include "math.hpp"
#include "ECSDefinitions.hpp"
#include "ResourceDefinitions.hpp"

namespace Crowy
{
    struct TransformComponent{
        Vec3 position = zeros();
        Vec4 rotation = unitQuat();
        Vec3 scale = ones();
    };

    struct CameraComponent{
        CameraType type;
        float fov;
        float nearPlane;
        float farPlane;
        Projection proj;
    };

    struct ColorComponent{
        Vec4 color;
    };

    struct RenderObjectComponent{
        float alpha;
        MeshHandle mesh;
        MaterialSetHandle materialSet;
        ShaderHandle shader;
    };
    struct RigidbodyComponent{
        Vec3 velocity;
        bool useGravity;
        float mass;
    };
    struct SphereColliderComponent{
        Vec3 position;
        float radius;

        // physical material
        float bounciness;
        float friction;
    };
    struct BoxColliderComponent{
        Vec3 position = zeros();
        Vec4 rotation = unitQuat();
        Vec3 scale = ones();

        // physical material
        float bounciness;
        float friction;
    };

    // Entity Type? Property? Tags (kept in Long-term)
    struct PlayerComponent{};
    struct EditorComponent{};
    struct AttachableComponent{};

    // Entity-to-Entity Event Tags
    struct ImpulseComponent{
        Vec3 force;
        float dt;
    };

    #define ARCHETYPE_PAIRS \
        X(     TransformComponent,      TRANSFORM) \
        X(        CameraComponent,         CAMERA) \
        X(         ColorComponent,          COLOR) \
        X(  RenderObjectComponent,  RENDER_OBJECT) \
        X(     RigidbodyComponent,      RIGIDBODY) \
        X(SphereColliderComponent, SPHERECOLLIDER) \
        X(   BoxColliderComponent,    BOXCOLLIDER) \
        X(       ImpulseComponent,        IMPULSE) \
        X(        PlayerComponent,         PLAYER) \
        X(        EditorComponent,         EDITOR) \
        X(    AttachableComponent,     ATTACHABLE)

    #define X(type, name) static_assert(std::is_trivially_copyable_v<type>);
    ARCHETYPE_PAIRS
    #undef X

    template<typename T>
    consteval bool isBuiltIn(){ return false; }
    #define X(type, _) template<> \
        consteval bool isBuiltIn<type>(){ return true; }
    ARCHETYPE_PAIRS
    #undef X

    enum{
        #define NAME_INDEX(_, name) name##_INDEX,
        #define X NAME_INDEX
        ARCHETYPE_PAIRS
        #undef X
        NUM_ARCHETYPES
    };

    #define X(_, name) constexpr ArchetypeBit \
        name##_BIT = (ArchetypeBit(1) << name##_INDEX);
    ARCHETYPE_PAIRS
    #undef X

    template<typename T>
    consteval ArchetypeBit bit_of(){
        return 0;
    }
    template<typename... Ts>
    consteval ArchetypeBit bits_of(){
        return (... | bit_of<Ts>());
    }
    #define X(type, name) template<> \
        consteval ArchetypeBit bit_of<type>(){ return name##_BIT; }
    ARCHETYPE_PAIRS
    #undef X

    template<value_type T>
    auto bits_of(T)->ArchetypeBit{
        using U = std::remove_cvref_t<T>;

        return bit_of<U>();
    }
    template<value_type T1, all_value... TN>
    auto bits_of(T1, TN... tn){
        using U = std::remove_cvref_t<T1>;

        return bit_of<U>() + bits_of(tn...);
    }

    template<pointer_type T>
    auto bits_of(T t){
        using U = std::remove_pointer_t<std::remove_cvref_t<T>>;

        return t != nullptr ? bit_of<U>() : 0;
    }
    template<pointer_type T1, all_pointer... TN>
    auto bits_of(T1 t1, TN... tn){
        using U = std::remove_pointer_t<std::remove_cvref_t<T1>>;
        auto bit = bits_of(tn...);

        return bit + (t1 != nullptr ? bit_of<U>() : 0);
    }

    template<optional_type T>
    auto bits_of(const T& t){
        using U = remove_optional_t<std::remove_cvref_t<T>>;

        return t.has_value() ? bit_of<U>() : 0;
    }
    template<optional_type T1, all_optional... TN>
    auto bits_of(const T1& t1, const TN&... tn){
        using U = remove_optional_t<std::remove_cvref_t<T1>>;
        auto bit = bits_of(tn...);

        return bit + (t1.has_value() ? bit_of<U>() : 0);
    }

    constexpr size_t size_of(ArchetypeBit bit){
        size_t size = sizeof(EntityID);
        #define X(type, name) \
            if(bit & name##_BIT) \
                size += sizeof(type);
        ARCHETYPE_PAIRS
        #undef X
        return size;
    }

    template<typename T>
    constexpr size_t offset_of(ArchetypeBit bit){
        if(!isSubset(bit_of<T>(), bit))
            return -1;

        size_t offset = sizeof(EntityID);
        #define X(type, name) \
            if(std::same_as<T, type>) \
                return offset; \
            if(bit & bit_of<type>()) \
                offset += sizeof(type);
        ARCHETYPE_PAIRS
        #undef X
        return offset;
    }

    constexpr std::string name_of(ArchetypeBit bit){
        switch(bit) {
        #define X(type, name) \
        case name##_BIT: \
            return #type;
        ARCHETYPE_PAIRS
        #undef X
        default:
            return "Unnamed";
        }
    }

    template<typename T>
    constexpr std::string name_of(){
        return name_of(bit_of<T>());
    }
}
