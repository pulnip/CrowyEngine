#include "JsonLoader.hpp"

#include <fstream>
#include <limits>
#include <stdexcept>

#include <nlohmann/json.hpp>

#include "StringUtil.hpp"

namespace
{
    using namespace Crowy;

    // JSON 1 lands as i64 and 1.0 as f64; an f64 never binds an integer
    // property, so a wrong literal fails loudly instead of converting.
    DOM::Value parseFromJson(const nlohmann::json& j){
        using namespace DOM;

        switch(j.type()){
        case nlohmann::json::value_t::boolean:
            return Value(j.get<bool>());
        case nlohmann::json::value_t::number_unsigned: {
            auto u = j.get<std::uint64_t>();
            if(u > static_cast<std::uint64_t>(std::numeric_limits<i64>::max())){
                throw std::out_of_range("JSON integer exceeds i64 range");
            }
            return Value(static_cast<i64>(u));
        }
        case nlohmann::json::value_t::number_integer:
            return Value(j.get<i64>());
        case nlohmann::json::value_t::number_float:
            return Value(j.get<f64>());
        case nlohmann::json::value_t::string:
            return Value(j.get<Str>());
        case nlohmann::json::value_t::array: {
            Array arr;
            arr.reserve(j.size());

            for(auto& child: j){
                arr.emplace_back(parseFromJson(child));
            }

            return Value(std::move(arr));
        }
        case nlohmann::json::value_t::object: {
            Table tbl;
            for(auto& [k, child]: j.items()){
                tbl.emplace(Str(k), parseFromJson(child));
            }

            return Value(std::move(tbl));
        }
        default:
            return Value();
        }
    }

    nlohmann::json emitFromValue(const DOM::Value& v){
        using enum DOM::Value::Kind;

        switch(v.kind()){
        case Bool:
            return *v.asBool();
        case Int:
            return *v.asInt();
        case Float:
            return *v.asFloat();
        case String:
            return *v.asString();
        case Array: {
            auto arr = nlohmann::json::array();
            for(auto& child: *v.asArray()){
                arr.emplace_back(emitFromValue(child));
            }
            return arr;
        }
        case Table: {
            auto obj = nlohmann::json::object();
            for(auto& [k, child]: *v.asTable()){
                obj[k] = emitFromValue(child);
            }
            return obj;
        }
        case None:
            break;
        }
        return nullptr;
    }
}

namespace Crowy
{
    DOM::Value parseJsonString(StrView str){
        auto j = nlohmann::json::parse(str);

        return parseFromJson(j);
    }

    DOM::Value parseJsonFile(const std::filesystem::path& path){
        std::ifstream stream(path);
        if(!stream){
            throw std::runtime_error("cannot open JSON file: " + toUTF8String(path));
        }

        auto j = nlohmann::json::parse(stream);

        return parseFromJson(j);
    }

    Str emitJson(const DOM::Value& v, JsonStyle style){
        auto j = emitFromValue(v);

        return style == JsonStyle::Pretty ? j.dump(4) : j.dump();
    }
}
