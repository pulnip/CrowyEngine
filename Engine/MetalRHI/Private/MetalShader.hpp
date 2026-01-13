#pragma once

#include <cstddef>
#include <filesystem>
#include <sstream>
#include <memory>
#include <string>
#include <Metal/Metal.hpp>
#include "string.hpp"
#include "RHIAPI.hpp"
#include "RHIDefinitions.hpp"
#ifndef USE_STATIC_RHI
    #include "RHIShader.hpp"
#endif

namespace Crowy
{
    class MetalShader
#ifndef USE_STATIC_RHI
        : public RHIShader
#endif
    {
    private:
        MTL::Function* function;
        const RHIShaderStage stage;

    public:
        MetalShader(
            MTL::Device* device,
            const RHIShaderCreateDesc& desc
        )
            : stage(desc.stage)
        {
            NS::Error* error = nullptr;
            MTL::Library* library;

            auto ext = std::filesystem::path(desc.file).extension().string();

            if(ext == ".metal"){
                auto code = readFileAsString(desc.file);

                auto source = NS::String::string(code.c_str(), NS::UTF8StringEncoding);
                library = device->newLibrary(source, nullptr, &error);
            }
            else if(ext == ".metallib"){
                auto path = NS::String::string(desc.file, NS::UTF8StringEncoding);
                auto url = NS::URL::fileURLWithPath(path);
                library = device->newLibrary(url, &error);
            }
            else{
                throw std::runtime_error("Unknown file format: " + ext);
            }

            if(!library){
                std::string errorMsg = error->localizedDescription()->utf8String();
                throw std::runtime_error(
                    "Shader compile failed: " + errorMsg
                );
            }

            auto entry = NS::String::string(desc.entry, NS::UTF8StringEncoding);
            auto func = library->newFunction(entry);
            // func holds reference
            library->release();

            if(!func){
                throw std::runtime_error(
                    "Entry point not found:" + std::string(desc.entry)
                );
            }

            function = func;
        }
        ~MetalShader(){
            function->release();
        }

        RHIShaderStage getStage() const noexcept RHI_OVERRIDE{
            return stage;
        }

        MTL::Function* get() const noexcept{ return function; }
    };
}
