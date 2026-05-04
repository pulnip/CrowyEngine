#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <random>
#include <vector>
#include "int_math.hpp"
#include "RHIDefinitions.hpp"
#include "RHIDevice.hpp"
#include "Renderer.hpp"
#include "RenderSpec.hpp"

using namespace Crowy;

constexpr auto N = 10000u;

// HG = Histogram
// PS = Prefix Sum
constexpr auto HG_GROUPSIZE = 16u;
constexpr auto HG_NUMGROUP = ceil_div(N, HG_GROUPSIZE);
constexpr auto HG_N = HG_GROUPSIZE * HG_NUMGROUP;

constexpr auto PS_GROUPSIZE = 1024u;
constexpr auto PS_BLOCK = 2*PS_GROUPSIZE;

constexpr auto HG_PADDED_N = next_mul(HG_N, PS_BLOCK);
constexpr auto PS_NUMGROUP = HG_PADDED_N / PS_BLOCK;

auto makeRadixPass(
    uint32_t bitOffset,
    const std::string& keysIn,
    const std::string& keysOut,
    const std::string& idxIn,
    const std::string& idxOut
){
    auto tag = std::to_string(bitOffset);

    return std::vector{
        ComputePassSpec{
            .name = "Histogram_" + tag,
            .cs = {
                .buffers = {
                    {.slot = "keys", .name = keysIn},
                    {.slot = "histogram", .name = "Histogram"}
                },
                .bytes = {
                    {.slot = "bit_offset", .data = bitOffset},
                    {.slot = "count", .data = N},
                    {.slot = "num_groups", .data = HG_NUMGROUP}
                }
            },
            .shader = {
            #ifdef CROWY_METALRHI
                .filePath = "asset/Shaders/radix_sort.metal",
                .funcName = "cs_histogram",
            #elif CROWY_D3DRHI
                .filePath = L"asset/Shaders/radix_sort.hlsl",
                .funcName = "cs_histogram",
            #endif
            },
            .gridSize = {.x=HG_N, .y=1, .z=1},
            .threadGroupSize = RHISize3D{
                .x=HG_GROUPSIZE,
                .y=1,
                .z=1
            }
        },
        ComputePassSpec{
            .name = "LocalPrefixSum_" + tag,
            .cs = {
                .buffers = {
                    {.slot = "data", .name = "Histogram"},
                    {.slot = "io_buf", .name = "PrefixSum"},
                    {.slot = "group_sums", .name = "GroupSums"}
                }
            },
            .shader = {
            #ifdef CROWY_METALRHI
                .filePath = "asset/Shaders/prefix_sum.metal",
                .funcName = "cs_prefix_sum",
            #elif CROWY_D3DRHI
                .filePath = L"asset/Shaders/prefix_sum.hlsl",
                .funcName = "cs_prefix_sum",
            #endif
            },
            .gridSize = {.x=HG_PADDED_N/2, .y=1, .z=1},
            .threadGroupSize = RHISize3D{
                .x=std::min(HG_PADDED_N/2u, 1024u),
                .y=1,
                .z=1
            }
        },
        ComputePassSpec{
            .name = "GroupPrefixSum_" + tag,
            .cs = {
                .buffers = {
                    {.slot = "io_buf", .name = "GroupSums"}
                }
            },
            .shader = {
            #ifdef CROWY_METALRHI
                .filePath = "asset/Shaders/prefix_sum.metal",
                .funcName = "cs_prefix_sum_single",
            #elif CROWY_D3DRHI
                .filePath = L"asset/Shaders/prefix_sum.hlsl",
                .funcName = "cs_prefix_sum_single",
            #endif
            },
            .gridSize = {.x=next_pow2(PS_NUMGROUP)/2u, .y=1u, .z=1u},
            .threadGroupSize = RHISize3D{
                .x=std::min(next_pow2(PS_NUMGROUP)/2u, 1024u),
                .y=1,
                .z=1
            }
        },
        ComputePassSpec{
            .name = "PrefixSum_" + tag,
            .cs = {
                .buffers = {
                    {.slot = "group_sums", .name = "GroupSums"},
                    {.slot = "io_buf", .name = "PrefixSum"}
                }
            },
            .shader = {
            #ifdef CROWY_METALRHI
                .filePath = "asset/Shaders/prefix_sum.metal",
                .funcName = "cs_add_group_sums",
            #elif CROWY_D3DRHI
                .filePath = L"asset/Shaders/prefix_sum.hlsl",
                .funcName = "cs_add_group_sums",
            #endif
            },
            .gridSize = {.x=HG_PADDED_N/2, .y=1, .z=1},
            .threadGroupSize = RHISize3D{
                .x=std::min(HG_PADDED_N/2u, 1024u),
                .y=1,
                .z=1
            }
        },
        ComputePassSpec{
            .name = "Scatter_" + tag,
            .cs = {
                .buffers = {
                    {.slot = "keys_in", .name = keysIn},
                    {.slot = "vals_in", .name = idxIn},
                    {.slot = "keys_out", .name = keysOut},
                    {.slot = "vals_out", .name = idxOut},
                    {.slot = "prefix_sums", .name = "PrefixSum"}
                },
                .bytes = {
                    {.slot = "bit_offset", .data = bitOffset},
                    {.slot = "count", .data = N},
                    {.slot = "num_groups", .data = HG_NUMGROUP},
                }
            },
            .shader = {
            #ifdef CROWY_METALRHI
                .filePath = "asset/Shaders/radix_sort.metal",
                .funcName = "cs_scatter",
            #elif CROWY_D3DRHI
                .filePath = L"asset/Shaders/radix_sort.hlsl",
                .funcName = "cs_scatter",
            #endif
            },
            .gridSize = {.x=HG_N, .y=1, .z=1},
            .threadGroupSize = RHISize3D{
                .x=HG_GROUPSIZE,
                .y=1,
                .z=1
            }
        }
    };
}

auto makeRadixSortPass(){
    std::vector<ComputePassSpec> passes;
    passes.reserve(8 * 5);

    for(int bit=0; bit<32; bit+=4){
        auto isEven = (bit / 4) % 2 == 0;

        auto pass = makeRadixPass(
            bit,
            isEven ? "Keys" : "KeysOut",
            isEven ? "KeysOut" : "Keys",
            isEven ? "Indexes" : "IndexesOut",
            isEven ? "IndexesOut" : "Indexes"
        );
        passes.append_range(pass);
    }

    return passes;
}

int main(int argc, char* argv[]){
    std::vector<uint32_t> keys(N);
    std::vector<uint32_t> indexes(N);

    {
        std::ranges::iota(keys, 0u);
        std::ranges::iota(indexes, 0u);

        auto seed = std::random_device{}();
        std::mt19937 gen(seed);
        std::ranges::shuffle(keys, gen);
    }

    // padding for prefix sum
    std::vector<uint32_t> hg_keys(HG_PADDED_N, 0u);

    auto device = createDevice();
    auto cmdList = device->createCommandList();

    Renderer renderer(*device);
    RenderSpec spec{
        .buffers = {
            {
                "Keys",
                RHIBufferCreateDesc{
                    .size = static_cast<uint32_t>(sizeof(decltype(keys)::value_type) * keys.size()),
                    .usage = BUF_AllowShaderRW,
                    .access = RHIMemoryAccess::CPUWrite,
                    .initialData = keys.data()
                }
            },
            {
                "Indexes",
                RHIBufferCreateDesc{
                    .size = static_cast<uint32_t>(sizeof(decltype(indexes)::value_type) * indexes.size()),
                    .usage = BUF_AllowShaderRW,
                    .access = RHIMemoryAccess::CPUWrite,
                    .initialData = indexes.data()
                }
            },
            {
                "Histogram",
                RHIBufferCreateDesc{
                    .size = static_cast<uint32_t>(sizeof(decltype(hg_keys)::value_type) * hg_keys.size()),
                    .usage = BUF_AllowShaderRW,
                    .access = RHIMemoryAccess::CPUWrite,
                    .initialData = hg_keys.data()
                }
            },
            {
                "GroupSums",
                RHIBufferCreateDesc{
                    .size = static_cast<uint32_t>(sizeof(uint32_t) * next_pow2(PS_NUMGROUP)),
                    .usage = BUF_AllowShaderRW
                }
            },
            {
                "PrefixSum",
                RHIBufferCreateDesc{
                    .size = sizeof(uint32_t) * HG_PADDED_N,
                    .usage = BUF_AllowShaderRW
                }
            },
            {
                "KeysOut",
                RHIBufferCreateDesc{
                    .size = sizeof(uint32_t) * N,
                    .usage = BUF_AllowShaderRW,
                    .access = RHIMemoryAccess::CPURead
                }
            },
            {
                "IndexesOut",
                RHIBufferCreateDesc{
                    .size = sizeof(uint32_t) * N,
                    .usage = BUF_AllowShaderRW,
                    .access = RHIMemoryAccess::CPURead
                }
            }
        },
        .computePasses = makeRadixSortPass()
    };

    try{
        renderer.loadPasses(spec);
    }
    catch(const std::exception& e){
        std::println("{}", e.what());

        return 0;
    }

    cmdList->begin();
    renderer.render(*cmdList);
    cmdList->close();

    device->submit(*cmdList);
    cmdList->waitUntilCompleted();

    auto outBuf = renderer.getBuffer("IndexesOut");
    outBuf->download(indexes.data(), sizeof(decltype(indexes)::value_type) * N);

    for(size_t i=0; i<indexes.size(); ++i){
        auto key = keys[indexes[i]];
        if(key != i){
            std::println("wrong result after [{}] = {}", i, key);
            return 0;
        }
    }

    std::println("Success!");
    return 0;
}