#pragma once

#include <filesystem>
#include <limits>
#include <memory>
#include <span>
#include <vector>

#include "Primitives.hpp"
#include "RHIDefinitions.hpp"
#include "Semantics.hpp"
#include "StringUtil.hpp"

namespace slang
{
    struct IComponentType;
    struct ISession;
}

namespace Crowy
{
    // call once in whole program-lifetime
    void initGlobalSession();

    struct RHISamplerUse {
        // backend binding index ([[sampler(n)]] on Metal)
        u32 slot = 0;
        // index into StaticSamplers
        u32 samplerIndex = 0;
    };

    struct RHIShaderReflection {
        u64 entryPointIndex = std::numeric_limits<u64>::max();
        // for compute pipeline, (0, 0, 0) for others.
        Size3D threadGroupSize{0, 0, 0};

        std::vector<RHISamplerUse> usedSamplers;
    };
    struct RHIProgramReflection {
        StringHashMap<RHIShaderReflection> shaderRefl;
    };

    class RHIShader {
    private:
        // releases through ISlangUnknown without this header seeing slang.h
        struct SlangRelease {
            void operator()(slang::ISession*) const noexcept;
            void operator()(slang::IComponentType*) const noexcept;
        };
        using SlangSession = std::unique_ptr<slang::ISession, SlangRelease>;
        using SlangProgram =
            std::unique_ptr<slang::IComponentType, SlangRelease>;

        // as given, for messages; the compile resolves it to an absolute path
        Str path;
        // declared first, so the program is released before its session
        SlangSession session;
        SlangProgram program;

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
            std::span<const std::filesystem::path> linkedModules = {},
            // the one entry composed, else every entry the file defines; Metal
            // links a whole program, handing each entry every entry's globals
            StrView onlyEntryPoint = {}
        );

        Size3D GetThreadGroupSize(StrView entryPoint);

        std::span<const RHISamplerUse> GetUsedSamplers(StrView entryPoint);

        std::vector<u8> GetEntryPointCode(StrView entryPoint);
        std::vector<u8> GetTargetCode();

    private:
        const RHIShaderReflection& findEntryPoint(StrView entryPoint) const;
    };
}
