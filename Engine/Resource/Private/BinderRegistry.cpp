#include <format>
#include <optional>
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
    using Vec = std::conditional_t<N==2, Vec2,
        std::conditional_t<N==3, Vec3, Vec4>
    >;

    template<unsigned N>
    static std::optional<Vec<N>> readVec(
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

            Vec<N> v;
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

    std::optional<Vec2> readVec2(
        const ValueArena& arena, const VTable& table,
        std::vector<BindError>& errors, const char* key
    ){
        return readVec<2>(arena, table, errors, key);
    }

    Vec2 readVec2(
        const ValueArena& arena, const VTable& table,
        std::vector<BindError>& errors, const char* key,
        Vec2 def
    ){
        auto v = readVec2(arena, table, errors, key);

        if(v.has_value())
            return *v;
        return def;
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

    std::optional<Mat4> readMat4(
        const ValueArena& arena, const VTable& table,
        std::vector<BindError>& errors, const char* key
    ){
        const VNode* n = findField(arena, table, key);
        if(!n)
            return std::nullopt;

        if(auto arr = std::get_if<VArray>(n)){
            if(arr->elements.size() != 4){
                errors.push_back({
                    std::format("{} should be {}", key, 4),
                    arr->location
                });
                return std::nullopt;
            }

            Mat4 v;
            for(int i=0; i<4; ++i){
                const VNode* elem = &arena.nodes[arr->elements[i]];

                if(auto varr = std::get_if<VArray>(elem)){
                    if(varr->elements.size() != 4){
                        // TODO. report error
                        return std::nullopt;
                    }

                    for(int j=0; j<4; ++j){
                        const VNode& velem = arena.nodes[varr->elements[j]];
                        auto f = asFloat(velem);
                        if(!f){
                            // TODO. report error
                            return std::nullopt;
                        }
                        v[i][j] = *f;
                    }
                }
            }

            return v;
        }

        errors.push_back({
            std::format("{} should be array", key),
            getLoc(*n)
        });
        return std::nullopt;
    }

    Mat4 readMat4(
        const ValueArena& arena, const VTable& table,
        std::vector<BindError>& errors, const char* key,
        Mat4 def
    ){
        auto v = readMat4(arena, table, errors, key);

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

    std::vector<std::string> readStringArray(
        const ValueArena& arena, const VTable& table,
        std::vector<BindError>& errors, const char* key,
        std::vector<std::string> def
    ){
        auto v = readStringArray(arena, table, errors, key);

        if(v.has_value())
            return *v;
        return def;
    }

    std::optional<std::filesystem::path> readPath(
        const ValueArena& arena, const VTable& table,
        std::vector<BindError>& errors, const char* key
    ){
        auto stropt = readString(arena, table, errors, key);
        if(!stropt.has_value())
            return std::nullopt;

        return stropt.value();
    }

    std::filesystem::path readPath(
        const ValueArena& arena, const VTable& table,
        std::vector<BindError>& errors, const char* key,
        std::filesystem::path def
    ){
        auto v = readPath(arena, table, errors, key);

        if(v.has_value())
            return *v;
        return def;
    }

    std::optional<std::vector<std::filesystem::path>> readPathArray(
        const ValueArena& arena, const VTable& table,
        std::vector<BindError>& errors, const char* key
    ){
        const VNode* n = findField(arena, table, key);
        if(!n)
            return std::nullopt;

        if(auto arr = std::get_if<VArray>(n)){
            std::vector<std::filesystem::path> v;
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

    std::vector<std::filesystem::path> readPathArray(
        const ValueArena& arena, const VTable& table,
        std::vector<BindError>& errors, const char* key,
        std::vector<std::filesystem::path> def
    ){
        auto v = readPathArray(arena, table, errors, key);

        if(v.has_value())
            return *v;
        return def;
    }
}