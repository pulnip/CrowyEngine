#pragma once

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>
#include <d3d11.h>
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

    class D3D11Shader
#ifndef USE_STATIC_RHI
        : public RHIShader
#endif
    {
    private:
        std::vector<uint8_t> bytecode;
        ID3D11VertexShader* vs = nullptr;
        ID3D11PixelShader* ps = nullptr;
        ID3D11ComputeShader* cs = nullptr;
        const RHIShaderStage stage;

    public:
        D3D11Shader(
            ID3D11Device* device,
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
                if(profile == nullptr){
                    throw std::runtime_error("Unsupported shader stage");
                }

                UINT compileFlags = 0;
            #if defined(_DEBUG) || !defined(NDEBUG)
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

            switch(desc.stage){
            case RHIShaderStage::VertexShader:
                device->CreateVertexShader(bytecode.data(), bytecode.size(), nullptr, &vs);
                vs->SetPrivateData(
                    WKPDID_D3DDebugObjectName, 
                    static_cast<UINT>(strlen(desc.entry)),
                    desc.entry
                );
                break;
            case RHIShaderStage::FragmentShader:
                device->CreatePixelShader(bytecode.data(), bytecode.size(), nullptr, &ps);
                ps->SetPrivateData(
                    WKPDID_D3DDebugObjectName, 
                    static_cast<UINT>(strlen(desc.entry)),
                    desc.entry
                );
                break;
            case RHIShaderStage::ComputeShader:
                device->CreateComputeShader(bytecode.data(), bytecode.size(), nullptr, &cs);
                cs->SetPrivateData(
                    WKPDID_D3DDebugObjectName, 
                    static_cast<UINT>(strlen(desc.entry)),
                    desc.entry
                );
                break;
            default:
                std::unreachable();
            }
        }

        ~D3D11Shader(){
            if(vs != nullptr){
                vs->Release();
                vs = nullptr;
            }
            if(ps != nullptr){
                ps->Release();
                ps = nullptr;
            }
            if(cs != nullptr){
                cs->Release();
                cs = nullptr;
            }
        }

        RHIShaderStage getStage() const noexcept RHI_OVERRIDE{
            return stage;
        }

        void* getBytecodePointer() noexcept{ return bytecode.data(); }
        size_t getBytecodeSize() noexcept{ return bytecode.size(); }

        ID3D11VertexShader* getVS() const noexcept{ return vs; }
        ID3D11PixelShader*  getPS() const noexcept{ return ps; }
    };
}
