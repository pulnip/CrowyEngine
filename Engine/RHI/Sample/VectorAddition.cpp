#include <cstring>
#include <print>
#include <vector>
#include <SDL3/SDL.h>
#include "enum_traits.hpp"
#include "RHIDefinitions.hpp"
#include "Logger.hpp"
#include "RHIDevice.hpp"
#include "Timer.hpp"

using namespace Crowy;

int main(int argc, char* argv[]){
    Logger::instance().setMinLevel(LogLevel::Warn);

    auto device = createDevice();

    auto cmdList = device->createCommandList();

    auto computeShader = device->createShader(RHIShaderCreateDesc{
    #ifdef CROWY_METALRHI
        .file = "asset/Shaders/vector_addition.metal",
        .entry = "cs_vector_addition",
    #elif CROWY_D3DRHI
        .file = L"asset/Shaders/vector_addition.hlsl",
        .entry = "cs_vector_addition",
    #endif
        .stage = RHIShaderStage::ComputeShader,
    });

    constexpr size_t N = 1 << 16;
    std::vector<float> floats(N, 1.0f);

    RHIBufferCreateDesc bufferDesc{
        .size = sizeof(float) * N,
        .usage = combine(
            RHIBufferUsage::CPUWrite
        ),
        .stride = 0,
        .initialData = floats.data()
    #if defined(_DEBUG) || !defined(NDEBUG)
        , .debugName = "Buffer"
    #endif
    };

    auto bufferA = device->createBuffer(bufferDesc);
    auto bufferB = device->createBuffer(bufferDesc);

    bufferDesc.initialData = nullptr;
    auto bufferOut = device->createBuffer(bufferDesc);

    auto pipelineState = device->createComputePipelineState({
        .computeShader = computeShader.get()
    #if defined(_DEBUG) || !defined(NDEBUG)
        , .debugName = "Compute Pipeline"
    #endif
    });

    Timer timer;
    timer.reset();

    cmdList->begin();
    cmdList->beginCompute();

    cmdList->setPipelineState(pipelineState.get());
    cmdList->setBuffer(0, *bufferA.get());
    cmdList->setBuffer(1, *bufferB.get());
    cmdList->setBuffer(2, *bufferOut.get());

    cmdList->dispatch(
        N, 1, 1,
        256, 1, 1
    );

    cmdList->endCompute();
    cmdList->close();

    device->submit(*cmdList.get());
    cmdList->waitUntilCompleted();

    // TODO. change this at support D3D12
    bufferOut->download(floats.data(), sizeof(float) * floats.size());

    timer.newFrame();
    float et = timer.elapsedSeconds();

    std::println("Elapsed: {}s", et);

    for(size_t i=0; i<N; ++i){
        if(std::abs(floats[i] - 2.0f) > 1e-3){
            std::println("float[{}]: {}", i, floats[i]);
            break;
        }
    }

    return 0;
}