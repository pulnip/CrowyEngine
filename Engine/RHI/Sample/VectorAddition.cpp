#include <cstring>
#include <print>
#include <vector>
#include <SDL3/SDL.h>
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

    constexpr size_t N = 1 << 26;
    std::vector<float> floats(N, 1.0f);
    std::vector<float> outGpu(N, 0.0f), outCpu(N, 0.0f);

    RHIBufferCreateDesc bufferDesc{
        .size = sizeof(float) * N,
        .usage = RHIBufferUsage::AllowShaderRead,
        .access = RHIMemoryAccess::CPUWrite,
        .stride = 0,
        .initialData = floats.data()
    };

    auto bufferA = device->createBuffer(bufferDesc, "BufferA");
    auto bufferB = device->createBuffer(bufferDesc, "BufferB");

    bufferDesc.usage = RHIBufferUsage::AllowShaderWrite;
    bufferDesc.access = RHIMemoryAccess::CPURead;
    bufferDesc.initialData = nullptr;
    auto bufferOut = device->createBuffer(bufferDesc, "BufferOut");

    auto pipelineState = device->createPipelineState({
        .computeShader = computeShader.get()
    }, "Compute Pipeline");

    Timer timer;
    timer.reset();

    cmdList->begin();
    cmdList->beginCompute();

    cmdList->setPipelineState(*pipelineState);
    cmdList->setBuffer(0, *bufferA.get());
    cmdList->setBuffer(1, *bufferB.get());
    cmdList->setBuffer(2, *bufferOut.get());

    cmdList->dispatch({N, 1, 1});

    cmdList->endCompute();
    cmdList->close();

    device->submit(*cmdList.get());
    cmdList->waitUntilCompleted();

    // TODO. change this at support D3D12
    bufferOut->download(outGpu.data(), sizeof(float) * outGpu.size());

    timer.newFrame();
    auto etGpu = timer.elapsedSeconds();

    for(size_t i=0; i<N; ++i){
        outCpu[i] = floats[i] + floats[i];
    }

    timer.newFrame();
    auto etCpu = timer.elapsedSeconds();

    // verify compute result
    for(size_t i=0; i<N; ++i){
        if(std::abs(outGpu[i] - 2.0f) < 1e-3)
            continue;
        std::println("[GPU] wrong result: float[{}] = {}", i, floats[i]);
    }
    for(size_t i=0; i<N; ++i){
        if(std::abs(outCpu[i] - 2.0f) < 1e-3)
            continue;
        std::println("[CPU] wrong result: float[{}] = {}", i, floats[i]);
    }

    std::println("Elapsed: cpu {}s, gpu {}s", etCpu, etGpu);

    return 0;
}