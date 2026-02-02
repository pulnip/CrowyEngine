#pragma once

#include <cstdint>
#include <filesystem>
#include <stdexcept>
#include <utility>
#include <vector>
#include <d3d12.h>
#include <d3dcompiler.h>
#include <wrl/client.h>
#include "string.hpp"
#include "RHIAPI.hpp"
#include "RHIDefinitions.hpp"
#ifndef USE_STATIC_RHI
    #include "RHIShader.hpp"
#endif

namespace Crowy
{
    static const char* getShaderTargetProfile(RHIShaderStage stage){
        switch(stage){
        case RHIShaderStage::VertexShader:   return "vs_5_0";
        case RHIShaderStage::FragmentShader: return "ps_5_0";
        case RHIShaderStage::ComputeShader:  return "cs_5_0";
        default:
            std::unreachable();
        }
    }

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
                bytecode = readFileAsBinary(desc.file);
            }
            else if(ext == ".hlsl"){
                auto profile = getShaderTargetProfile(desc.stage);

                UINT compileFlags = 0;
            #if defined(_DEBUG)
                compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
            #endif

                Microsoft::WRL::ComPtr<ID3DBlob> shaderBlob;
                Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;

                if(FAILED(D3DCompileFromFile(
                    desc.file,
                    nullptr,
                    D3D_COMPILE_STANDARD_FILE_INCLUDE,
                    desc.entry,
                    profile,
                    compileFlags,
                    0,
                    &shaderBlob,
                    &errorBlob
                ))){
                    std::string errorMsg = "HLSL compile failed";
                    if(errorBlob){
                        errorMsg += ": ";
                        errorMsg += static_cast<const char*>(errorBlob->GetBufferPointer());
                    }
                    throw std::runtime_error(errorMsg);
                }

                bytecode.resize(shaderBlob->GetBufferSize());
                memcpy(bytecode.data(), shaderBlob->GetBufferPointer(), shaderBlob->GetBufferSize());
            }
            else{
                throw std::runtime_error("Unknown shader file format: " + ext);
            }

            if(bytecode.empty()){
                throw std::runtime_error("Shader bytecode is empty");
            }
        }

        RHIShaderStage getStage() const RHI_OVERRIDE{
            return stage;
        }

        auto getBytecodePointer(){ return bytecode.data(); }
        auto getBytecodeSize   (){ return bytecode.size(); }

        auto getBytecode() const{
            return D3D12_SHADER_BYTECODE{
                .pShaderBytecode = bytecode.data(),
                .BytecodeLength = bytecode.size()
            };
        }
    };
}
