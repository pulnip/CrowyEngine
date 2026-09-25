#include <array>
#include <cstddef>
#include <exception>
#include <print>
#include <span>
#include <vector>

#include "RHIBuffer.hpp"
#include "RHICommandList.hpp"
#include "RHIDevice.hpp"

// Records a copy out of a transient slice, then creates enough initial data
// to flush and wrap the creation-upload staging before the frame goes out.
// The slice has to live until the batch that copies it completes, so the copy
// must read back exactly what was written, whatever the flush did meanwhile.

namespace
{
    using namespace Crowy;

    constexpr u32 PatternWords = 1024;
    constexpr u32 PatternBytes = PatternWords * sizeof(u32);

    // five of these overrun 32 MiB of staging, so a flush has to happen and the
    // allocation after it wraps back over offset 0, where the slice sits
    constexpr u32 FillerBytes = 8u << 20;
    constexpr u32 FillerCount = 5;

    constexpr u32 patternWord(u32 i) noexcept { return 0xC0DE0000u ^ i; }
}

int main(void){
    try{
        using namespace Crowy;

        auto device = CreateDevice();
        auto cmdList = device->CreateCommandList();

        auto readback = device->CreateBuffer(RHIBufferCreateDesc{
            .size = PatternBytes,
            .memory = RHIMemoryType::CPURead
        }, "UploadFlushReadback");

        std::array<u32, PatternWords> pattern{};
        for(u32 i = 0; i < PatternWords; ++i)
            pattern[i] = patternWord(i);

        cmdList->Begin();

        // allocated before any creation upload, so it sits where the wrap lands
        const auto slice = device->UploadTransient(std::span<const u32>(pattern));

        cmdList->BeginBlitPass();
        cmdList->Copy(*slice.buffer, *readback, slice.offset, 0, PatternBytes);
        cmdList->EndBlitPass();

        const std::vector<std::byte> fillerData(FillerBytes, std::byte{0x5A});
        std::vector<RHIBufferRAII> fillers;
        for(u32 i = 0; i < FillerCount; ++i){
            fillers.push_back(device->CreateBuffer(RHIBufferCreateDesc{
                .size = FillerBytes,
                .initialData = fillerData.data()
            }, "UploadFlushFiller"));
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
        for(u32 i = 0; i < PatternWords; ++i){
            if(result[i] == pattern[i])
                continue;

            if(mismatches == 0)
                firstMismatch = i;
            ++mismatches;
        }

        std::println(
            "UploadFlushCheck: {} filler uploads of {} MiB behind one {}-byte slice",
            FillerCount, FillerBytes >> 20, PatternBytes
        );

        if(mismatches > 0){
            std::println(
                "  FAIL: {} of {} words changed before the copy ran - "
                "the flush let the slice be reused",
                mismatches, PatternWords
            );
            std::println(
                "  first at word {}: 0x{:08X}, expected 0x{:08X}",
                firstMismatch, result[firstMismatch], pattern[firstMismatch]
            );

            return 1;
        }

        std::println("Succeed!");
    }
    catch(const std::exception& e){
        std::println("Exception: {}", e.what());

        return 1;
    }

    return 0;
}
