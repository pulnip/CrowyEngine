#include "PostChain.hpp"

#include <algorithm>
#include <array>
#include <format>
#include <stdexcept>
#include <utility>

#include "Assert.hpp"

namespace Crowy
{
    namespace
    {
        // FullscreenPush carries the source and three more reads
        constexpr usize MaxPostInputs = 3;
        // Scene and Display
        constexpr usize OutputClassCount = 2;
        // ping and pong
        constexpr usize IntermediatesPerClass = 2;

        using Reads = std::vector<FrameTargetID>;

        [[noreturn]] void refuse(StrView entry, StrView rule) {
            throw std::invalid_argument(
                std::format("post entry '{}': {}", entry, rule)
            );
        }

        Str targetNames(const FramePipelineDesc& desc) {
            Str names;
            for(const auto& target: desc.targets) {
                if(!names.empty())
                    names += ", ";
                names += "'" + target.name + "'";
            }

            return names;
        }

        // every entry's inputs as IDs, after the rules the list keeps
        std::vector<Reads> resolveInputs(
            const FramePipelineDesc& desc,
            std::span<const PostPassDesc> post
        ) {
            std::vector<Reads> inputs;
            const PostPassDesc* firstDisplay = nullptr;
            for(const auto& entry: post) {
                if(entry.output == PostOutput::Display &&
                   firstDisplay == nullptr)
                    firstDisplay = &entry;
                if(entry.output == PostOutput::Scene &&
                   firstDisplay != nullptr) {
                    refuse(
                        entry.name,
                        std::format(
                            "it outputs Scene after '{}' output Display; the "
                            "chain crosses into display values once",
                            firstDisplay->name
                        )
                    );
                }
                if(entry.inputs.size() > MaxPostInputs) {
                    refuse(
                        entry.name,
                        std::format(
                            "it names {} inputs; a post entry reads at most {} "
                            "besides its source",
                            entry.inputs.size(),
                            MaxPostInputs
                        )
                    );
                }

                Reads ids;
                for(const auto& name: entry.inputs) {
                    const auto found = std::ranges::find(
                        desc.targets,
                        name,
                        &FrameTargetDesc::name
                    );
                    if(found == desc.targets.end()) {
                        refuse(
                            entry.name,
                            std::format(
                                "input '{}' is not a target; the targets are "
                                "{}",
                                name,
                                targetNames(desc)
                            )
                        );
                    }
                    ids.push_back(
                        static_cast<FrameTargetID>(
                            found - desc.targets.begin() + 1
                        )
                    );
                }
                inputs.push_back(std::move(ids));
            }

            if(post.back().output != PostOutput::Display) {
                refuse(
                    post.back().name,
                    "it is last and outputs Scene; the last entry writes "
                    "display values to the back buffer"
                );
            }

            return inputs;
        }
    }

    void appendPostChain(
        FramePipelineDesc& desc,
        FrameTargetID sceneColor,
        std::span<const PostPassDesc> post,
        FrameTargetID output
    ) {
        if(post.empty()) {
            throw std::invalid_argument(
                "the post list is empty; its last entry writes the back buffer"
            );
        }
        CROWY_ASSERT(
            sceneColor != BackBufferTarget && sceneColor <= desc.targets.size(),
            "the post chain reads scene color from target {}, one of the "
            "desc's",
            sceneColor
        );
        CROWY_ASSERT(
            output <= desc.targets.size(),
            "the post chain writes target {}, the back buffer or one of the "
            "desc's",
            output
        );
        CROWY_ASSERT(
            output != sceneColor,
            "the post chain writes target {}, not scene color, its source",
            output
        );
        // the entries load the texel under their pixel
        const auto& sceneTarget = desc.targets[sceneColor - 1];
        if(output != BackBufferTarget) {
            const auto& written = desc.targets[output - 1];
            if(written.size != sceneTarget.size) {
                throw std::invalid_argument(
                    std::format(
                        "the post chain writes '{}', whose size is not scene "
                        "color's",
                        written.name
                    )
                );
            }
        }
        // every rule before the first append, so a refused list adds nothing
        const auto inputs = resolveInputs(desc, post);

        // copies: the appends below move desc.targets
        const auto sceneFormat = sceneTarget.format;
        const auto sceneSize = sceneTarget.size;
        // per class, the intermediates added so far; 0 for one not yet added
        std::array<
            std::array<FrameTargetID, IntermediatesPerClass>,
            OutputClassCount>
            intermediates{};

        auto source = sceneColor;
        for(usize i = 0; i < post.size(); ++i) {
            const auto& entry = post[i];

            auto written = output;
            if(i + 1 < post.size()) {
                const bool scene = entry.output == PostOutput::Scene;
                auto& slots = intermediates[static_cast<usize>(entry.output)];
                // source is never 0, so an unadded slot is never it
                const usize slot = source == slots[0] ? 1 : 0;
                if(slots[slot] == 0) {
                    desc.targets.push_back(
                        FrameTargetDesc{
                            .name = std::format(
                                "Post{}{}",
                                scene ? "Scene" : "Display",
                                slot
                            ),
                            .format = scene ? sceneFormat
                                            : RHIPixelFormat::RGBA8_UNORM,
                            .size = sceneSize
                        }
                    );
                    slots[slot] =
                        static_cast<FrameTargetID>(desc.targets.size());
                }
                written = slots[slot];
            }

            Reads reads{source};
            reads.append_range(inputs[i]);
            desc.passes.push_back(
                PassDesc{
                    .name = entry.name,
                    .colors = {ColorTargetUse{.target = written}},
                    .reads = std::move(reads),
                    .kind = FullscreenPassDesc{
                        .fragmentShader = entry.fragmentShader,
                        .params = entry.params
                    }
                }
            );
            source = written;
        }
    }

    PostPassDesc tonemapPass() {
        return PostPassDesc{
            .name = "Tonemap",
            .fragmentShader = {
                .path = "Engine/Render/Shader/Tonemap.slang",
                .entryPoint = "fs_main"
            }
        };
    }

    PostPassDesc presentPass() {
        return PostPassDesc{
            .name = "Present",
            .fragmentShader = {
                .path = "Engine/Render/Shader/Present.slang",
                .entryPoint = "fs_main"
            }
        };
    }
}
