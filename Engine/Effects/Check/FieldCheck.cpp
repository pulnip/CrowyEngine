#include <algorithm>
#include <array>
#include <exception>
#include <format>
#include <print>
#include <span>
#include <vector>

#include "ComputeKernel.hpp"
#include "EffectRandom.hpp"
#include "FieldBuffer.hpp"
#include "FieldPass.hpp"
#include "RHIBuffer.hpp"
#include "RHICommandList.hpp"
#include "RHIDevice.hpp"

// Steps fields on the device and holds them against the CPU: the clear, a
// field written across submissions, the hash effects draw from.

namespace
{
    using namespace Crowy;
    using Words = std::vector<u32>;
    using Readbacks = std::span<RHIBuffer* const>;

    constexpr CStr CheckShader = "Engine/Effects/Check/FieldCheck.slang";
    constexpr u32 Count = 4096;

    // the frames this program submits, each drained before the next
    void submitAndWait(RHIDevice& device, RHICommandList& cmdList) {
        static u64 frame = 0;
        RHICommandList* lists[] = {&cmdList};
        device.Submit(lists, ++frame);
        device.WaitFrame(frame);
    }

    // a u32 field whose readers copy it out
    FieldBuffer wordField(RHIDevice& device, u32 count, StrView name) {
        return FieldBuffer(device, count, 4, name, RHIResourceUsage::CopySrc);
    }

    // a blit pass taking the field pass's releases, copying each field out
    void recordReadback(
        RHICommandList& cmdList,
        std::span<const RHIBufferBarrier> releases,
        FieldBuffers fields,
        Readbacks readbacks
    ) {
        cmdList.BeginBlitPass({}, releases);
        for(usize i = 0; i < fields.size(); ++i)
            cmdList.Copy(
                fields[i]->Buffer(),
                *readbacks[i],
                0,
                0,
                fields[i]->Bytes()
            );
        cmdList.EndBlitPass();
    }

    Words download(RHIBuffer& readback, u32 count) {
        Words words(count);
        readback.Download(words.data(), count * 4);

        return words;
    }

    bool report(CStr label, bool passed, Str detail) {
        std::println("  {}: {} ({})", label, passed ? "ok" : "FAIL", detail);

        return passed;
    }

    // FieldCheck.slang's push
    struct CheckPush {
        u64 words = 0;
        u32 count = 0;
        u32 value = 0;
    };

    class Checks {
    private:
        RHIDevice& device;
        RHICommandListRAII cmdList;
        FieldKernels kernels;
        ComputeKernel fill;
        ComputeKernel add;
        ComputeKernel hash;

    public:
        explicit Checks(RHIDevice& device)
            : device(device),
              cmdList(device.CreateCommandList()),
              kernels(device),
              fill(device, {.path = CheckShader, .entryPoint = "cs_fill"}),
              add(device, {.path = CheckShader, .entryPoint = "cs_add"}),
              hash(device, {.path = CheckShader, .entryPoint = "cs_hash"}) {}

        // one field filled then cleared, one only filled, in one pass
        bool Clear() {
            constexpr u32 Filler = 0xDEADBEEFu;
            auto cleared = wordField(device, Count, "FieldCheck.cleared");
            auto filled = wordField(device, Count, "FieldCheck.filled");
            FieldBuffer* const fields[] = {&cleared, &filled};
            FieldBuffer* const onlyCleared[] = {&cleared};
            FieldBuffer* const onlyFilled[] = {&filled};
            auto clearedBack = readbackOf(Count);
            auto filledBack = readbackOf(Count);
            RHIBuffer* const readbacks[] = {
                clearedBack.get(),
                filledBack.get()
            };

            cmdList->Begin();
            FieldPass pass(*cmdList, kernels);
            pass.Begin(fields);
            pass.Dispatch(
                fill,
                push(cleared, Filler),
                {Count, 1, 1},
                onlyCleared
            );
            pass.Dispatch(
                fill,
                push(filled, Filler),
                {Count, 1, 1},
                onlyFilled
            );
            pass.Clear(cleared);
            recordReadback(*cmdList, pass.End(), fields, readbacks);
            cmdList->Close();
            submitAndWait(device, *cmdList);

            const auto zeros = download(*clearedBack, Count);
            const auto filledWords = download(*filledBack, Count);
            u32 wrong = 0;
            for(u32 i = 0; i < Count; ++i)
                wrong += (zeros[i] != 0) + (filledWords[i] != Filler);

            return report(
                "clear",
                wrong == 0,
                std::format("{} of {} words wrong", wrong, 2 * Count)
            );
        }

        // two adds a frame over seven frames, each frame's acquire across
        // the submission before it
        bool Accumulate() {
            constexpr u32 Frames = 7;
            constexpr u32 Slots = 1000;
            auto field = wordField(device, Slots, "FieldCheck.accumulated");
            FieldBuffer* const fields[] = {&field};
            auto back = readbackOf(Slots);
            RHIBuffer* const readbacks[] = {back.get()};

            for(u32 frame = 0; frame < Frames; ++frame) {
                cmdList->Begin();
                FieldPass pass(*cmdList, kernels);
                pass.Begin(fields);
                if(frame == 0)
                    pass.Clear(field);
                pass.Dispatch(add, push(field, 1), {Slots, 1, 1}, fields);
                pass.Dispatch(add, push(field, 1), {Slots, 1, 1}, fields);
                const auto releases = pass.End();
                if(frame + 1 == Frames)
                    recordReadback(*cmdList, releases, fields, readbacks);
                cmdList->Close();
                submitAndWait(device, *cmdList);
            }

            const auto words = download(*back, Slots);
            u32 wrong = 0;
            for(const auto word: words)
                wrong += word != 2 * Frames;

            return report(
                "accumulate",
                wrong == 0,
                std::format("{} of {} slots not {}", wrong, Slots, 2 * Frames)
            );
        }

        // the GPU's effectHash against the CPU's, bit for bit
        bool Hash() {
            constexpr u32 Seed = 1;
            auto field = wordField(device, Count, "FieldCheck.hashed");
            FieldBuffer* const fields[] = {&field};
            auto back = readbackOf(Count);
            RHIBuffer* const readbacks[] = {back.get()};

            cmdList->Begin();
            FieldPass pass(*cmdList, kernels);
            pass.Begin(fields);
            pass.Dispatch(hash, push(field, Seed), {Count, 1, 1}, fields);
            recordReadback(*cmdList, pass.End(), fields, readbacks);
            cmdList->Close();
            submitAndWait(device, *cmdList);

            const auto words = download(*back, Count);
            u32 wrong = 0;
            for(u32 i = 0; i < Count; ++i)
                wrong += words[i] != effectHash(Seed, i, 0, 0);

            return report(
                "hash",
                wrong == 0,
                std::format("{} of {} draws differ", wrong, Count)
            );
        }

    private:
        static CheckPush push(FieldBuffer& field, u32 value) {
            return CheckPush{
                .words = field.Writable(),
                .count = field.Count(),
                .value = value
            };
        }

        RHIBufferRAII readbackOf(u32 count) {
            return device.CreateBuffer(
                RHIBufferCreateDesc{
                    .size = count * 4,
                    .memory = RHIMemoryType::CPURead
                },
                "FieldCheck.readback"
            );
        }
    };
}

int main() {
    try {
        auto device = Crowy::CreateDevice();
        Checks checks(*device);

        std::println("FieldCheck");
        const std::array results{
            checks.Clear(),
            checks.Accumulate(),
            checks.Hash()
        };
        const auto passed = std::ranges::count(results, true);
        std::println("FieldCheck: {} of {} passed", passed, results.size());

        return static_cast<usize>(passed) == results.size() ? 0 : 1;
    } catch(const std::exception& e) {
        std::println(stderr, "FieldCheck: {}", e.what());

        return 1;
    }
}
