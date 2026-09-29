#include "StandardPipeline.hpp"

#include <vector>

#include "SceneRenderer.hpp"

namespace Crowy
{
    FramePipelineDesc makeStandardPipeline(
        const StandardPipelineConfig& config
    ) {
        // targets[i] is ID i + 1: SceneDepth stays 1, which VertexPullSpike
        // names
        constexpr FrameTargetID SceneDepth = 1;

        const DrawFilter opaque{.domains = MaterialDomain::Opaque};
        const bool prepass = config.depthPrepass;

        FramePipelineDesc desc{
            .targets = {FrameTargetDesc{
                .name = "SceneDepth",
                .format = config.depthFormat,
                .clearDepth = 1.0f
            }}
        };

        // what the color passes sample: the map, when there is one
        std::vector<FrameTargetID> shadowReads;
        if(config.shadowMapSize > 0) {
            desc.targets.push_back(
                FrameTargetDesc{
                    .name = "ShadowMap",
                    .format = RHIPixelFormat::D32_FLOAT,
                    .width = config.shadowMapSize,
                    .height = config.shadowMapSize
                }
            );
            desc.shadowMap = static_cast<FrameTargetID>(desc.targets.size());
            shadowReads = {desc.shadowMap};

            // no colors: depth-only, the viewport from the map; near first
            // along the light
            desc.passes.push_back(
                PassDesc{
                    .name = "Shadow",
                    .depth = DepthTargetUse{.target = desc.shadowMap},
                    .kind = MeshPassDesc{
                        .view = SceneRenderer::ShadowView,
                        .filter =
                            DrawFilter{
                                .domains = MaterialDomain::Opaque,
                                .required = PrimitiveFlags::CastShadow
                            },
                        .order = DrawOrder::PipelineThenNearFirst,
                        .state =
                            MeshPassState{
                                .depthFunc = RHIComparisonFunc::Less,
                                .depthWrite = true
                            },
                        .drawCapacity = config.drawCapacity
                    }
                }
            );
        }

        // after the map, so SceneDepth keeps ID 1 either way
        desc.targets.push_back(
            FrameTargetDesc{
                .name = "SceneColor",
                .format = config.sceneColorFormat,
                .clearColor = config.clearColor
            }
        );
        desc.sceneColor = static_cast<FrameTargetID>(desc.targets.size());

        if(prepass) {
            desc.passes.push_back(
                PassDesc{
                    .name = "DepthPrepass",
                    .depth =
                        DepthTargetUse{
                            .target = SceneDepth,
                            .load = RHILoadAction::Clear,
                            .store = RHIStoreAction::Store
                        },
                    .kind = MeshPassDesc{
                        .filter = opaque,
                        .order = DrawOrder::PipelineThenNearFirst,
                        .state =
                            MeshPassState{
                                .depthFunc = RHIComparisonFunc::Less,
                                .depthWrite = true
                            },
                        .drawCapacity = config.drawCapacity
                    }
                }
            );
        }

        // Equal after the prepass: the same vertex shader lands on the same
        // depths, so every pixel shades once
        desc.passes.push_back(
            PassDesc{
                .name = "Opaque",
                .colors = {ColorTargetUse{
                    .target = desc.sceneColor,
                    .load = RHILoadAction::Clear,
                    .store = RHIStoreAction::Store
                }},
                .depth =
                    DepthTargetUse{
                        .target = SceneDepth,
                        .load = prepass ? RHILoadAction::Load
                                        : RHILoadAction::Clear,
                        .store = RHIStoreAction::Store
                    },
                .reads = shadowReads,
                .kind = MeshPassDesc{
                    .filter = opaque,
                    .order = DrawOrder::PipelineThenNearFirst,
                    .state =
                        MeshPassState{
                            .depthFunc = prepass ? RHIComparisonFunc::Equal
                                                 : RHIComparisonFunc::Less,
                            .depthWrite = !prepass
                        },
                    .drawCapacity = config.drawCapacity
                }
            }
        );

        // blended over the opaque scene in scene colour, before the post list
        desc.passes.push_back(
            PassDesc{
                .name = "Translucent",
                .colors = {ColorTargetUse{
                    .target = desc.sceneColor,
                    .load = RHILoadAction::Load,
                    .store = RHIStoreAction::Store
                }},
                .depth =
                    DepthTargetUse{
                        .target = SceneDepth,
                        .load = RHILoadAction::Load,
                        .store = RHIStoreAction::DontCare
                    },
                .reads = shadowReads,
                .kind = MeshPassDesc{
                    .filter =
                        DrawFilter{.domains = MaterialDomain::Translucent},
                    .order = DrawOrder::FarFirst,
                    .state =
                        MeshPassState{
                            .depthFunc = RHIComparisonFunc::Less,
                            .depthWrite = false
                        },
                    .drawCapacity = config.drawCapacity
                }
            }
        );

        // the last entry writes the back buffer, so the UI rides it
        appendPostChain(desc, desc.sceneColor, config.post);

        return desc;
    }
}
