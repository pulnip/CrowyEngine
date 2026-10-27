#include "JsonLoader.hpp"

#include <charconv>
#include <cmath>
#include <format>
#include <fstream>
#include <iterator>
#include <limits>
#include <stdexcept>

#include <nlohmann/json.hpp>

#include "PathUtil.hpp"

namespace
{
    using namespace Crowy;

    // keeps an object's keys in the file's order
    using Json = nlohmann::ordered_json;

    // JSON 1 lands as i64 and 1.0 as f64; an f64 never binds an integer
    // property, so a wrong literal fails loudly instead of converting.
    DOM::Value parseFromJson(const Json& j) {
        using namespace DOM;

        switch(j.type()) {
        case Json::value_t::boolean:
            return Value(j.get<bool>());
        case Json::value_t::number_unsigned: {
            auto u = j.get<std::uint64_t>();
            if(u >
               static_cast<std::uint64_t>(std::numeric_limits<i64>::max())) {
                throw std::out_of_range("JSON integer exceeds i64 range");
            }
            return Value(static_cast<i64>(u));
        }
        case Json::value_t::number_integer:
            return Value(j.get<i64>());
        case Json::value_t::number_float:
            return Value(j.get<f64>());
        case Json::value_t::string:
            return Value(j.get<Str>());
        case Json::value_t::array: {
            Array arr;
            arr.reserve(j.size());

            for(auto& child: j) {
                arr.emplace_back(parseFromJson(child));
            }

            return Value(std::move(arr));
        }
        case Json::value_t::object: {
            Table tbl;
            for(auto& [k, child]: j.items()) {
                tbl.emplace(Str(k), parseFromJson(child));
            }

            return Value(std::move(tbl));
        }
        default:
            return Value();
        }
    }

    // whether the bytes are well-formed UTF-8, which a JSON string must be
    bool isUtf8(StrView text) noexcept {
        for(usize i = 0; i < text.size();) {
            const auto lead = static_cast<u8>(text[i]);
            const usize length = lead < 0x80             ? 1
                                 : (lead & 0xE0) == 0xC0 ? 2
                                 : (lead & 0xF0) == 0xE0 ? 3
                                 : (lead & 0xF8) == 0xF0 ? 4
                                                         : 0;
            if(length == 0 || i + length > text.size())
                return false;

            for(usize k = 1; k < length; ++k) {
                if((static_cast<u8>(text[i + k]) & 0xC0) != 0x80)
                    return false;
            }
            i += length;
        }

        return true;
    }

    void writeString(Str& out, StrView text) {
        if(!isUtf8(text))
            throw std::invalid_argument("a JSON string must be UTF-8");

        out += '"';
        for(const char c: text) {
            switch(c) {
            case '"':
                out += "\\\"";
                break;
            case '\\':
                out += "\\\\";
                break;
            case '\b':
                out += "\\b";
                break;
            case '\f':
                out += "\\f";
                break;
            case '\n':
                out += "\\n";
                break;
            case '\r':
                out += "\\r";
                break;
            case '\t':
                out += "\\t";
                break;
            default:
                if(static_cast<u8>(c) < 0x20)
                    out += std::format("\\u{:04x}", static_cast<u32>(c));
                else
                    out += c;
            }
        }
        out += '"';
    }

    void writeNumberText(Str& out, StrView text) {
        out += text;
        // without a point or an exponent it would read back as an integer
        if(text.find_first_of(".e") == StrView::npos)
            out += ".0";
    }

    // the shortest text that reads back as the same value
    template<typename T>
        requires std::floating_point<T>
    void writeFloat(Str& out, T value) {
        if(!std::isfinite(value)) {
            out += "null";

            return;
        }

        char buffer[32];
        const auto end =
            std::to_chars(std::begin(buffer), std::end(buffer), value).ptr;
        writeNumberText(out, StrView(std::begin(buffer), end));
    }

    // a reader parses through an f64, which can round the shortest f32 text
    // onto the tie between two f32; the f64 text of the same value cannot
    void writeSingle(Str& out, f32 value) {
        if(!std::isfinite(value)) {
            out += "null";

            return;
        }

        char buffer[32];
        auto end =
            std::to_chars(std::begin(buffer), std::end(buffer), value).ptr;
        auto back = 0.0;
        std::from_chars(std::begin(buffer), end, back);
        if(static_cast<f32>(back) != value) {
            const auto wide = static_cast<f64>(value);
            end = std::to_chars(std::begin(buffer), std::end(buffer), wide).ptr;
        }
        writeNumberText(out, StrView(std::begin(buffer), end));
    }

    // a pretty line break, indented four spaces a level
    void breakLine(Str& out, JsonStyle style, u32 depth) {
        if(style == JsonStyle::Pretty) {
            out += '\n';
            out.append(4 * depth, ' ');
        }
    }

    void writeValue(Str& out, const DOM::Value& v, JsonStyle style, u32 depth) {
        using enum DOM::Value::Kind;

        switch(v.kind()) {
        case None:
            out += "null";
            break;
        case Bool:
            out += *v.asBool() ? "true" : "false";
            break;
        case Int: {
            char buffer[24];
            const auto end =
                std::to_chars(std::begin(buffer), std::end(buffer), *v.asInt())
                    .ptr;
            out.append(std::begin(buffer), end);
            break;
        }
        case Float:
            if(const auto single = v.asSingle())
                writeSingle(out, *single);
            else
                writeFloat(out, *v.asFloat());
            break;
        case String:
            writeString(out, *v.asString());
            break;
        case Array: {
            const auto& array = *v.asArray();
            out += '[';
            for(usize i = 0; i < array.size(); ++i) {
                if(i > 0)
                    out += ',';
                breakLine(out, style, depth + 1);
                writeValue(out, array[i], style, depth + 1);
            }
            if(!array.empty())
                breakLine(out, style, depth);
            out += ']';
            break;
        }
        case Table: {
            const auto& table = *v.asTable();
            out += '{';
            bool first = true;
            for(const auto& [key, child]: table) {
                if(!first)
                    out += ',';
                first = false;
                breakLine(out, style, depth + 1);
                writeString(out, key);
                out += style == JsonStyle::Pretty ? ": " : ":";
                writeValue(out, child, style, depth + 1);
            }
            if(!table.empty())
                breakLine(out, style, depth);
            out += '}';
            break;
        }
        }
    }
}

namespace Crowy
{
    DOM::Value parseJsonString(StrView str) {
        auto j = Json::parse(str);

        return parseFromJson(j);
    }

    DOM::Value parseJsonFile(const std::filesystem::path& path) {
        std::ifstream stream(path);
        if(!stream) {
            throw std::runtime_error(
                "cannot open JSON file: " + toUTF8String(path)
            );
        }

        auto j = Json::parse(stream);

        return parseFromJson(j);
    }

    Str emitJson(const DOM::Value& v, JsonStyle style) {
        Str out;
        writeValue(out, v, style, 0);

        return out;
    }
}
