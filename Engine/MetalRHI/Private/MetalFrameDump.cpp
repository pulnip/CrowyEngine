#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>
#include <Metal/MTLTexture.hpp>
#include "LogLocal.hpp"
#include "MetalFrameDump.hpp"
#include "Primitives.hpp"

namespace Crowy
{
    namespace{
        struct DumpRequest{
            const char* path = nullptr;
            int frameIndex = 60;
        };

        DumpRequest readRequest(){
            DumpRequest request;
            request.path = std::getenv("CROWY_DUMP_FRAME");

            if(const char* at = std::getenv("CROWY_DUMP_FRAME_AT")){
                if(const int parsed = std::atoi(at); parsed > 0)
                    request.frameIndex = parsed;
            }

            return request;
        }

        void writePPM(MTL::Texture& texture, const std::string& path){
            const auto format = texture.pixelFormat();
            const bool bgra =
                format == MTL::PixelFormatBGRA8Unorm ||
                format == MTL::PixelFormatBGRA8Unorm_sRGB;
            const bool rgba =
                format == MTL::PixelFormatRGBA8Unorm ||
                format == MTL::PixelFormatRGBA8Unorm_sRGB;
            if(!bgra && !rgba){
                LOG_WARN(
                    "CROWY_DUMP_FRAME skipped: "
                    "drawable format ({}) is not 8-bit RGBA/BGRA",
                    static_cast<u32>(format)
                );
                return;
            }

            const auto width = texture.width();
            const auto height = texture.height();

            std::vector<u8> pixels(width * height * 4);
            texture.getBytes(
                pixels.data(),
                width * 4,
                MTL::Region::Make2D(0, 0, width, height),
                0
            );

            FILE* file = std::fopen(path.c_str(), "wb");
            if(file == nullptr){
                LOG_WARN(
                    "CROWY_DUMP_FRAME skipped: cannot open '{}'",
                    path
                );
                return;
            }

            std::fprintf(file, "P6\n%zu %zu\n255\n",
                static_cast<usize>(width),
                static_cast<usize>(height)
            );

            const usize red = bgra ? 2 : 0;
            const usize blue = bgra ? 0 : 2;
            for(usize i = 0; i < width * height; ++i){
                const u8 rgb[3] = {
                    pixels[i * 4 + red],
                    pixels[i * 4 + 1],
                    pixels[i * 4 + blue]
                };
                std::fwrite(rgb, 1, sizeof(rgb), file);
            }
            std::fclose(file);

            LOG_INFO(
                "CROWY_DUMP_FRAME: wrote {}x{} frame to '{}'",
                width, height, path
            );
        }
    }

    void DumpFrameIfRequested(
        MTL::CommandBuffer& cmdBuffer,
        CA::MetalDrawable* drawable
    ){
        static const DumpRequest request = readRequest();
        if(request.path == nullptr || drawable == nullptr)
            return;

        static int presentedCount = 0;
        if(++presentedCount != request.frameIndex)
            return;

        auto* texture = drawable->texture();
        if(texture->storageMode() == MTL::StorageModePrivate){
            LOG_WARN("CROWY_DUMP_FRAME skipped: drawable is not CPU-readable");
            return;
        }

        // the handler outlives this call, so it keeps its own references
        texture->retain();
        const std::string path = request.path;
        cmdBuffer.addCompletedHandler(MTL::HandlerFunction(
            [texture, path](MTL::CommandBuffer*){
                writePPM(*texture, path);
                texture->release();
            }
        ));
    }
}
