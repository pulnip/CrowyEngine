#pragma once

#include <memory>
#include <string>
#include <unordered_map>
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
        DECLARE_INTERFACE(Binder);

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
        std::vector<BindError>& errors, const char* key,
        std::optional<bool> def = std::nullopt
    );
    std::optional<double> readFloat(
        const ValueArena& arena, const VTable& table,
        std::vector<BindError>& errors, const char* key,
        std::optional<double> def = std::nullopt
    );
    std::optional<Vec3> readVec3(
        const ValueArena& arena, const VTable& table,
        std::vector<BindError>& errors, const char* key,
        std::optional<Vec3> def = std::nullopt
    );
    std::optional<Vec4> readVec4(
        const ValueArena& arena, const VTable& table,
        std::vector<BindError>& errors, const char* key,
        std::optional<Vec4> def = std::nullopt
    );
    std::optional<std::string> readString(
        const ValueArena& arena, const VTable& table,
        std::vector<BindError>& errors, const char* key,
        std::optional<std::string> def = std::nullopt
    );
    std::optional<std::vector<std::string>> readStringArray(
        const ValueArena& arena, const VTable& table,
        std::vector<BindError>& errors, const char* key
    );
}