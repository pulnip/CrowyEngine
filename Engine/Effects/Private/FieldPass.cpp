#include "FieldPass.hpp"

#include <algorithm>

#include "Assert.hpp"
#include "RHIBuffer.hpp"
#include "RHIDevice.hpp"

namespace Crowy
{
    namespace
    {
        // the words of FieldClear.slang's push
        struct ClearPush {
            u64 words = 0;
            u32 count = 0;
            // the threads dispatched, each striding over the words by it
            u32 stride = 0;
        };
        static_assert(sizeof(ClearPush) == 16);
    }

    FieldKernels::~FieldKernels() = default;

    FieldKernels::FieldKernels(RHIDevice& device)
        : clear(
              device,
              RHIShaderDesc{
                  .path = "Engine/Effects/Shader/FieldClear.slang",
                  .entryPoint = "cs_clear"
              }
          ) {}

    FieldPass::~FieldPass() {
        CROWY_ASSERT(!open, "a field pass ends before it is destroyed");
    }

    FieldPass::FieldPass(RHICommandList& cmdList, FieldKernels& kernels)
        : cmdList(cmdList),
          kernels(kernels) {}

    void FieldPass::Begin(FieldBuffers fields) {
        CROWY_ASSERT(!open, "a field pass ends before it begins again");

        this->fields.assign(fields.begin(), fields.end());
        pending.assign(this->fields.size(), false);
        scratch.clear();
        for(const auto* field: this->fields)
            scratch.push_back(field->AcquireForWrite());
        cmdList.BeginComputePass({}, scratch);
        open = true;
    }

    void FieldPass::Clear(FieldBuffer& field) {
        const auto words = field.Bytes() / 4;
        const ClearPush push{
            .words = field.Buffer().GetWritableID(4u),
            .count = words,
            .stride = std::min(words, MaxDispatchThreads)
        };
        FieldBuffer* const touches[] = {&field};
        dispatch(
            kernels.Clear(),
            &push,
            sizeof(push),
            Size3D{push.stride, 1, 1},
            touches
        );
    }

    std::span<const RHIBufferBarrier> FieldPass::End() {
        CROWY_ASSERT(open, "a field pass begins before it ends");

        releases.clear();
        for(auto* field: fields)
            releases.push_back(field->Release());
        cmdList.EndComputePass({}, releases);
        open = false;

        return releases;
    }

    void FieldPass::dispatch(
        ComputeKernel& kernel,
        const void* push,
        u32 size,
        Size3D threads,
        FieldBuffers touches
    ) {
        CROWY_ASSERT(open, "a dispatch runs inside its field pass");

        scratch.clear();
        for(usize t = 0; t < touches.size(); ++t) {
            CROWY_ASSERT(
                std::ranges::find(touches.first(t), touches[t]) ==
                    touches.first(t).end(),
                "a dispatch names each field it touches once"
            );
            if(pending[indexOf(*touches[t])]) {
                scratch.push_back(MakeBarrier(
                    touches[t]->Buffer(),
                    RHIResourceUsage::StorageCompute,
                    RHIResourceUsage::StorageCompute
                ));
            }
        }
        if(!scratch.empty())
            cmdList.DispatchBarrier({}, scratch);
        for(const auto* field: touches)
            pending[indexOf(*field)] = true;

        cmdList.SetPipelineState(kernel.Pipeline());
        cmdList.SetPushComputeConstants(push, size);
        cmdList.Dispatch(threads);
    }

    usize FieldPass::indexOf(const FieldBuffer& field) const {
        const auto found = std::ranges::find(fields, &field);
        CROWY_ASSERT(
            found != fields.end(),
            "a dispatch touches its pass's fields"
        );

        return static_cast<usize>(found - fields.begin());
    }
}
