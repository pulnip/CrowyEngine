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

    void checkMetadata(slang::IMetadata& metadata){
        using namespace slang;

        auto bindless = static_cast<IBindlessResourceMetadata*>(
            metadata.castAs(IBindlessResourceMetadata::getTypeGuid())
        );

        (void)bindless->usesBindlessResourceHeap();
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
    namespace{
        SlangCompileTarget convert(RHIBackend backend){
            using enum RHIBackend;

            switch(backend){
            case DirectX12: return SLANG_DXIL;
            case Metal:     return SLANG_METAL_LIB;
            default:
                std::unreachable();
            }
        }

        auto extractReflection(slang::ProgramLayout& layout){
            using namespace slang;

            RHIProgramReflection refl;

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

            const auto count = layout.getEntryPointCount();
            for(SlangUInt i = 0; i < count; ++i){
                auto entryPoint = layout.getEntryPointByIndex(i);

                // thread group size
                SlangUInt threadGroupSize[3] = {1, 1, 1};
                entryPoint->getComputeThreadGroupSize(3, threadGroupSize);

                refl.shaderRefl.emplace(
                    entryPoint->getName(),
                    RHIShaderReflection{
                        .entryPointIndex = i,
                        .threadGroupSize = Size3D{
                            .x = static_cast<u32>(threadGroupSize[0]),
                            .y = static_cast<u32>(threadGroupSize[1]),
                            .z = static_cast<u32>(threadGroupSize[2])
                        }
                    }
                );
            }

            return refl;
        }
    }

    void InitGlobalSession(){
        const SlangGlobalSessionDesc desc{};

        CHECK_SRESULT(slang_createGlobalSession2(
            &desc,
            globalSession.writeRef()
        ), "Failed to create Global session");
    }

    RHIShader::RHIShader(
        const std::filesystem::path& filePath,
        RHIBackend backend,
        CStr profile
    )
        : hash(hashAll(filePath))
    {
        using namespace slang;
        using namespace Slang;

        CROWY_ASSERT(globalSession != nullptr,
            "Did you call InitGlobalSession()?"
        );

        const std::array targetDescs{
            TargetDesc{
                .format = convert(backend),
                .profile = globalSession->findProfile(profile)
            }
        };
        const auto absPath = std::filesystem::absolute(filePath);
        const auto path = toUTF8String(absPath);

        const auto searchDir = toUTF8String(absPath.parent_path());
        const std::array searchPaths{
            searchDir.c_str()
        };
        const std::array compilerOptions{
            CompilerOptionEntry{
                .name = CompilerOptionName::GenerateWholeProgram,
                .value = {
                    .kind = CompilerOptionValueKind::Int,
                    .intValue0 = 1
                }
            }
        };
        const SessionDesc sessionDesc{
            .targets = targetDescs.data(),
            .targetCount = targetDescs.size(),
            .defaultMatrixLayoutMode = SLANG_MATRIX_LAYOUT_COLUMN_MAJOR,
            .searchPaths = searchPaths.data(),
            .searchPathCount = searchPaths.size(),
            .compilerOptionEntries = compilerOptions.data(),
            .compilerOptionEntryCount = backend == RHIBackend::Metal ?
                static_cast<u32>(compilerOptions.size()) : 0
        };
        CHECK_SRESULT(globalSession->createSession(
            sessionDesc,
            &session
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

        // entry points
        const auto entryPointCount = mod->getDefinedEntryPointCount();
        std::vector<ComPtr<IEntryPoint>> entryPoints;
        entryPoints.reserve(entryPointCount);

        // compose module + entryPoint
        std::vector<IComponentType*> components(entryPointCount + 1);
        components[0] = mod;

        for(SlangInt32 i=0; i<entryPointCount; ++i){
            ComPtr<IEntryPoint> entryPoint = nullptr;
            CHECK_SRESULT(mod->getDefinedEntryPoint(
                i,
                entryPoint.writeRef()
            ), "Failed to find entry point");

            entryPoints.push_back(std::move(entryPoint));
            components[i + 1] = entryPoints[i].get();
        }

        ComPtr<IComponentType> composed = nullptr;
        CHECK_SRESULT(session->createCompositeComponentType(
            components.data(),
            components.size(),
            composed.writeRef()
        ), "Failed to compose slang component");

        // link to single program
        {
            ComPtr<ISlangBlob> diagnostics = nullptr;
            CHECK_SRESULT(composed->link(
                &program,
                diagnostics.writeRef()
            ), "Failed to link slang component");

            ::throwIfSlangError(diagnostics.get());
        }

        reflection = extractReflection(*program->getLayout());

        for(auto& [name, refl]: reflection.shaderRefl){
            ComPtr<IMetadata> metadata;
            ComPtr<ISlangBlob> diagnostics = nullptr;
            CHECK_SRESULT(program->getEntryPointMetadata(
                refl.entryPointIndex,
                0,
                metadata.writeRef(),
                diagnostics.writeRef()
            ), "");

            ::throwIfSlangError(diagnostics.get());

            ::checkMetadata(*metadata);
        }
    }

    RHIShader::~RHIShader(){
        if(program != nullptr){
            program->release();
            program = nullptr;
        }
        if(session != nullptr){
            session->release();
            session = nullptr;
        }
    }

    Size3D RHIShader::GetThreadGroupSize(StrView entryPoint){
        auto it = reflection.shaderRefl.find(entryPoint);
        CROWY_ASSERT(it != reflection.shaderRefl.end(), "unknown entry point");

        return it->second.threadGroupSize;
    }

    std::vector<u8> RHIShader::GetEntryPointCode(StrView entryPoint){
        using namespace Slang;

        const auto it = reflection.shaderRefl.find(entryPoint);
        CROWY_ASSERT(it != reflection.shaderRefl.end(), "unknown entry point");

        ComPtr<ISlangBlob> code = nullptr;
        ComPtr<ISlangBlob> diagnostics = nullptr;
        CHECK_SRESULT(program->getEntryPointCode(
            it->second.entryPointIndex,
            0,
            code.writeRef(),
            diagnostics.writeRef()
        ), "failed to get target entry code");

        ::throwIfSlangError(diagnostics.get());

        std::vector<u8> bytecode(code->getBufferSize());
        std::memcpy(
            bytecode.data(),
            code->getBufferPointer(),
            code->getBufferSize()
        );

        return bytecode;
    }

    std::vector<u8> RHIShader::GetTargetCode(){
        using namespace Slang;

        ComPtr<ISlangBlob> code = nullptr;
        ComPtr<ISlangBlob> diagnostics = nullptr;
        CHECK_SRESULT(program->getTargetCode(
            0,
            code.writeRef(),
            diagnostics.writeRef()
        ), "failed to get target entry code");

        ::throwIfSlangError(diagnostics.get());

        std::vector<u8> bytecode(code->getBufferSize());
        std::memcpy(
            bytecode.data(),
            code->getBufferPointer(),
            code->getBufferSize()
        );

        return bytecode;
    }
}
