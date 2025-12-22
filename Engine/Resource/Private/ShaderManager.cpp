#include "LoadContext.hpp"
#include "ShaderManager.hpp"

namespace Crowy
{
    ShaderManager* ShaderManager::instance = nullptr;

    Shader instantiate(const ShaderRequest& request, LoadContext& ctx){
        // TODO
        throw std::runtime_error("Not implemented");
    }
}
