#pragma once

#include <vector>
#include "Primitives.hpp"
#include "RHIDefinitions.hpp"

namespace Crowy
{
    // call once in whole program-lifetime
    void InitGlobalSession();

    class RHIShader{
    private:
        std::vector<u8> bytecode;
        std::size_t hash = 0;

        RHIShaderReflection reflection;

    public:
        RHIShader(
            const RHIShaderDesc&,
            RHIBackend backend,
            CStr profile = nullptr
        );

        const void* GetBytecode() const noexcept{
            return bytecode.data();
        }
        usize GetBytecodeLength() const noexcept{
            return bytecode.size();
        }
        std::size_t Gethash() const noexcept{
            return hash;
        }

        const auto& GetRefl() const noexcept{
            return reflection;
        }
    };
}
