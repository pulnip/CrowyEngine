#include <array>
#include <cstddef>
#include <exception>
#include <print>
#include <span>
#include <vector>

#include "RHIBuffer.hpp"
#include "RHICommandList.hpp"
#include "RHIDevice.hpp"

namespace
{
    using namespace Crowy;

    constexpr u32 PatternWords = 1024;
    constexpr u32 PatternBytes = PatternWords * sizeof(u32);

    // one more than the staging holds, so a flush happens and the allocation
    // after it wraps back over offset 0, where the slice sits
    constexpr u32 FillerBytes = 8u << 20;
    constexpr u32 FillerCount = UploadStagingBytes / FillerBytes + 1;

    constexpr u32 patternWord(u32 i) noexcept {
        return 0xC0DE0000u ^ i;
    }
}

int main(void) {
    try {
        using namespace Crowy;

        auto device = createDevice();
        auto cmdList = device->CreateCommandList();

        auto readback = device->CreateBuffer(
            RHIBufferCreateDesc{
                .size = PatternBytes,
                .memory = RHIMemoryType::CPURead
            },
            "UploadFlushReadback"
        );

        std::array<u32, PatternWords> pattern{};
        for(u32 i = 0; i < PatternWords; ++i)
            pattern[i] = patternWord(i);

        cmdList->Begin();

        // allocated before any creation upload, so it sits where the wrap lands
        const auto slice =
            device->UploadTransient(std::span<const u32>(pattern));

        cmdList->BeginBlitPass();
        cmdList->Copy(*slice.buffer, *readback, slice.offset, 0, PatternBytes);
        cmdList->EndBlitPass();

        const std::vector<std::byte> fillerData(FillerBytes, std::byte{0x5A});
        std::vector<RHIBufferRAII> fillers;
        for(u32 i = 0; i < FillerCount; ++i) {
            fillers.push_back(device->CreateBuffer(
                RHIBufferCreateDesc{
                    .size = FillerBytes,
                    .initialData = fillerData.data()
                },
                "UploadFlushFiller"
            ));
        }

        cmdList->Close();
        RHICommandList* cmdLists[] = {cmdList.get()};
        // the flush took a serial of its own, but no frame number
        device->Submit(cmdLists, 1);
        device->WaitFrame(1);
        std::array<u32, PatternWords> result{};
        readback->Download(result.data(), PatternBytes);

        u32 mismatches = 0;
        u32 firstMismatch = 0;
        for(u32 i = 0; i < PatternWords; ++i) {
            if(result[i] == pattern[i])
                continue;

            if(mismatches == 0)
                firstMismatch = i;
            ++mismatches;
        }

        std::println(
            "UploadFlushCheck: {} filler uploads of {} MiB behind one {}-byte "
            "slice",
            FillerCount,
            FillerBytes >> 20,
            PatternBytes
        );

        if(mismatches > 0) {
            std::println(
                "  FAIL: {} of {} words changed before the copy ran - "
                "the flush let the slice be reused",
                mismatches,
                PatternWords
            );
            std::println(
                "  first at word {}: 0x{:08X}, expected 0x{:08X}",
                firstMismatch,
                result[firstMismatch],
                pattern[firstMismatch]
            );

            return 1;
        }

        std::println("Succeed!");
    }
    catch(const std::exception& e) {
        std::println("Exception: {}", e.what());

        return 1;
    }

    return 0;
}
