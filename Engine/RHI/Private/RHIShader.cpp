#include <algorithm>
#include <array>
#include <chrono>
#include <format>
#include <memory>
#include <stdexcept>
#include <slang.h>
#include <slang-com-ptr.h>
#include "Assert.hpp"
#include "HashUtil.hpp"
#include "LogLocal.hpp"
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

    Crowy::StrView slangDiagnostics(ISlangBlob* diagnostics){
        if(diagnostics == nullptr || diagnostics->getBufferSize() == 0){
            return {};
        }
        return {
            static_cast<const char*>(diagnostics->getBufferPointer()),
            diagnostics->getBufferSize()
        };
    }

    void logSlangDiagnostics(ISlangBlob* diagnostics){
        if(const auto text = ::slangDiagnostics(diagnostics); !text.empty()){
            LOG_WARN("{}", text);
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

// The result decides success;
// the blob only carries the explanation;
// which is worth keeping on failure and worth logging on success.
#define CHECK_SRESULT_DIAG(expr, diagnostics, msg) \
    do{ \
        if(const auto hr = (expr); SLANG_FAILED(hr)) [[unlikely]]{ \
            throw std::runtime_error(std::format( \
                "{}: {}\n{}", msg, ::SlangResultToString(hr), \
                ::slangDiagnostics(diagnostics) \
            )); \
        } \
        ::logSlangDiagnostics(diagnostics); \
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
                case Mixed: {
                    // Metal legalizes DescriptorHandle<T> members into
                    // real texture slots inside the argument buffer,
                    // so the CB param consumes ConstantBuffer + ShaderResource.
                    bool hasConstantBuffer = false;
                    const auto subCount = param->getCategoryCount();
                    for(unsigned j=0; j<subCount; ++j){
                        switch(param->getCategoryByIndex(j)){
                        case ConstantBuffer:
                            hasConstantBuffer = true;
                            break;
                        case ShaderResource:
                            [[fallthrough]];
                        case DescriptorTableSlot: // for Metal
                            break;
                        default:
                            throw std::runtime_error(
                                "Unexpected sub-category in mixed param"
                            );
                        }
                    }
                    if(!hasConstantBuffer){
                        throw std::runtime_error(
                            "Mixed param without constant buffer"
                        );
                    }
                    refl.nameToSlot[raw] = static_cast<u32>(
                        param->getOffset(ConstantBuffer)
                    );
                    break;
                }
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

        // names in RHI_STATIC_SAMPLERS order;
        // must match Engine/Shader/Sampler.slang
        constexpr std::array staticSamplerNames{
            "linearWrap",
            "linearClamp",
            "linearMirror",
            "linearBorder",
            "nearestWrap",
            "nearestClamp",
            "nearestMirror",
            "nearestBorder"
        };

        struct SamplerParam{
            u32 slot;
            u32 space;
            u32 samplerIndex;
        };

        auto collectSamplerParams(slang::ProgramLayout& layout){
            using namespace slang;

            std::vector<SamplerParam> params;

            const auto numParams = layout.getParameterCount();
            for(u32 i=0; i<numParams; ++i){
                auto param = layout.getParameterByIndex(i);
                if(param->getCategory() != SamplerState)
                    continue;

                const StrView name = param->getName();
                const auto found = std::ranges::find(
                    staticSamplerNames,
                    name
                );
                if(found == staticSamplerNames.end()){
                    throw std::runtime_error(std::format(
                        "unknown global sampler '{}'; "
                        "only the static samplers of Sampler.slang can be bound",
                        name
                    ));
                }

                params.push_back(SamplerParam{
                    .slot = param->getBindingIndex(),
                    .space = static_cast<u32>(param->getBindingSpace()),
                    .samplerIndex = static_cast<u32>(
                        found - staticSamplerNames.begin()
                    )
                });
            }

            return params;
        }
    }

    void InitGlobalSession(){
        const SlangGlobalSessionDesc desc{};

        CHECK_SRESULT(slang_createGlobalSession2(
            &desc,
            globalSession.writeRef()
        ), "Failed to create Global session");
    }

    void RHIShader::SlangRelease::operator()(
        slang::ISession* session
    ) const noexcept{
        session->release();
    }

    void RHIShader::SlangRelease::operator()(
        slang::IComponentType* program
    ) const noexcept{
        program->release();
    }

    RHIShader::RHIShader(
        const std::filesystem::path& filePath,
        RHIBackend backend,
        CStr profile,
        std::span<const std::filesystem::path> linkedModules
    )
        : path(toUTF8String(filePath)),
          hash(hashAll(filePath))
    {
        using namespace slang;
        using namespace Slang;

        const auto started = std::chrono::steady_clock::now();

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
        const auto modulePath = toUTF8String(absPath);

        const auto searchDir = toUTF8String(absPath.parent_path());
        // a shader lives beside whatever owns it and includes its neighbours
        // from there; Engine/Shader is the engine's own library, so it is an
        // include root for every shader in the tree
        const auto libraryDir = toUTF8String(
            std::filesystem::absolute("Engine/Shader")
        );
        const std::array searchPaths{
            searchDir.c_str(),
            libraryDir.c_str()
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
            std::out_ptr(session)
        ), "Failed to create session");

        // Compile
        IModule* mod = nullptr;
        {
            ComPtr<ISlangBlob> diagnostics = nullptr;
            mod = session->loadModule(
                modulePath.c_str(),
                diagnostics.writeRef()
            );
            // loadModule reports failure by returning null, not by a result
            if(mod == nullptr) [[unlikely]]{
                throw std::runtime_error(std::format(
                    "Failed to load slang module '{}'\n{}",
                    path, ::slangDiagnostics(diagnostics.get())
                ));
            }
            ::logSlangDiagnostics(diagnostics.get());
        }

        // the session owns every module it loads, as it owns the file's
        std::vector<IModule*> modules;
        std::vector<std::filesystem::path> linkedPaths;
        Str linkedNames;
        for(const auto& linked: linkedModules){
            const auto linkedPath = std::filesystem::absolute(linked);
            // composing one module twice is an error Slang reports vaguely
            if(std::ranges::contains(linkedPaths, linkedPath)) [[unlikely]]{
                throw std::runtime_error(std::format(
                    "module '{}' is linked twice", toUTF8String(linked)
                ));
            }
            linkedPaths.push_back(linkedPath);

            ComPtr<ISlangBlob> diagnostics = nullptr;
            IModule* linkedModule = session->loadModule(
                toUTF8String(linkedPath).c_str(),
                diagnostics.writeRef()
            );
            if(linkedModule == nullptr) [[unlikely]]{
                throw std::runtime_error(std::format(
                    "Failed to load slang module '{}'\n{}",
                    toUTF8String(linked),
                    ::slangDiagnostics(diagnostics.get())
                ));
            }
            ::logSlangDiagnostics(diagnostics.get());

            // one that included SceneData.slang would declare `pass` and
            // `view` a second time
            const auto parameterCount =
                linkedModule->getLayout(0)->getParameterCount();
            if(parameterCount > 0) [[unlikely]]{
                throw std::runtime_error(std::format(
                    "linked module '{}' declares {} shader parameters",
                    toUTF8String(linked),
                    parameterCount
                ));
            }

            modules.push_back(linkedModule);
            if(!linkedNames.empty())
                linkedNames += ", ";
            linkedNames += toUTF8String(linked);
        }

        // entry points
        const auto entryPointCount = mod->getDefinedEntryPointCount();
        std::vector<ComPtr<IEntryPoint>> entryPoints;
        entryPoints.reserve(entryPointCount);

        // the file, its modules, then every entry point, which resolves the
        // file's extern types against the modules' exports
        std::vector<IComponentType*> components;
        components.reserve(1 + modules.size() + entryPointCount);
        components.push_back(mod);
        components.insert(components.end(), modules.begin(), modules.end());

        for(SlangInt32 i=0; i<entryPointCount; ++i){
            ComPtr<IEntryPoint> entryPoint = nullptr;
            CHECK_SRESULT(mod->getDefinedEntryPoint(
                i,
                entryPoint.writeRef()
            ), "Failed to find entry point");

            entryPoints.push_back(std::move(entryPoint));
            components.push_back(entryPoints.back().get());
        }

        ComPtr<IComponentType> composed = nullptr;
        {
            ComPtr<ISlangBlob> diagnostics = nullptr;
            CHECK_SRESULT_DIAG(session->createCompositeComponentType(
                components.data(),
                components.size(),
                composed.writeRef(),
                diagnostics.writeRef()
            ), diagnostics.get(), "Failed to compose slang component");
        }

        // link to single program
        {
            ComPtr<ISlangBlob> diagnostics = nullptr;
            CHECK_SRESULT_DIAG(composed->link(
                std::out_ptr(program),
                diagnostics.writeRef()
            ), diagnostics.get(), "Failed to link slang component");
        }

        reflection = extractReflection(*program->getLayout());
        const auto samplerParams = collectSamplerParams(
            *program->getLayout()
        );

        for(auto& [name, refl]: reflection.shaderRefl){
            ComPtr<IMetadata> metadata;
            ComPtr<ISlangBlob> diagnostics = nullptr;
            CHECK_SRESULT_DIAG(program->getEntryPointMetadata(
                refl.entryPointIndex,
                0,
                metadata.writeRef(),
                diagnostics.writeRef()
            ), diagnostics.get(), std::format(
                "Failed to get entry point metadata for '{}' in {}",
                name, path
            ));

            ::checkMetadata(*metadata);

            for(const auto& sampler: samplerParams){
                bool used = false;
                CHECK_SRESULT(metadata->isParameterLocationUsed(
                    SLANG_PARAMETER_CATEGORY_SAMPLER_STATE,
                    sampler.space,
                    sampler.slot,
                    used
                ), "Failed to query sampler usage");

                if(used){
                    refl.usedSamplers.push_back({
                        .slot = sampler.slot,
                        .samplerIndex = sampler.samplerIndex
                    });
                }
            }
        }

        const std::chrono::duration<f64, std::milli> elapsed =
            std::chrono::steady_clock::now() - started;
        if(linkedNames.empty()){
            LOG_DEBUG("compiled {} in {:.0f} ms", path, elapsed.count());
        } else {
            LOG_DEBUG(
                "compiled {} with {} in {:.0f} ms",
                path,
                linkedNames,
                elapsed.count()
            );
        }
    }

    Size3D RHIShader::GetThreadGroupSize(StrView entryPoint){
        return findEntryPoint(entryPoint).threadGroupSize;
    }

    std::span<const RHISamplerUse> RHIShader::GetUsedSamplers(
        StrView entryPoint
    ){
        return findEntryPoint(entryPoint).usedSamplers;
    }

    std::vector<u8> RHIShader::GetEntryPointCode(StrView entryPoint){
        using namespace Slang;

        const auto& refl = findEntryPoint(entryPoint);

        ComPtr<ISlangBlob> code = nullptr;
        ComPtr<ISlangBlob> diagnostics = nullptr;
        CHECK_SRESULT_DIAG(program->getEntryPointCode(
            refl.entryPointIndex,
            0,
            code.writeRef(),
            diagnostics.writeRef()
        ), diagnostics.get(), "failed to get target entry code");

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
        CHECK_SRESULT_DIAG(program->getTargetCode(
            0,
            code.writeRef(),
            diagnostics.writeRef()
        ), diagnostics.get(), "failed to get target code");

        std::vector<u8> bytecode(code->getBufferSize());
        std::memcpy(
            bytecode.data(),
            code->getBufferPointer(),
            code->getBufferSize()
        );

        return bytecode;
    }

    const RHIShaderReflection& RHIShader::findEntryPoint(
        StrView entryPoint
    ) const{
        const auto found = reflection.shaderRefl.find(entryPoint);
        if(found == reflection.shaderRefl.end()){
            throw std::runtime_error(std::format(
                "unknown entry point '{}' in {}", entryPoint, path
            ));
        }

        return found->second;
    }
}
