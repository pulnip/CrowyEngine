#include <charconv>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>
#include "path_util.hpp"
#include "AppConfig.hpp"

namespace Crowy
{
    template<typename T>
    std::optional<T> arg_cast(std::string_view arg) = delete;

    template<> std::optional<std::string_view>
    arg_cast<std::string_view>(std::string_view arg){
        return arg;
    }

    template<> std::optional<std::filesystem::path>
    arg_cast<std::filesystem::path>(std::string_view arg){
        return to_path(arg.data(), arg.size());
    }

    template<> std::optional<int>
    arg_cast<int>(std::string_view arg){
        int value = 0;
        auto [ptr, ec] = std::from_chars(
            arg.data(),
            arg.data() + arg.size(),
            value
        );
        return ec == std::errc{} ?
            std::optional{value} :
            std::nullopt;
    }

    template<> std::optional<bool>
    arg_cast<bool>(std::string_view arg){
        if(arg == "true"  || arg == "1") return true;
        if(arg == "false" || arg == "0") return false;
        return std::nullopt;
    }

    template<> std::optional<float>
    arg_cast<float>(std::string_view arg){
        float value = 0;
        auto [ptr, ec] = std::from_chars(
            arg.data(),
            arg.data() + arg.size(),
            value
        );
        return ec == std::errc{} ?
            std::optional{value} :
            std::nullopt;
    }

    class CommandLineParser{
    private:
        std::unordered_map<std::string, std::string> options;
        std::vector<std::string> positional;

    public:
        void parse(int argc, char* argv[]){
            for(int i=1; i<argc; ++i){
                std::string_view arg = argv[i];

                if(arg.starts_with("--")){
                    // --key=value form
                    if(auto pos = arg.find('='); pos != std::string_view::npos){
                        auto key = std::string(arg.substr(2, pos - 2));
                        auto value = std::string(arg.substr(pos + 1));
                        options[key] = value;
                    }
                    else{
                        // --flag form (boolean)
                        options[std::string(arg.substr(2))] = "true";
                    }
                }
                else if(arg.starts_with("-")){
                    // -key value form
                    auto key = std::string(arg.substr(1));
                    if(i + 1 < argc && argv[i + 1][0] != '-'){
                        options[key] = argv[++i];
                    }
                    else{
                        options[key] = "true";
                    }
                }
                else{
                    positional.emplace_back(arg);
                }
            }
        }

        bool has(std::string_view key) const{
            return options.contains(std::string(key));
        }

        template<typename T=std::string_view>
        std::optional<T> get(std::string_view key) const{
            if(auto it=options.find(std::string(key)); it!=options.end())
                return arg_cast<T>(it->second);
            return std::nullopt;
        }
        template<typename T=std::string_view>
        T get(std::string_view key, const T& defaultVal) const{
            return get<T>(key).value_or(defaultVal);
        }
    };

    AppConfig parseCommandLine(int argc, char* argv[]){
        CommandLineParser parser;
        parser.parse(argc, argv);

        auto width  = parser.get( "width", 800);
        auto height = parser.get("height", 600);
        auto fullscreen    = parser.get("fullscreen", false);
        auto resizable     = parser.get( "resizable",  true);
        auto borderless    = parser.get("borderless", false);
        auto always_on_top = parser.get("always_on_top", false);

        auto  sceneFile = parser.get<std::filesystem::path>( "scene", "");
        auto renderFile = parser.get<std::filesystem::path>("render", "");
        auto  inputFIle = parser.get<std::filesystem::path>( "input", "");

        return AppConfig{
            .window = WindowConfig{
                .title = "Crowy",
                .width  = width,
                .height = height,
                .fullscreen    = fullscreen,
                .resizable     = resizable,
                .borderless    = borderless,
                .always_on_top = always_on_top
            },
            .sceneFile  = sceneFile,
            .renderFile = renderFile
        };
    }
}