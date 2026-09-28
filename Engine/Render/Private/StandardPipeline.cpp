#include "StandardPipeline.hpp"

namespace Crowy
{
    FramePipelineDesc makeStandardPipeline(
        const StandardPipelineConfig& config
    ) {
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
                    .target = BackBufferTarget,
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

        // the last pass on the back buffer, so the UI rides it
        desc.passes.push_back(
            PassDesc{
                .name = "Translucent",
                .colors = {ColorTargetUse{
                    .target = BackBufferTarget,
                    .load = RHILoadAction::Load,
                    .store = RHIStoreAction::Store
                }},
                .depth =
                    DepthTargetUse{
                        .target = SceneDepth,
                        .load = RHILoadAction::Load,
                        .store = RHIStoreAction::DontCare
                    },
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

        return desc;
    }
}
