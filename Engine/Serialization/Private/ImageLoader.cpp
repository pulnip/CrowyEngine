#include "RHIDefinitions.hpp"
#include <cstring>
#include <format>
#include <stdexcept>
#include <utility>
#include <ktx.h>
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#include "ImageLoader.hpp"
#include "PtrUtil.hpp"
#include "StringUtil.hpp"

#define CHECK_KTX_ERROR(expr, msg) \
    do{ \
        if(const auto err = (expr); err != KTX_SUCCESS) [[unlikely]]{ \
            throw std::runtime_error(std::format( \
                "{}: {}", msg, ktxErrorString(err) \
            )); \
        } \
    } while(false)

namespace{
    Crowy::RHIPixelFormat convert(ktx_uint32_t format){
        using enum Crowy::RHIPixelFormat;

    switch(format){
        // Uncompressed
        case 37:  return RGBA8_UNORM;       // VK_FORMAT_R8G8B8A8_UNORM
        case 43:  return RGBA8_UNORM_SRGB;  // VK_FORMAT_R8G8B8A8_SRGB
        case 44:  return BGRA8_UNORM;       // VK_FORMAT_B8G8R8A8_UNORM
        case 50:  return BGRA8_UNORM_SRGB;  // VK_FORMAT_B8G8R8A8_SRGB
        case 9:   return R8_UNORM;          // VK_FORMAT_R8_UNORM
        case 16:  return RG8_UNORM;         // VK_FORMAT_R8G8_UNORM
        // BCn
        case 131: return BC1_UNORM;         // BC1_RGB_UNORM_BLOCK
        case 132: return BC1_UNORM_SRGB;    // BC1_RGB_SRGB_BLOCK
        case 133: return BC1_UNORM;         // BC1_RGBA_UNORM_BLOCK
        case 134: return BC1_UNORM_SRGB;    // BC1_RGBA_SRGB_BLOCK
        case 137: return BC3_UNORM;         // BC3_UNORM_BLOCK
        case 138: return BC3_UNORM_SRGB;    // BC3_SRGB_BLOCK
        case 139: return BC4_UNORM;         // BC4_UNORM_BLOCK
        case 140: return BC4_SNORM;         // BC4_SNORM_BLOCK
        case 141: return BC5_UNORM;         // BC5_UNORM_BLOCK
        case 142: return BC5_SNORM;         // BC5_SNORM_BLOCK
        case 143: return BC6H_UF16;         // BC6H_UFLOAT_BLOCK
        case 144: return BC6H_SF16;         // BC6H_SFLOAT_BLOCK
        case 145: return BC7_UNORM;         // BC7_UNORM_BLOCK
        case 146: return BC7_UNORM_SRGB;    // BC7_SRGB_BLOCK
        default:
            std::unreachable();
        }
    }

    Crowy::ImageData loadCompressed(const std::filesystem::path& path){
        using namespace Crowy;

        auto bin = readFileAsBinary(path);

        ktxTexture2* tex2 = nullptr;
        CHECK_KTX_ERROR(ktxTexture2_CreateFromMemory(
            bin.data(),
            bin.size(),
            KTX_TEXTURE_CREATE_LOAD_IMAGE_DATA_BIT,
            &tex2
        ), "Failed to create KTX texture");

        if(ktxTexture2_NeedsTranscoding(tex2)){
            CHECK_KTX_ERROR(ktxTexture2_TranscodeBasis(
                tex2,
                KTX_TTF_BC7_RGBA,
                0
            ), "Failed to transcode");
        }

        auto tex = ktxTexture(tex2);
        ImageData imageData{
            .format = convert(tex2->vkFormat),
            .width = tex->baseWidth,
            .height = tex->baseHeight,
            .mipLevels = tex->numLevels,
            .arraySize = tex->numLayers * tex->numFaces
        };
        const void* data = ktxTexture_GetData(tex);
        const auto size = ktxTexture_GetDataSize(tex);
        imageData.blob.resize(size);
        std::memcpy(imageData.blob.data(), data, size);

        imageData.subs.resize(imageData.mipLevels * imageData.arraySize);
        for(u32 slice=0; slice<imageData.arraySize; ++slice){
            for(u32 mip=0; mip<imageData.mipLevels; ++mip){
                ktx_size_t offset = 0;
                ktxTexture_GetImageOffset(
                    tex,
                    mip,
                    slice,
                    0,
                    &offset
                );

                const auto mipW = std::max(1u, imageData.width >> mip);
                imageData.subs[slice * imageData.mipLevels + mip] = RHISubresourceData{
                    .data = ptrAdd(imageData.blob.data(), offset),
                    .rowPitch = GetRowPitch(imageData.format, mipW)
                };
            }
        }

        return imageData;
    }
}

namespace{
    Crowy::ImageData loadUncompressed(const std::filesystem::path& path){
        using namespace Crowy;

        auto bin = readFileAsBinary(path);

        int width = 0, height = 0;
        const auto data = stbi_load_from_memory(
            bin.data(), static_cast<int>(bin.size()),
            &width, &height, nullptr,
            STBI_rgb_alpha
        );
        if(data == nullptr){
            throw std::runtime_error(std::format(
                "Image load Failed: {} - {}",
                path, stbi_failure_reason()
            ));
        }

        ImageData imageData{
            .format = RHIPixelFormat::RGBA8_UNORM_SRGB,
            .width = static_cast<u32>(width),
            .height = static_cast<u32>(height)
        };
        const auto size = width * height * 4;
        imageData.blob.resize(size);
        std::memcpy(imageData.blob.data(), data, size);
        stbi_image_free(data);

        imageData.subs.push_back(RHISubresourceData{
            .data = imageData.blob.data(),
            .rowPitch = GetRowPitch(imageData.format, imageData.width)
        });

        return imageData;
    }
}

namespace Crowy
{
    ImageData LoadImage(const std::filesystem::path& path){
        auto ext = path.extension().string();

        if(ext == ".ktx2"){
            return loadCompressed(path);
        }

        return loadUncompressed(path);
    }
}
