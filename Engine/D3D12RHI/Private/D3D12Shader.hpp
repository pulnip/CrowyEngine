#pragma once

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <memory>
#include <string>
#include <vector>
#include <d3d12.h>
#include "RHIAPI.hpp"
#include "RHIDefinitions.hpp"
#ifndef USE_STATIC_RHI
    #include "RHIShader.hpp"
#endif

namespace Crowy
{
    class D3D12Shader
#ifndef USE_STATIC_RHI
        : public RHIShader
#endif
    {
    private:
        std::vector<uint8_t> bytecode;
        const RHIShaderStage stage;

    public:
        D3D12Shader(
            ID3D12Device* device,
            const RHIShaderCreateDesc& desc
        )
            : stage(desc.stage)
        {
            auto ext = std::filesystem::path(desc.file).extension().string();

            if(ext == ".cso" || ext == ".dxbc" || ext == ".dxil"){
                // Load precompiled shader bytecode
                std::ifstream file(desc.file, std::ios::binary | std::ios::ate);
                if(!file.is_open()){
                    throw std::runtime_error("Failed to open shader file: " + std::string(desc.file));
                }

                auto fileSize = file.tellg();
                file.seekg(0, std::ios::beg);

                bytecode.resize(fileSize);
                file.read(reinterpret_cast<char*>(bytecode.data()), fileSize);
            }
            else if(ext == ".hlsl"){
                // Note: Runtime HLSL compilation requires D3DCompile
                // This would need to be implemented with D3DCompileFromFile
                // For now, throw an error suggesting precompilation
                throw std::runtime_error(
                    "Runtime HLSL compilation not implemented. Please precompile shaders to .cso format"
                );
            }
            else{
                throw std::runtime_error("Unknown shader file format: " + ext);
            }

            if(bytecode.empty()){
                throw std::runtime_error("Shader bytecode is empty: " + std::string(desc.file));
            }
        }

        ~D3D12Shader(){
        }

        RHIShaderStage getStage() const RHI_OVERRIDE{
            return stage;
        }

        D3D12_SHADER_BYTECODE getBytecode() const{
            D3D12_SHADER_BYTECODE bc = {};
            bc.pShaderBytecode = bytecode.data();
            bc.BytecodeLength = bytecode.size();
            return bc;
        }
    };
}
