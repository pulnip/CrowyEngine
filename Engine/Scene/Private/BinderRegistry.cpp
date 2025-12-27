#include <format>
#include <memory>
#include "BinderRegistry.hpp"

namespace Crowy
{
    const VNode* findField(const ValueArena& a,
        const VTable& table, const char* key
    ){
        auto it = table.fields.find(key);
        if(it == table.fields.end())
            return nullptr;
    
        return &a.nodes[it->second];
    }

    static std::optional<float> asFloat(const VNode& n){
        if(auto f = std::get_if<VFloat>(&n))
            return static_cast<float>(f->v);
        if(auto i = std::get_if<VInt>(&n))  
            return static_cast<float>(i->v);
        return std::nullopt;
    }

    static std::optional<std::string> asString(const VNode& n){
        if(auto s = std::get_if<VString>(&n)){
            return s->v;
        }
        return std::nullopt;
    }

    SourceLocation getLoc(const VNode& n){
        return std::visit(
            [](auto const& x) -> SourceLocation {
                return x.location;
            }, n);
    }

    std::optional<bool> readBool(
        const ValueArena& arena, const VTable& table,
        std::vector<BindError>& errors, const char* key,
        std::optional<bool> def
    ){
        const VNode* n = findField(arena, table, key);
        if(!n)
            return def;

        if(auto bl = std::get_if<VBool>(n))
            return bl->v;

        errors.push_back({
            std::format("{} should be boolean", key),
            getLoc(*n)
        });
        return std::nullopt;
    }

    std::optional<double> readFloat(
        const ValueArena& arena, const VTable& table,
        std::vector<BindError>& errors, const char* key,
        std::optional<double> def
    ){
        const VNode* n = findField(arena, table, key);
        if(!n)
            return def;

        if(auto flt = std::get_if<VFloat>(n))
            return flt->v;
        else if(auto num = std::get_if<VInt>(n))
            return num->v;

        errors.push_back({
            std::format("{} should be number", key),
            getLoc(*n)
        });
        return std::nullopt;
    }

    template<unsigned N>
    static std::optional<std::conditional_t<N==3, Vec3, Vec4>> readVec(
        const ValueArena& arena, const VTable& table,
        std::vector<BindError>& errors, const char* key,
        std::optional<std::conditional_t<N==3, Vec3, Vec4>> def = std::nullopt
    ){
        const VNode* n = findField(arena, table, key);
        if(!n)
            return def;

        if(auto arr = std::get_if<VArray>(n)){
            if(arr->elements.size() != N){
                errors.push_back({
                    std::format("{} should be {}", key, N),
                    arr->location
                });
                return std::nullopt;
            }

            std::conditional_t<N==3, Vec3, Vec4> v;
            for(int i=0; i<N; ++i){
                const VNode& elem = arena.nodes[arr->elements[i]];
                auto f = asFloat(elem);
                if(!f){
                    errors.push_back({
                        "element of Vec should be number",
                        getLoc(elem)
                    });
                    return std::nullopt;
                }
                v[i] = *f;
            }

            return v;
        }

        errors.push_back({
            std::format("{} should be array", key),
            getLoc(*n)
        });
        return std::nullopt;
    }

    std::optional<Vec3> readVec3(
        const ValueArena& arena, const VTable& table,
        std::vector<BindError>& errors, const char* key,
        std::optional<Vec3> def
    ){
        return readVec<3>(arena, table, errors, key, def);
    }

    std::optional<Vec4> readVec4(
        const ValueArena& arena, const VTable& table,
        std::vector<BindError>& errors, const char* key,
        std::optional<Vec4> def
    ){
        return readVec<4>(arena, table, errors, key, def);
    }

    std::optional<std::string> readString(
        const ValueArena& arena, const VTable& table,
        std::vector<BindError>& errors, const char* key,
        std::optional<std::string> def
    ){
        const VNode* n = findField(arena, table, key);
        if(!n)
            return def;

        if(auto str = std::get_if<VString>(n))
            return str->v;

        errors.push_back({
            std::format("{} should be string", key),
            getLoc(*n)
        });
        return std::nullopt;
    }

    std::optional<std::vector<std::string>> readStringArray(
        const ValueArena& arena, const VTable& table,
        std::vector<BindError>& errors, const char* key
    ){
        const VNode* n = findField(arena, table, key);
        if(!n)
            return std::nullopt;

        if(auto arr = std::get_if<VArray>(n)){
            std::vector<std::string> v;

            for(int i=0; i<arr->elements.size(); ++i){
                const VNode& elem = arena.nodes[arr->elements[i]];
                auto f = asString(elem);
                if(!f){
                    errors.push_back({
                        "element of StringArray should be String",
                        getLoc(elem)
                    });
                    return std::nullopt;
                }
                v[i] = *f;
            }

            return v;
        }

        errors.push_back({
            std::format("{} should be array", key),
            getLoc(*n)
        });
        return std::nullopt;
    }
}