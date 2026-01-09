#include <filesystem>
#include <d3dcompiler.h>
#include "string.hpp"
#include "D3D12Shader.hpp"

namespace Crowy
{
    static const char* getShaderTargetProfile(RHIShaderStage stage){
        switch(stage){
        case RHIShaderStage::VertexShader:   return "vs_5_0";
        case RHIShaderStage::FragmentShader: return "ps_5_0";
        case RHIShaderStage::ComputeShader:  return "cs_5_0";
        default: return nullptr;
        }
    }

    D3D12Shader::D3D12Shader(
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
            if(!profile){
                throw std::runtime_error("Unsupported shader stage");
            }

            Microsoft::WRL::ComPtr<ID3DBlob> shaderBlob;
            Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;

            auto wpath = std::filesystem::path(desc.file).wstring();

            UINT compileFlags = 0;
        #if defined(_DEBUG)
            compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
        #endif

            HRESULT hr = D3DCompileFromFile(
                wpath.c_str(),
                nullptr,
                D3D_COMPILE_STANDARD_FILE_INCLUDE,
                desc.entry,
                profile,
                compileFlags,
                0,
                &shaderBlob,
                &errorBlob
            );

            if(FAILED(hr)){
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
            throw std::runtime_error("Shader bytecode is empty: " + std::string(desc.file));
        }
    }
}
