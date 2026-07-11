#pragma once

#include <filesystem>
#include "ResourceLoader.hpp"
#include "RHIFWD.hpp"
#include "Resource.hpp"

namespace Crowy
{
    class ImmediateResourceLoader: public ResourceLoader<SpriteResource>{
    private:
        RHIDevice& device;
        const std::filesystem::path root;
        // std::vector<Completion> done;

    public:
        ImmediateResourceLoader(RHIDevice&, std::filesystem::path root = "Content/");

        void Submit(const Request&, Handle, ResourceManager<SpriteResource>&) override;
        // Completion for parallel resource loading,
        // no-op for Immediate ResourceLoader
        void Poll(std::vector<Completion>&) override{}
    };
}
