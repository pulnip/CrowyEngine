#include <SDL3/SDL.h>
#include <cstddef>
#include "Logger.hpp"
#include "RHIDefinitions.hpp"
#include "RHIDevice.hpp"
#include "Renderer.hpp"
#include "RenderSpec.hpp"

using namespace Crowy;

int main(int argc, char* argv[]){
    Logger::instance().setMinLevel(LogLevel::Warn);

    auto device = createDevice();
    auto cmdList = device->createCommandList();

    constexpr size_t N = 1 << 20;
    std::vector<float> ones(N, 1.0f);
    std::vector<float> twos(N, 2.0f);

    Renderer renderer(device.get());
    RenderSpec spec{
        .buffers = {
            {
                "BufferA",
                RHIBufferCreateDesc{
                    .size = sizeof(float) * N,
                    .usage = RHIBufferUsage::AllowShaderRead,
                    .access = RHIMemoryAccess::CPUWrite,
                    .initialData = ones.data()
                }
            },
            {
                "BufferB",
                RHIBufferCreateDesc{
                    .size = sizeof(float) * N,
                    .usage = RHIBufferUsage::AllowShaderRead,
                    .access = RHIMemoryAccess::CPUWrite,
                    .initialData = twos.data()
                }
            },
            {
                "BufferOut",
                RHIBufferCreateDesc{
                    .size = sizeof(float) * N,
                    .usage = RHIBufferUsage::AllowShaderWrite,
                    .access = RHIMemoryAccess::CPURead,
                    .initialData = nullptr
                }
            }
        },
        .computePasses = {
            {
                .name = "VectorMultiply",
                .cs = {
                    .buffers = {
                        {.slot = "A", .name = "BufferA"},
                        {.slot = "B", .name = "BufferB"},
                        {.slot = "out", .name = "BufferOut"},
                    }
                },
                .shader = {
                #ifdef CROWY_METALRHI
                    .filePath = "asset/Shaders/vector_multiply.metal",
                    .funcName = "cs_vector_multiply",
                #elif CROWY_D3DRHI
                    .filePath = L"asset/Shaders/vector_multiply.hlsl",
                    .funcName = "cs_vector_multiply",
                #endif
                },
                .gridSize = {N, 1, 1}
            }
        }
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

    auto outBuf = renderer.getBuffer("BufferOut");
    std::vector<float> result(N);
    outBuf->download(result.data(), sizeof(float) * result.size());

    for(size_t i=0; i<N; ++i){
        if(std::abs(result[i] - 2.0) > 1e-3){
            std::println("wrong result after [{}] = {}", i, result[i]);
            return 0;
        }
    }

    std::println("Success!");
    return 0;
}