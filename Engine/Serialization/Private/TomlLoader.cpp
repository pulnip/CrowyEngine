#include <toml++/toml.hpp>
#include "StringUtil.hpp"
#include "TomlLoader.hpp"

namespace Crowy
{
    DOM::Value parseFromNode(const toml::node& n){
        using namespace DOM;

        return n.visit([](auto&& v) -> Value {
            using T = std::decay_t<decltype(v)>;

            if constexpr(toml::is_boolean<T>){
                return Value(v.get());
            }
            else if constexpr(toml::is_integer<T>){
                return Value(v.get());
            }
            else if constexpr(toml::is_floating_point<T>){
                return Value(v.get());
            }
            else if constexpr(toml::is_string<T>){
                return Value(v.get());
            }
            else if constexpr(toml::is_array<T>){
                Array arr;
                arr.reserve(v.size());

                for(auto&& child: v){
                    arr.emplace_back(parseFromNode(child));
                }

                return Value(std::move(arr));
            }
            else if constexpr(toml::is_table<T>){
                Table tbl;
                for(auto&& [k, child]: v){
                    tbl.emplace(Str(k.str()), parseFromNode(child));
                }

                return Value(std::move(tbl));
            }

            return Value();
        });
    }

    DOM::Value parseTomlString(StrView str){
        auto pr = toml::parse(str);

        return parseFromNode(pr);
    }

    DOM::Value parseTomlFile(const std::filesystem::path& path){
        auto utf8Path = toUTF8String(path);
        auto pr = toml::parse_file(utf8Path);

        return parseFromNode(pr);
    }
}
