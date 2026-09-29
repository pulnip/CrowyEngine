#pragma once

#include <filesystem>
#include <memory>
#include <span>
#include <vector>
#include "Primitives.hpp"
#include "RHIDefinitions.hpp"
#include "Semantics.hpp"

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
        // releases through ISlangUnknown without this header seeing slang.h
        struct SlangRelease{
            void operator()(slang::ISession*) const noexcept;
            void operator()(slang::IComponentType*) const noexcept;
        };
        using SlangSession = std::unique_ptr<slang::ISession, SlangRelease>;
        using SlangProgram = std::unique_ptr<slang::IComponentType, SlangRelease>;

        // as given, for messages; the compile resolves it to an absolute path
        Str path;
        // declared first, so the program is released before its session
        SlangSession session;
        SlangProgram program;
        std::size_t hash = 0;

        RHIProgramReflection reflection;

    public:
        ~RHIShader() = default;
        CROWY_DECLARE_MOVE_ONLY_NOEXCEPT(RHIShader)

        RHIShader(
            const std::filesystem::path&,
            RHIBackend backend,
            CStr profile = nullptr,
            // composed after this file, each exporting a link-time type the
            // file declares extern; none may declare a shader parameter
            std::span<const std::filesystem::path> linkedModules = {}
        );

        std::size_t Gethash() const noexcept{
            return hash;
        }
        Size3D GetThreadGroupSize(StrView entryPoint);

        std::span<const RHISamplerUse> GetUsedSamplers(StrView entryPoint);

        std::vector<u8> GetEntryPointCode(StrView entryPoint);
        std::vector<u8> GetTargetCode();

    private:
        const RHIShaderReflection& findEntryPoint(StrView entryPoint) const;
    };
}
