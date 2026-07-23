#include "ImageLoader.hpp"
#include "ImmediateResourceLoader.hpp"
#include "ResourceManager.hpp"
#include "RHIDevice.hpp"
#include "RHITexture.hpp"

namespace Crowy
{
    ImmediateResourceLoader::ImmediateResourceLoader(
        RHIDevice& device,
        std::filesystem::path root
    )
        : device(device)
        , root(root)
    {}

    void ImmediateResourceLoader::Submit(
        const Request& request, Handle handle,
        ResourceManager<SpriteResource>& resourceManager
    ){
        auto imagePath = root / request.path;
        auto image = LoadImage(imagePath);

        auto texture = device.CreateTexture(
            RHITextureCreateDesc{
                .width = image.width, .height = image.height,
                .mipLevels = image.mipLevels,
                .arraySize = image.arraySize,
                .format = image.format,
                .usage = RHITextureUsage::ShaderRead,
                .initialData = image.subs
            }
        );

        resourceManager.GetRef(handle) = SpriteResource{
            .texture = std::move(texture),
            .sheetSize = request.sheetSize,
            .animations = request.animations
        };
    }
}
