#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <vector>
#include <d3d12.h>
#include <wrl/client.h>
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
        );

        ~D3D12Shader() = default;

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
