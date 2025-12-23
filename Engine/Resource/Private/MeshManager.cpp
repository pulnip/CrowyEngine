#include "LoadContext.hpp"
#include "MeshImporter.hpp"
#include "MeshManager.hpp"
#include "RHIDevice.hpp"

namespace Crowy
{
    MeshManager* MeshManager::instance = nullptr;

    Mesh instantiate(const MeshRequest& request, LoadContext& ctx){
        auto [scheme, path] = splitSchemeAndPath(request.uri);

        std::optional<MeshData> m;

        switch(scheme){
        case SchemeKind::File:
            m = importMesh(path, ctx.device->getCapabilities());
            break;
        case SchemeKind::Embedded:
            m = loadEmbeddedMesh(path);
            break;
        default:
            throw std::runtime_error("met Undefined Scheme");
        }

        if(!m.has_value())
            throw std::runtime_error("impossible to import mesh");

        auto meshData = m.value();

        // auto mesh = ctx.device->create
    }
}