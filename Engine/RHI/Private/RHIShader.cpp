#include <array>
#include <format>
#include <stdexcept>
#include <slang.h>
#include <slang-com-ptr.h>
#include "Assert.hpp"
#include "HashUtil.hpp"
#include "RHIShader.hpp"
#include "StringUtil.hpp"

namespace{
    Crowy::Str SlangResultToString(SlangResult result){
    #define CASE_RETURN(x) case x: return #x;
        switch(result){
        CASE_RETURN(SLANG_OK)
        CASE_RETURN(SLANG_FAIL)
        CASE_RETURN(SLANG_E_NOT_IMPLEMENTED)
        CASE_RETURN(SLANG_E_NOT_FOUND)
        CASE_RETURN(SLANG_E_INVALID_ARG)
        CASE_RETURN(SLANG_E_OUT_OF_MEMORY)
        CASE_RETURN(SLANG_E_INVALID_HANDLE)
        CASE_RETURN(SLANG_E_UNINITIALIZED)
        CASE_RETURN(SLANG_E_PENDING)
        CASE_RETURN(SLANG_E_CANNOT_OPEN)
        CASE_RETURN(SLANG_E_BUFFER_TOO_SMALL)
        CASE_RETURN(SLANG_E_TIME_OUT)
        CASE_RETURN(SLANG_E_INTERNAL_FAIL)
        CASE_RETURN(SLANG_E_NOT_AVAILABLE)
        default:
            return std::format("Unknown SlangResult (0x{:08X})",
                static_cast<std::uint32_t>(result));
        }
    #undef CASE_RETURN
    }

    void throwIfSlangError(ISlangBlob* diagnostics){
        if(diagnostics != nullptr && diagnostics->getBufferSize() > 0){
            auto errors = diagnostics->getBufferPointer();
            throw std::runtime_error(std::format(
                "Slang compile failed: {}",
                errors
            ));
        }
    }

    Slang::ComPtr<slang::IGlobalSession> globalSession;

    SlangCompileTarget convert(Crowy::RHIBackend backend){
        using enum Crowy::RHIBackend;

        switch(backend){
        case DirectX12: return SLANG_DXIL;
        case Metal:     return SLANG_METAL_LIB;
        default:
            std::unreachable();
        }
    }

    void checkMetadata(slang::IMetadata& metadata){
        using namespace slang;

        auto bindless = static_cast<IBindlessResourceMetadata*>(
            metadata.castAs(IBindlessResourceMetadata::getTypeGuid())
        );

        (void)bindless->usesBindlessResourceHeap();
    }

    Crowy::RHIShaderReflection extractReflection(slang::ProgramLayout& layout){
        using namespace slang;
        using namespace Crowy;

        RHIShaderReflection refl;

        const auto numParams = layout.getParameterCount();
        for(u32 i=0; i<numParams; ++i){
            auto param = layout.getParameterByIndex(i);
            auto category = param->getCategory();

            auto raw = param->getName();

            switch(category){
            case ConstantBuffer:
                refl.nameToSlot[raw] = param->getBindingIndex();
                break;
            case SamplerState:
                // use static sampler(index fixed), so skip
                break;
            default:
                // SRV, UAV -> use only Descriptor Heap Indexing
                throw std::runtime_error("Unexpected param category");
            }
        }

        // thread group size
        auto entryPoint = layout.getEntryPointByIndex(0);
        if(entryPoint->getStage() == SLANG_STAGE_COMPUTE){
            SlangUInt threadGroupSize[3] = {1, 1, 1};
            entryPoint->getComputeThreadGroupSize(3, threadGroupSize);

            refl.threadGroupSize.x = threadGroupSize[0];
            refl.threadGroupSize.y = threadGroupSize[1];
            refl.threadGroupSize.z = threadGroupSize[2];
        }

        return refl;
    }
}

#define CHECK_SRESULT(expr, msg) \
    do{ \
        if(const auto hr = (expr); SLANG_FAILED(hr)) [[unlikely]]{ \
            throw std::runtime_error(std::format( \
                "{}: {}", msg, ::SlangResultToString(hr) \
            )); \
        } \
    } while(false)

namespace Crowy
{
    void InitGlobalSession(){
        const SlangGlobalSessionDesc desc{};

        CHECK_SRESULT(slang_createGlobalSession2(
            &desc,
            globalSession.writeRef()
        ), "Failed to create Global session");
    }

    RHIShader::RHIShader(
        const RHIShaderDesc& desc,
        RHIBackend backend,
        CStr profile
    )
        : hash(hashAll(desc))
    {
        using namespace slang;
        using namespace Slang;

        CROWY_ASSERT(globalSession != nullptr,
            "Did you call InitGlobalSession()?"
        );

        const std::array targetDescs{
            TargetDesc{
                .format = ::convert(backend),
                .profile = globalSession->findProfile(profile)
            }
        };
        const auto path = toUTF8String(std::filesystem::absolute(desc.path));
        const std::array searchPaths{
            path.c_str()
        };
        const SessionDesc sessionDesc{
            .targets = targetDescs.data(),
            .targetCount = targetDescs.size(),
            .defaultMatrixLayoutMode = SLANG_MATRIX_LAYOUT_COLUMN_MAJOR,
            .searchPaths = searchPaths.data(),
            .searchPathCount = searchPaths.size()
        };
        ComPtr<ISession> session = nullptr;
        CHECK_SRESULT(globalSession->createSession(
            sessionDesc,
            session.writeRef()
        ), "Failed to create session");

        // Compile
        IModule* mod = nullptr;
        {
            ComPtr<ISlangBlob> diagnostics = nullptr;
            mod = session->loadModule(
                path.c_str(),
                diagnostics.writeRef()
            );
            // warning and errors
            ::throwIfSlangError(diagnostics.get());
        }

        // find entry point
        ComPtr<IEntryPoint> entryPoint = nullptr;
        CHECK_SRESULT(mod->findEntryPointByName(
            desc.entryPoint.c_str(),
            entryPoint.writeRef()
        ), "Failed to find entry point");

        // compose module + entryPoint
        std::array<IComponentType*, 2> components{
            mod,
            entryPoint.get()
        };
        ComPtr<IComponentType> composed = nullptr;
        CHECK_SRESULT(session->createCompositeComponentType(
            components.data(),
            components.size(),
            composed.writeRef()
        ), "Failed to compose slang component");

        // link to single program
        ComPtr<IComponentType> linked = nullptr;
        {
            ComPtr<ISlangBlob> diagnostics = nullptr;
            CHECK_SRESULT(composed->link(
                linked.writeRef(),
                diagnostics.writeRef()
            ), "Failed to link slang component");

            ::throwIfSlangError(diagnostics.get());
        }

        ComPtr<IMetadata> metadata;
        {
            ComPtr<ISlangBlob> diagnostics = nullptr;
            CHECK_SRESULT(linked->getTargetMetadata(
                0,
                metadata.writeRef(),
                diagnostics.writeRef()
            ), "");

            ::throwIfSlangError(diagnostics.get());

            ::checkMetadata(*metadata);
        }

        // interpret to final target (DirectX IL, Apple IR)
        ComPtr<ISlangBlob> code = nullptr;
        {
            ComPtr<ISlangBlob> diagnostics = nullptr;
            CHECK_SRESULT(linked->getEntryPointCode(
                0,
                0,
                code.writeRef(),
                diagnostics.writeRef()
            ), "failed to get target entry code");

            ::throwIfSlangError(diagnostics.get());
        }

        bytecode.resize(code->getBufferSize());
        std::memcpy(
            bytecode.data(),
            code->getBufferPointer(),
            code->getBufferSize()
        );

        reflection = ::extractReflection(*linked->getLayout());
    }
}
