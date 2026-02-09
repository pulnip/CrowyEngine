#pragma once

#include <memory>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>
#include "math.hpp"
#include "semantics.hpp"
#include "SourceLocation.hpp"
#include "ParseResult.hpp"

namespace Crowy
{
    // bind/freeze plan
    struct BindError{
        std::string msg;
        SourceLocation location;
    };

    // element binder interface
    template<typename BindPlan>
    class Binder{
    public:
        CROWY_DECLARE_INTERFACE(Binder);

        virtual void validateAndPlan(const ValueArena&,
            const VTable&, size_t elmIndex, BindPlan&)=0;

        virtual void validateAndPlanArray(const ValueArena& arena,
            const VArray& array, size_t elmIndex, BindPlan& plan
        ){
            for(size_t i: array.elements){
                if(auto table = std::get_if<VTable>(&arena.nodes[i]))
                    validateAndPlan(arena, *table, elmIndex, plan);
            }
        }
    };

    template<typename BindPlan>
    using BinderRegistry = std::unordered_map<
        std::string,
        std::unique_ptr<Binder<BindPlan>>
    >;

    const VNode* findField(const ValueArena& a,
        const VTable& table, const char* key
    );
    SourceLocation getLoc(const VNode& n);

    std::optional<bool> readBool(
        const ValueArena& arena, const VTable& table,
        std::vector<BindError>& errors, const char* key
    );
    bool readBool(
        const ValueArena& arena, const VTable& table,
        std::vector<BindError>& errors, const char* key,
        bool def
    );
    std::optional<double> readFloat(
        const ValueArena& arena, const VTable& table,
        std::vector<BindError>& errors, const char* key
    );
    double readFloat(
        const ValueArena& arena, const VTable& table,
        std::vector<BindError>& errors, const char* key,
        double def
    );
    std::optional<Vec2> readVec2(
        const ValueArena& arena, const VTable& table,
        std::vector<BindError>& errors, const char* key
    );
    Vec2 readVec2(
        const ValueArena& arena, const VTable& table,
        std::vector<BindError>& errors, const char* key,
        Vec2 def
    );
    std::optional<Vec3> readVec3(
        const ValueArena& arena, const VTable& table,
        std::vector<BindError>& errors, const char* key
    );
    Vec3 readVec3(
        const ValueArena& arena, const VTable& table,
        std::vector<BindError>& errors, const char* key,
        Vec3 def
    );
    std::optional<Vec4> readVec4(
        const ValueArena& arena, const VTable& table,
        std::vector<BindError>& errors, const char* key
    );
    Vec4 readVec4(
        const ValueArena& arena, const VTable& table,
        std::vector<BindError>& errors, const char* key,
        Vec4 def
    );
    std::optional<Mat4> readMat4(
        const ValueArena& arena, const VTable& table,
        std::vector<BindError>& errors, const char* key
    );
    Mat4 readMat4(
        const ValueArena& arena, const VTable& table,
        std::vector<BindError>& errors, const char* key,
        Mat4 def
    );
    std::optional<std::string> readString(
        const ValueArena& arena, const VTable& table,
        std::vector<BindError>& errors, const char* key
    );
    std::string readString(
        const ValueArena& arena, const VTable& table,
        std::vector<BindError>& errors, const char* key,
        std::string def
    );
    std::optional<std::vector<std::string>> readStringArray(
        const ValueArena& arena, const VTable& table,
        std::vector<BindError>& errors, const char* key
    );
}