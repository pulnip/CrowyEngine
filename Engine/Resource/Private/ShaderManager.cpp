#include "LoadContext.hpp"
#include "RHIDevice.hpp"
#include "RHIShader.hpp"
#include "ShaderManager.hpp"

namespace Crowy
{
    ShaderManager* ShaderManager::instance = nullptr;

    Shader instantiate(const ShaderRequest& request, LoadContext& ctx){
        return Shader{
            .vertexShader = ctx.device->createShader(
                RHIShaderCreateDesc{
                    .file = request.vsFilePath.c_str(),
                    .entry = request.vsFuncName.c_str(),
                    .stage = RHIShaderStage::VertexShader,
                    .debugName = request.fsFilePath.c_str()
                }
            ),
            .fragmentShader = ctx.device->createShader(
                RHIShaderCreateDesc{
                    .file = request.fsFilePath.c_str(),
                    .entry = request.fsFuncName.c_str(),
                    .stage = RHIShaderStage::FragmentShader,
                    .debugName = request.fsFilePath.c_str()
                }
            )
        };
    }
}
