#include "MetalFrameDump.hpp"

#include <exception>
#include <utility>
#include <vector>

#include <Metal/MTLTexture.hpp>

#include "Assert.hpp"
#include "ImageWriter.hpp"
#include "LogLocal.hpp"
#include "PathUtil.hpp"
#include "Primitives.hpp"

namespace Crowy
{
    namespace
    {
        bool writeFrame(MTL::Texture& texture, const Str& path) {
            const auto format = texture.pixelFormat();
            const bool bgra = format == MTL::PixelFormatBGRA8Unorm ||
                              format == MTL::PixelFormatBGRA8Unorm_sRGB;
            const bool rgba = format == MTL::PixelFormatRGBA8Unorm ||
                              format == MTL::PixelFormatRGBA8Unorm_sRGB;
            if(!bgra && !rgba) {
                LOG_WARN(
                    "frame dump skipped: "
                    "drawable format ({}) is not 8-bit RGBA/BGRA",
                    static_cast<u32>(format)
                );
                return false;
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

            try {
                writeImage(
                    toPath(path),
                    Rgba8View{
                        .pixels = pixels.data(),
                        .width = static_cast<u32>(width),
                        .height = static_cast<u32>(height),
                        .rowPitch = width * 4
                    },
                    bgra ? ChannelOrder::Bgra : ChannelOrder::Rgba
                );
            }
            catch(const std::exception& e) {
                LOG_WARN("frame dump skipped: {}", e.what());
                return false;
            }

            LOG_INFO(
                "frame dump: wrote {}x{} frame to '{}'",
                width,
                height,
                path
            );
            return true;
        }
    }

    void DumpFrame(
        MTL::CommandBuffer& cmdBuffer,
        CA::MetalDrawable& drawable,
        Str path,
        std::function<void(bool)> onDone
    ) {
        auto* texture = drawable.texture();
        CROWY_ASSERT(
            !texture->framebufferOnly(),
            "a dumped drawable must not be framebufferOnly"
        );
        if(texture->storageMode() == MTL::StorageModePrivate) {
            LOG_WARN("frame dump skipped: drawable is not CPU-readable");
            onDone(false);
            return;
        }

        // the handler outlives this call, so it keeps its own references
        texture->retain();
        cmdBuffer.addCompletedHandler(
            MTL::HandlerFunction([texture,
                                  path = std::move(path),
                                  onDone =
                                      std::move(onDone)](MTL::CommandBuffer*) {
                onDone(writeFrame(*texture, path));
                texture->release();
            })
        );
    }
}
