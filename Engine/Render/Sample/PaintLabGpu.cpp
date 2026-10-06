#include "PaintLabGpu.hpp"

#include <algorithm>
#include <array>
#include <cstring>
#include <utility>

#include "IntMath.hpp"
#include "PaintAtlasBaker.hpp"
#include "RHIBuffer.hpp"
#include "RHIDevice.hpp"
#include "RHITexture.hpp"
#include "RenderTexture.hpp"

namespace Crowy
{
    namespace
    {
        constexpr CStr BrushShader =
            "Engine/Render/Sample/Paint/PaintBrush.slang";
        constexpr CStr CopyShader =
            "Engine/Render/Sample/Paint/PaintCopy.slang";
        constexpr CStr SurfaceShader =
            "Engine/Render/Sample/Paint/PaintSurface.slang";
        constexpr CStr PanelShader =
            "Engine/Render/Sample/Paint/PanelAtlas.slang";
        constexpr CStr ShapeLabShader =
            "Engine/Render/Sample/Paint/PanelShapeLab.slang";

        // PaintIdNoneColor: no id, no height, "far" in B
        constexpr Color PaintClearColor{7.0f / 255.0f, 0.0f, 0.0f, 1.0f};

        Vec4 toVec4(DVec3 v, f32 w = 0.0f) {
            return {
                static_cast<f32>(v.x),
                static_cast<f32>(v.y),
                static_cast<f32>(v.z),
                w
            };
        }

        RHIGraphicsPipelineStateDesc atlasPipeline(
            CStr shader,
            CStr vs,
            CStr fs
        ) {
            RHIGraphicsPipelineStateDesc desc{
                .preRasterizer =
                    RHILegacyFrontendDesc{
                        .vertexShader =
                            RHIShaderDesc{.path = shader, .entryPoint = vs}
                    },
                .rasterizer = RHIRasterizerState{.cullMode = RHICullMode::None},
                .fragmentShader =
                    RHIShaderDesc{.path = shader, .entryPoint = fs},
                .renderTargetCount = 1,
                .profile = "sm_6_8"
            };
            desc.renderTargetFormats[0] = RHIPixelFormat::RGBA8_UNORM;

            return desc;
        }

        RHIViewport fullViewport(u32 size) {
            const auto s = static_cast<f32>(size);

            return RHIViewport{0.0f, 0.0f, s, s, 0.0f, 1.0f};
        }

        RHIScissorRect scissorOf(const IntRect& rect) {
            return RHIScissorRect{
                rect.min.x,
                rect.min.y,
                rect.max.x,
                rect.max.y
            };
        }
    }

    PaintTexture::PaintTexture(RHITextureRAII texture)
        : texture(std::move(texture)) {}

    void PaintTexture::NewFrame() noexcept {
        pending.reset();
        touched = false;
    }

    std::optional<RHITextureBarrier> PaintTexture::Acquire(
        RHIResourceUsage use,
        bool discard
    ) {
        using enum RHIResourceUsage;

        if(pending) {
            CROWY_ASSERT(
                state == use,
                "an acquire must match the release it pairs"
            );
            const auto edge = *pending;
            pending.reset();
            touched = true;
            return edge;
        }
        if(state == Undefined) {
            state = use;
            touched = true;
            return MakeBarrier(*texture, Undefined, use);
        }
        if(touched) {
            CROWY_ASSERT(state == use, "a change of use needs a release first");
            return std::nullopt;
        }

        // the first use this command list waits on the submission that last
        // used it, even in the same use
        const auto before = std::exchange(state, use);
        touched = true;

        return MakeCrossSubmissionBarrier(*texture, before, use, discard);
    }

    RHITextureBarrier PaintTexture::Release(RHIResourceUsage next) {
        const auto edge = MakeBarrier(*texture, state, next);
        pending = edge;
        state = next;
        touched = true;

        return edge;
    }

    PaintGpu::PaintGpu(RHIDevice& device)
        : device(device) {
        const auto texel =
            [&](RHIPixelFormat format, const void* data, u32 bytes, CStr name) {
                const std::array initial{
                    RHISubresourceData{.data = data, .rowPitch = bytes}
                };
                return device.CreateTexture(
                    RHITextureCreateDesc{
                        .width = 1,
                        .height = 1,
                        .format = format,
                        .usage = RHITextureUsage::ShaderResource,
                        .initialData = initial
                    },
                    name
                );
            };
        constexpr std::array<u8, 4> NoPaint{7, 0, 0, 255};
        constexpr auto Empty = toHalf(PaintAtlasBaker::EmptyPosition);
        constexpr std::array<u16, 4> NoPosition{Empty, Empty, Empty, 0};
        constexpr u8 NoFade = 0;
        bareAtlas = {
            texel(RHIPixelFormat::RGBA8_UNORM, NoPaint.data(), 4, "paint.bare"),
            texel(
                RHIPixelFormat::RGBA16_FLOAT,
                NoPosition.data(),
                8,
                "position.bare"
            ),
            texel(RHIPixelFormat::R8_UNORM, &NoFade, 1, "edgeFade.bare")
        };
    }

    void PaintGpu::Sync(std::span<const PaintSurface> all) {
        surfaces.resize(all.size());
        for(usize i = 0; i < all.size(); ++i) {
            if(surfaces[i].builtVersion != all[i].LayoutVersion())
                rebuild(i, all[i]);
        }
    }

    void PaintGpu::ClearAll() {
        for(auto& surface: surfaces)
            surface.clearPending = static_cast<bool>(surface.paint);
    }

    void PaintGpu::Clear(usize surface) {
        if(surface < surfaces.size())
            surfaces[surface].clearPending =
                static_cast<bool>(surfaces[surface].paint);
    }

    bool PaintGpu::IsReady(usize surface) const noexcept {
        return surface < surfaces.size() && surfaces[surface].atlasSize > 0;
    }

    void PaintGpu::rebuild(usize index, const PaintSurface& surface) {
        auto& gpu = surfaces[index];
        if(gpu.paint)
            device.Retire(gpu.paint.Take());
        if(gpu.position)
            device.Retire(gpu.position.Take());
        if(gpu.edgeFade)
            device.Retire(std::move(gpu.edgeFade));
        if(gpu.staging)
            device.Retire(std::move(gpu.staging));
        gpu = SurfaceGpu{.builtVersion = surface.LayoutVersion()};

        const auto& layout = surface.Layout();
        const auto size = static_cast<u32>(layout.atlasSize);
        if(size == 0)
            return;

        PaintAtlasBakeOutput baked;
        PaintAtlasBaker::bake(surface.BakeInput(), baked);

        const auto& name = surface.Object().name;
        gpu.paint = PaintTexture(device.CreateTexture(
            RHITextureCreateDesc{
                .width = size,
                .height = size,
                .format = RHIPixelFormat::RGBA8_UNORM,
                .usage = combine(
                    RHITextureUsage::ShaderResource,
                    RHITextureUsage::RenderTarget
                ),
                .clearColor = PaintClearColor
            },
            name + ".paint"
        ));
        gpu.position = PaintTexture(device.CreateTexture(
            RHITextureCreateDesc{
                .width = size,
                .height = size,
                .format = RHIPixelFormat::RGBA16_FLOAT,
                .usage = combine(
                    RHITextureUsage::ShaderResource,
                    RHITextureUsage::CopyDst
                )
            },
            name + ".position"
        ));
        const std::array fadeData{
            RHISubresourceData{.data = baked.edgeFade.data(), .rowPitch = size}
        };
        gpu.edgeFade = device.CreateTexture(
            RHITextureCreateDesc{
                .width = size,
                .height = size,
                .format = RHIPixelFormat::R8_UNORM,
                .usage = RHITextureUsage::ShaderResource,
                .initialData = fadeData
            },
            name + ".edgeFade"
        );

        // a 2048 position atlas is the whole upload ring, so it goes through
        // a staging buffer of its own and a copy instead of initial data
        const auto rowBytes = size * 8u;
        const auto pitch =
            nextMul(rowBytes, device.GetCapabilities().textureRowPitchAlign);
        gpu.staging = device.CreateBuffer(
            RHIBufferCreateDesc{
                .size = pitch * size,
                .memory = RHIMemoryType::CPUWrite
            },
            name + ".positionStaging"
        );
        auto* mapped = static_cast<std::byte*>(gpu.staging->GetMappedPtr());
        for(u32 row = 0; row < size; ++row) {
            std::memcpy(
                mapped + static_cast<usize>(row) * pitch,
                baked.positions.data() + static_cast<usize>(row) * size * 4,
                rowBytes
            );
        }

        gpu.atlasSize = size;
        gpu.uploadPending = true;
        gpu.clearPending = true;
    }

    PaintTexture& PaintGpu::scratchFor(u32 atlasSize) {
        auto& texture = scratch[atlasSize];
        if(!texture) {
            texture = PaintTexture(device.CreateTexture(
                RHITextureCreateDesc{
                    .width = atlasSize,
                    .height = atlasSize,
                    .format = RHIPixelFormat::RGBA8_UNORM,
                    .usage = combine(
                        RHITextureUsage::ShaderResource,
                        RHITextureUsage::RenderTarget
                    ),
                    .clearColor = PaintClearColor
                },
                "paint.scratch"
            ));
        }

        return texture;
    }

    std::span<const RHITextureBarrier> PaintGpu::Record(
        RHICommandList& cmdList,
        PipelineCache& pipelines,
        std::span<const PaintSurface> all,
        std::span<const PaintStampDraw> stamps
    ) {
        for(auto& surface: surfaces) {
            if(surface.paint)
                surface.paint.NewFrame();
            if(surface.position)
                surface.position.NewFrame();
        }
        for(auto& [size, texture]: scratch)
            texture.NewFrame();

        recordUploads(cmdList);
        recordClears(cmdList);
        for(usize i = 0; i < stamps.size(); ++i) {
            const auto& stamp = stamps[i];
            if(stamp.surface >= surfaces.size() ||
               !surfaces[stamp.surface].paint)
                continue;
            const auto size = surfaces[stamp.surface].atlasSize;
            const bool usedAgain = std::any_of(
                stamps.begin() + static_cast<isize>(i) + 1,
                stamps.end(),
                [&](const PaintStampDraw& later) {
                    return later.surface < surfaces.size() &&
                           surfaces[later.surface].atlasSize == size &&
                           static_cast<bool>(surfaces[later.surface].paint);
                }
            );
            recordStamp(
                cmdList,
                pipelines,
                stamp,
                all[stamp.surface],
                usedAgain
            );
        }

        collectHookAcquires();

        return hookAcquires;
    }

    void PaintGpu::recordUploads(RHICommandList& cmdList) {
        std::vector<RHITextureBarrier> acquires;
        std::vector<SurfaceGpu*> uploads;
        for(auto& surface: surfaces) {
            if(!surface.uploadPending)
                continue;
            if(const auto edge =
                   surface.position.Acquire(RHIResourceUsage::CopyDst))
                acquires.push_back(*edge);
            uploads.push_back(&surface);
        }
        if(uploads.empty())
            return;

        cmdList.BeginBlitPass(acquires);
        std::vector<RHITextureBarrier> releases;
        for(auto* surface: uploads) {
            const auto pitch = surface->staging->GetSize() / surface->atlasSize;
            cmdList.Copy(
                *surface->staging,
                0,
                pitch,
                surface->position.Get(),
                RHITextureRegion{
                    .width = surface->atlasSize,
                    .height = surface->atlasSize
                }
            );
            releases.push_back(
                surface->position.Release(RHIResourceUsage::SampledFragment)
            );
        }
        cmdList.EndBlitPass(releases);

        for(auto* surface: uploads) {
            device.Retire(std::move(surface->staging));
            surface->uploadPending = false;
        }
    }

    void PaintGpu::recordClears(RHICommandList& cmdList) {
        for(auto& surface: surfaces) {
            if(!surface.clearPending)
                continue;
            surface.clearPending = false;

            std::vector<RHITextureBarrier> acquires;
            if(const auto edge =
                   surface.paint.Acquire(RHIResourceUsage::RenderTarget, true))
                acquires.push_back(*edge);
            const std::array colors{RHIColorAttachment{
                .texture = &surface.paint.Get(),
                .loadAction = RHILoadAction::Clear,
                .storeAction = RHIStoreAction::Store,
                .clearColor = PaintClearColor
            }};
            cmdList.BeginRenderPass(
                RHIRenderPassDesc{.colorAttachments = colors},
                acquires
            );
            const std::array releases{
                surface.paint.Release(RHIResourceUsage::SampledFragment)
            };
            cmdList.EndRenderPass(releases);
        }
    }

    void PaintGpu::recordStamp(
        RHICommandList& cmdList,
        PipelineCache& pipelines,
        const PaintStampDraw& stamp,
        const PaintSurface& surface,
        bool scratchUsedAgain
    ) {
        using enum RHIResourceUsage;

        if(stamp.rects.empty())
            return;
        auto& gpu = surfaces[stamp.surface];
        auto& work = scratchFor(gpu.atlasSize);
        const auto bounds = surface.ScaledBounds();
        const auto& splat = stamp.splat;
        const auto& s = stamp.stamp;

        // the brush into the scratch: it reads this surface's paint and atlas
        {
            std::vector<RHITextureBarrier> acquires;
            for(auto edge:
                {work.Acquire(RenderTarget, true),
                 gpu.paint.Acquire(SampledFragment),
                 gpu.position.Acquire(SampledFragment)}) {
                if(edge)
                    acquires.push_back(*edge);
            }
            const std::array colors{RHIColorAttachment{
                .texture = &work.Get(),
                .loadAction = RHILoadAction::DontCare,
                .storeAction = RHIStoreAction::Store
            }};
            cmdList.BeginRenderPass(
                RHIRenderPassDesc{.colorAttachments = colors},
                acquires
            );
            cmdList.SetViewport(fullViewport(gpu.atlasSize));
            cmdList.SetPipelineState(pipelines.Resolve(
                atlasPipeline(BrushShader, "vs_brush", "fs_brush")
            ));
            const PaintBrushPush push{
                .previous = gpu.paint.Get().GetReadableID(),
                .position = gpu.position.Get().GetReadableID(),
                .boundsMin = toVec4(bounds.min),
                .boundsSize = toVec4(bounds.Size()),
                .centerRadius = toVec4(s.center, s.radius),
                .axisUStretch = toVec4(s.axisU, s.stretch),
                .axisVImpact = toVec4(s.axisV, splat.impactU),
                .splat =
                    {static_cast<f32>(splat.seed),
                     splat.heightAdd,
                     static_cast<f32>(splat.paintId) / 255.0f,
                     static_cast<f32>(splat.starGen)},
                .lockGens =
                    {static_cast<f32>(splat.lockGens.gen[0]),
                     static_cast<f32>(splat.lockGens.gen[1]),
                     static_cast<f32>(splat.lockGens.gen[2]),
                     static_cast<f32>(splat.lockGens.gen[3])},
                .shape = {
                    splat.shapeNoise,
                    stamp.shapeStage,
                    PaintDistanceRange,
                    0.0f
                }
            };
            cmdList.SetPushGraphicsConstants(push);
            for(const auto& rect: stamp.rects) {
                cmdList.SetScissorRect(scissorOf(rect));
                cmdList.Draw(3);
            }
            const std::array releases{
                work.Release(SampledFragment),
                gpu.paint.Release(RenderTarget)
            };
            cmdList.EndRenderPass(releases);
        }

        // the rectangles back into the paint buffer, which makes it stick
        {
            std::vector<RHITextureBarrier> acquires;
            for(auto edge:
                {gpu.paint.Acquire(RenderTarget),
                 work.Acquire(SampledFragment)}) {
                if(edge)
                    acquires.push_back(*edge);
            }
            const std::array colors{RHIColorAttachment{
                .texture = &gpu.paint.Get(),
                .loadAction = RHILoadAction::Load,
                .storeAction = RHIStoreAction::Store
            }};
            cmdList.BeginRenderPass(
                RHIRenderPassDesc{.colorAttachments = colors},
                acquires
            );
            cmdList.SetViewport(fullViewport(gpu.atlasSize));
            cmdList.SetPipelineState(pipelines.Resolve(
                atlasPipeline(CopyShader, "vs_copy", "fs_copy")
            ));
            cmdList.SetPushGraphicsConstants(work.Get().GetReadableID());
            for(const auto& rect: stamp.rects) {
                cmdList.SetScissorRect(scissorOf(rect));
                cmdList.Draw(3);
            }
            std::vector<RHITextureBarrier> releases{
                gpu.paint.Release(SampledFragment)
            };
            if(scratchUsedAgain)
                releases.push_back(work.Release(RenderTarget));
            cmdList.EndRenderPass(releases);
        }
    }

    void PaintGpu::collectHookAcquires() {
        hookAcquires.clear();
        for(auto& surface: surfaces) {
            for(auto* texture: {&surface.paint, &surface.position}) {
                if(*texture && texture->Pending())
                    hookAcquires.push_back(*texture->Pending());
            }
        }
    }

    u32 PaintGpu::DrawSurfaces(
        RHICommandList& cmdList,
        const HookPassContext& context,
        PipelineCache& pipelines,
        ScenePush push,
        RHIIndexBufferView indices,
        std::span<const PaintSurface> all,
        std::span<const u8> active,
        std::span<const GeometryAllocation> geometry,
        const PaintDrawSettings& settings
    ) {
        // the frame's texture table: paint, position and fade per surface
        textureRows.clear();
        std::vector<u32> firstRow(all.size(), 0);
        for(usize i = 0; i < all.size(); ++i) {
            if(active[i] == 0 || i >= surfaces.size())
                continue;
            firstRow[i] = static_cast<u32>(textureRows.size()) + 1;
            auto& gpu = surfaces[i];
            const bool bare = !gpu.paint;
            textureRows.push_back(
                TextureData{
                    .texture = bare ? bareAtlas[0]->GetReadableID()
                                    : gpu.paint.Get().GetReadableID(),
                    .sampler = static_cast<u32>(TextureSampler::NearestClamp)
                }
            );
            textureRows.push_back(
                TextureData{
                    .texture = bare ? bareAtlas[1]->GetReadableID()
                                    : gpu.position.Get().GetReadableID(),
                    .sampler = static_cast<u32>(TextureSampler::NearestClamp)
                }
            );
            textureRows.push_back(
                TextureData{
                    .texture = bare ? bareAtlas[2]->GetReadableID()
                                    : gpu.edgeFade->GetReadableID(),
                    .sampler = static_cast<u32>(TextureSampler::LinearClamp)
                }
            );
        }
        if(textureRows.empty())
            return 0;

        constexpr auto Stride = static_cast<u32>(sizeof(TextureData));
        textureSlice = device.UploadTransient(
            std::span<const TextureData>(textureRows),
            Stride
        );
        push.textures = textureSlice.buffer->GetReadableID(Stride);
        push.textureBase = textureSlice.offset / Stride;

        RHIGraphicsPipelineStateDesc desc{
            .preRasterizer =
                RHILegacyFrontendDesc{
                    .vertexShader =
                        RHIShaderDesc{
                            .path = SurfaceShader,
                            .entryPoint = "vs_surface"
                        }
                },
            .rasterizer =
                RHIRasterizerState{
                    .cullMode = RHICullMode::Back,
                    .frontCounterClockwise = false
                },
            .fragmentShader =
                RHIShaderDesc{
                    .path = SurfaceShader,
                    .entryPoint = "fs_surface"
                },
            .depthStencil =
                RHIDepthStencilState{
                    .format = context.formats.depth,
                    .depthWriteEnable = true,
                    .depthFunc = RHIComparisonFunc::Less
                },
            .renderTargetCount = context.formats.colors.size(),
            .profile = "sm_6_8"
        };
        std::ranges::copy(
            context.formats.colors,
            desc.renderTargetFormats.begin()
        );
        cmdList.SetPipelineState(pipelines.Resolve(desc));
        cmdList.SetPushGraphicsConstants(push);

        u32 draws = 0;
        for(usize i = 0; i < all.size(); ++i) {
            if(firstRow[i] == 0)
                continue;
            const auto& surface = all[i];
            const auto& object = surface.Object();
            const auto& layout = surface.Layout();
            const auto& g = geometry[static_cast<usize>(object.mesh)];
            const auto& r = object.transform.rotation;

            PaintSurfaceConstants constants{
                .objectToWorld = mintToCrowy() * object.transform.ToMat4(),
                .rotation =
                    {toVec4(r.Rotate({1.0, 0.0, 0.0})),
                     toVec4(r.Rotate({0.0, 1.0, 0.0})),
                     toVec4(r.Rotate({0.0, 0.0, 1.0}))},
                .scale3D = toVec4(surface.Scale3D()),
                .boundsMin = toVec4(surface.LocalBounds().min),
                .boundsSize = toVec4(surface.LocalBounds().Size()),
                .atlas =
                    {static_cast<f32>(layout.atlasSize),
                     1.0f / static_cast<f32>(std::max(layout.atlasSize, 1)),
                     layout.texelCm,
                     PaintDistanceRange},
                .paintMap = firstRow[i],
                .positionMap = firstRow[i] + 1,
                .edgeFadeMap = firstRow[i] + 2,
                .vbIndex = static_cast<u32>(g.baseVertex),
                .view = settings.view,
                .compareView = settings.compareView,
                .selected = static_cast<u32>(i),
                .screen = {settings.splitPixels, 0.0f, 0.0f, 0.0f},
                .lookStyle = settings.lookStyle,
                .lookStyle2 = settings.lookStyle2
            };
            std::ranges::copy(settings.lookModes, constants.lookModes);
            std::ranges::copy(settings.lookModes2, constants.lookModes2);
            // rotation rows: the columns above are the rotated axes
            const auto c0 = constants.rotation[0];
            const auto c1 = constants.rotation[1];
            const auto c2 = constants.rotation[2];
            constants.rotation[0] = {c0.x, c1.x, c2.x, 0.0f};
            constants.rotation[1] = {c0.y, c1.y, c2.y, 0.0f};
            constants.rotation[2] = {c0.z, c1.z, c2.z, 0.0f};
            for(const auto& island: layout.islands) {
                constants.islands[static_cast<usize>(island.direction)] =
                    island.ToShaderParam(layout.atlasSize);
            }
            if(settings.showsCells && surface.Cells().IsBuilt()) {
                const auto& cells = surface.Cells();
                cellRows.assign(cells.Ids().begin(), cells.Ids().end());
                const auto slice = device.UploadTransient(
                    std::span<const u32>(cellRows),
                    static_cast<u32>(sizeof(u32))
                );
                const auto& dims = cells.Dims();
                constants.cellIds = slice.buffer->GetReadableID(sizeof(u32));
                constants.cellBase =
                    slice.offset / static_cast<u32>(sizeof(u32));
                constants.cellOrigin = toVec4(cells.Origin(), cells.CellSize());
                constants.cellDims[0] = static_cast<u32>(dims[0]);
                constants.cellDims[1] = static_cast<u32>(dims[1]);
                constants.cellDims[2] = static_cast<u32>(dims[2]);
                constants.cellDims[3] = 1;
            }

            cmdList.SetGraphicsConstantBuffer(
                device.UploadTransient(constants),
                1
            );
            cmdList.DrawIndexed(indices, g.indexCount, 1, g.firstIndex, 0);
            ++draws;
        }

        return draws;
    }

    u32 PaintGpu::DrawShapeLab(
        RHICommandList& cmdList,
        const HookPassContext& context,
        PipelineCache& pipelines,
        const PaintShapeLabPush& push
    ) {
        RHIGraphicsPipelineStateDesc desc{
            .preRasterizer =
                RHILegacyFrontendDesc{
                    .vertexShader =
                        RHIShaderDesc{
                            .path = ShapeLabShader,
                            .entryPoint = "vs_shapelab"
                        }
                },
            .rasterizer = RHIRasterizerState{.cullMode = RHICullMode::None},
            .fragmentShader =
                RHIShaderDesc{
                    .path = ShapeLabShader,
                    .entryPoint = "fs_shapelab"
                },
            .renderTargetCount = context.formats.colors.size(),
            .profile = "sm_6_8"
        };
        std::ranges::copy(
            context.formats.colors,
            desc.renderTargetFormats.begin()
        );
        const auto& rect = push.rect;

        cmdList.SetPipelineState(pipelines.Resolve(desc));
        cmdList.SetViewport(
            RHIViewport{rect.x, rect.y, rect.z, rect.w, 0.0f, 1.0f}
        );
        cmdList.SetScissorRect(
            RHIScissorRect{
                static_cast<i32>(rect.x),
                static_cast<i32>(rect.y),
                static_cast<i32>(rect.x + rect.z),
                static_cast<i32>(rect.y + rect.w)
            }
        );
        cmdList.SetPushGraphicsConstants(push);
        cmdList.Draw(3);

        return 1;
    }

    u32 PaintGpu::DrawPanel(
        RHICommandList& cmdList,
        const HookPassContext& context,
        PipelineCache& pipelines,
        const PaintSurface& surface,
        usize index,
        u32 channel,
        Vec4 rect,
        Vec4 profile
    ) {
        if(index >= surfaces.size() || !surfaces[index].paint)
            return 0;
        auto& gpu = surfaces[index];
        const auto& layout = surface.Layout();

        RHIGraphicsPipelineStateDesc desc{
            .preRasterizer =
                RHILegacyFrontendDesc{
                    .vertexShader =
                        RHIShaderDesc{
                            .path = PanelShader,
                            .entryPoint = "vs_panel"
                        }
                },
            .rasterizer = RHIRasterizerState{.cullMode = RHICullMode::None},
            .fragmentShader =
                RHIShaderDesc{.path = PanelShader, .entryPoint = "fs_panel"},
            .renderTargetCount = context.formats.colors.size(),
            .profile = "sm_6_8"
        };
        std::ranges::copy(
            context.formats.colors,
            desc.renderTargetFormats.begin()
        );

        PaintPanelPush push{
            .paint = gpu.paint.Get().GetReadableID(),
            .position = gpu.position.Get().GetReadableID(),
            .edgeFade = gpu.edgeFade->GetReadableID(),
            .channel = channel,
            .atlasSize = gpu.atlasSize,
            .rect = rect,
            .profile = profile
        };
        for(const auto& island: layout.islands) {
            push.islandRects[static_cast<usize>(island.direction)] = Vec4{
                static_cast<f32>(island.rect.min.x),
                static_cast<f32>(island.rect.min.y),
                static_cast<f32>(island.rect.max.x),
                static_cast<f32>(island.rect.max.y)
            };
        }

        cmdList.SetPipelineState(pipelines.Resolve(desc));
        cmdList.SetViewport(
            RHIViewport{rect.x, rect.y, rect.z, rect.w, 0.0f, 1.0f}
        );
        cmdList.SetScissorRect(
            RHIScissorRect{
                static_cast<i32>(rect.x),
                static_cast<i32>(rect.y),
                static_cast<i32>(rect.x + rect.z),
                static_cast<i32>(rect.y + rect.w)
            }
        );
        cmdList.SetPushGraphicsConstants(push);
        cmdList.Draw(3);

        return 1;
    }
}
