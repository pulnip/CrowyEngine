#pragma once

#include <vector>
#include "Primitives.hpp"
#include "RHIDefinitions.hpp"

namespace slang{
    struct IComponentType;
    struct ISession;
}

namespace Crowy
{
    // call once in whole program-lifetime
    void InitGlobalSession();

    class RHIShader{
    private:
        slang::ISession* session = nullptr;
        slang::IComponentType* program = nullptr;
        std::size_t hash = 0;

        RHIProgramReflection reflection;

    public:
        RHIShader(
            const std::filesystem::path&,
            RHIBackend backend,
            CStr profile = nullptr
        );
        ~RHIShader();

        std::size_t Gethash() const noexcept{
            return hash;
        }
        Size3D GetThreadGroupSize(StrView entryPoint);

        std::vector<u8> GetEntryPointCode(StrView entryPoint);
        std::vector<u8> GetTargetCode();
    };
}
