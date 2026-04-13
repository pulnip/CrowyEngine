#include <bit>
#include <cstddef>
#include <cstdint>
#include <numeric>
#include <random>
#include <vector>
#include "RHIDefinitions.hpp"
#include "RHIDevice.hpp"
#include "Renderer.hpp"
#include "RenderSpec.hpp"

using namespace Crowy;

constexpr auto nextPow2(uint32_t n){
    return 1 << (32 - std::countl_zero(n-1));
}

constexpr auto ceilDiv(int n, int d){
    return (n + (d-1)) / d;
}

constexpr auto nextMul(int n, int m){
    return ceilDiv(n, m) * m;
}

template<typename T>
auto pickInRange(T m, T M){
    static auto seed = std::random_device{}();
    static std::mt19937 gen(seed);

    return std::uniform_int_distribution(m, M-1)(gen);
}

constexpr auto N = 10000;

// HG = Histogram
// PS = Prefix Sum
constexpr auto HG_GROUPSIZE = 16;
constexpr auto HG_NUMGROUP = ceilDiv(N, HG_GROUPSIZE);
constexpr auto HG_N = HG_GROUPSIZE * HG_NUMGROUP;

constexpr auto PS_GROUPSIZE = 1024;
constexpr auto PS_BLOCK = 2*PS_GROUPSIZE;

constexpr auto HG_PADDED_N = nextMul(HG_N, PS_BLOCK);
constexpr auto PS_NUMGROUP = HG_PADDED_N / PS_BLOCK;

auto makeRadixPass(
    uint32_t bitOffset,
    const std::string& keysIn,
    const std::string& keysOut,
    const std::string& idxIn,
    const std::string& idxOut
) -> std::vector<ComputePassSpec>
{
    auto tag = std::to_string(bitOffset);

    return{
        {
            .name = "Histogram_" + tag,
            .inputBuffers = {
                {.name = keysIn, .slot = 0}
            },
            .inputInts = {
                {.data = bitOffset, .slot = 2},
                {.data = N, .slot = 3},
                {.data = HG_NUMGROUP, .slot = 4}
            },
            .outputBuffers = {
                {.name = "Histogram", .slot = 1}
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
        {
            .name = "LocalPrefixSum_" + tag,
            .inputBuffers = {
                {.name = "Histogram", .slot = 0}
            },
            .outputBuffers = {
                {.name = "PrefixSum", .slot = 1},
                {.name = "GroupSums", .slot = 2}
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
                .x=std::min(HG_PADDED_N/2, 1024),
                .y=1,
                .z=1
            }
        },
        {
            .name = "GroupPrefixSum_" + tag,
            .inputBuffers = {
                {.name = "GroupSums", .slot = 0}
            },
            .outputBuffers = {
                {.name = "GroupSums", .slot = 1},
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
            .gridSize = {.x=nextPow2(PS_NUMGROUP)/2, .y=1, .z=1},
            .threadGroupSize = RHISize3D{
                .x=std::min(nextPow2(PS_NUMGROUP)/2, 1024),
                .y=1,
                .z=1
            }
        },
        {
            .name = "PrefixSum_" + tag,
            .inputBuffers = {
                {.name = "GroupSums", .slot = 0}
            },
            .outputBuffers = {
                {.name = "PrefixSum", .slot = 1},
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
                .x=std::min(HG_PADDED_N/2, 1024),
                .y=1,
                .z=1
            }
        },
        {
            .name = "Scatter_" + tag,
            .inputBuffers = {
                {.name = keysIn,     .slot = 0},
                {.name = idxIn,      .slot = 1},
                {.name = "PrefixSum", .slot = 4},
            },
            .inputInts = {
                {.data = bitOffset, .slot = 5},
                {.data = N, .slot = 6},
                {.data = HG_NUMGROUP, .slot = 7}
            },
            .outputBuffers = {
                {.name = keysOut,    .slot = 2},
                {.name = idxOut,     .slot = 3},
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
        for(int i=0; i<N; ++i){
            std::swap(
                keys[pickInRange(0, N)],
                keys[pickInRange(0, N)]
            );
        }
        std::ranges::iota(indexes, 0u);
    }

    // padding for prefix sum
    std::vector<uint32_t> hg_keys(HG_PADDED_N, 0u);

    auto device = createDevice();
    auto cmdList = device->createCommandList();

    Renderer renderer(device.get());
    RenderSpec spec{
        .buffers = {
            {
                "Keys",
                RHIBufferCreateDesc{
                    .size = sizeof(decltype(keys)::value_type) * keys.size(),
                    .usage = combine(
                        RHIBufferUsage::CPUWrite,
                        RHIBufferUsage::ShaderResource,
                        RHIBufferUsage::UnorderedAccess
                    ),
                    .initialData = keys.data()
                }
            },
            {
                "Indexes",
                RHIBufferCreateDesc{
                    .size = sizeof(decltype(indexes)::value_type) * indexes.size(),
                    .usage = combine(
                        RHIBufferUsage::CPUWrite,
                        RHIBufferUsage::ShaderResource,RHIBufferUsage::UnorderedAccess
                    ),
                    .initialData = indexes.data()
                }
            },
            {
                "Histogram",
                RHIBufferCreateDesc{
                    .size = sizeof(decltype(hg_keys)::value_type) * hg_keys.size(),
                    .usage = combine(
                        RHIBufferUsage::CPUWrite,
                        RHIBufferUsage::ShaderResource,
                        RHIBufferUsage::UnorderedAccess
                    ),
                    .initialData = hg_keys.data()
                }
            },
            {
                "GroupSums",
                RHIBufferCreateDesc{
                    .size = sizeof(uint32_t) * nextPow2(PS_NUMGROUP),
                    .usage = combine(
                        RHIBufferUsage::ShaderResource,
                        RHIBufferUsage::UnorderedAccess
                    )
                }
            },
            {
                "PrefixSum",
                RHIBufferCreateDesc{
                    .size = sizeof(uint32_t) * HG_PADDED_N,
                    .usage = combine(
                        RHIBufferUsage::ShaderResource,
                        RHIBufferUsage::UnorderedAccess
                    )
                }
            },
            {
                "KeysOut",
                RHIBufferCreateDesc{
                    .size = sizeof(uint32_t) * N,
                    .usage = combine(
                        RHIBufferUsage::ShaderResource,
                        RHIBufferUsage::UnorderedAccess,
                        RHIBufferUsage::CPURead
                    )
                }
            },
            {
                "IndexesOut",
                RHIBufferCreateDesc{
                    .size = sizeof(uint32_t) * N,
                    .usage = combine(
                        RHIBufferUsage::ShaderResource,
                        RHIBufferUsage::UnorderedAccess,
                        RHIBufferUsage::CPURead
                    )
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