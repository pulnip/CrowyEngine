#include "FileUtil.hpp"

#include <format>
#include <fstream>
#include <stdexcept>

#include "PathUtil.hpp"

namespace Crowy
{
    namespace
    {
        template<typename Buffer>
        Buffer readWhole(const std::filesystem::path& path) {
            std::ifstream file(path, std::ios::binary | std::ios::ate);
            if(!file.is_open()) {
                throw std::runtime_error(
                    std::format("Failed to open file: {}", path)
                );
            }

            const auto size = file.tellg();
            file.seekg(0);

            Buffer buffer(static_cast<usize>(size), {});
            file.read(reinterpret_cast<char*>(buffer.data()), size);

            return buffer;
        }
    }

    Str readFileAsString(const std::filesystem::path& path) {
        return readWhole<Str>(path);
    }

    std::vector<u8> readFileAsBinary(const std::filesystem::path& path) {
        return readWhole<std::vector<u8>>(path);
    }
}
