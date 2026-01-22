#include <format>
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
        std::vector<BindError>& errors, const char* key
    ){
        const VNode* n = findField(arena, table, key);
        if(!n)
            return std::nullopt;

        if(auto bl = std::get_if<VBool>(n))
            return bl->v;

        errors.push_back({
            std::format("{} should be boolean", key),
            getLoc(*n)
        });
        return std::nullopt;
    }

    bool readBool(
        const ValueArena& arena, const VTable& table,
        std::vector<BindError>& errors, const char* key,
        bool def
    ){
        auto v = readBool(arena, table, errors, key);

        if(v.has_value())
            return *v;
        return def;
    }

    std::optional<double> readFloat(
        const ValueArena& arena, const VTable& table,
        std::vector<BindError>& errors, const char* key
    ){
        const VNode* n = findField(arena, table, key);
        if(!n)
            return std::nullopt;

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

    double readFloat(
        const ValueArena& arena, const VTable& table,
        std::vector<BindError>& errors, const char* key,
        double def
    ){
        auto v = readFloat(arena, table, errors, key);

        if(v.has_value())
            return *v;
        return def;
    }

    template<unsigned N>
    static std::optional<std::conditional_t<N==3, Vec3, Vec4>> readVec(
        const ValueArena& arena, const VTable& table,
        std::vector<BindError>& errors, const char* key
    ){
        const VNode* n = findField(arena, table, key);
        if(!n)
            return std::nullopt;

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
        std::vector<BindError>& errors, const char* key
    ){
        return readVec<3>(arena, table, errors, key);
    }

    Vec3 readVec3(
        const ValueArena& arena, const VTable& table,
        std::vector<BindError>& errors, const char* key,
        Vec3 def
    ){
        auto v = readVec3(arena, table, errors, key);

        if(v.has_value())
            return *v;
        return def;
    }

    std::optional<Vec4> readVec4(
        const ValueArena& arena, const VTable& table,
        std::vector<BindError>& errors, const char* key
    ){
        return readVec<4>(arena, table, errors, key);
    }

    Vec4 readVec4(
        const ValueArena& arena, const VTable& table,
        std::vector<BindError>& errors, const char* key,
        Vec4 def
    ){
        auto v = readVec4(arena, table, errors, key);

        if(v.has_value())
            return *v;
        return def;
    }

    std::optional<std::string> readString(
        const ValueArena& arena, const VTable& table,
        std::vector<BindError>& errors, const char* key
    ){
        const VNode* n = findField(arena, table, key);
        if(!n)
            return std::nullopt;

        if(auto str = std::get_if<VString>(n))
            return str->v;

        errors.push_back({
            std::format("{} should be string", key),
            getLoc(*n)
        });
        return std::nullopt;
    }

    std::string readString(
        const ValueArena& arena, const VTable& table,
        std::vector<BindError>& errors, const char* key,
        std::string def
    ){
        auto v = readString(arena, table, errors, key);

        if(v.has_value())
            return *v;
        return def;
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
            v.resize(arr->elements.size());

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