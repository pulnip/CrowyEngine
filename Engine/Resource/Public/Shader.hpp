#pragma once

#include <memory>
#include <string>
#include "generic_handle.hpp"
#include "RHIDefinitions.h"

namespace Crowy
{
    struct LoadContext;

    struct Shader{
        RHIShaderHandle vertexShader;
        RHIShaderHandle fragmentShader;

        inline bool isValid() const{
            return vertexShader.isValid() && fragmentShader.isValid();
        }

        struct Request{
            using Key     = std::string;
            using KeyHash = std::hash<std::string>;

            std::string path;

            inline Key key() const{ return path; }
        };

        static Shader make(const Request&, LoadContext&);
    };
    using ShaderHandle = generic_handle<Shader>;

    class ShaderManager{
    public:
        ShaderManager();
        ~ShaderManager();

        ShaderHandle getOrLoad(const Shader::Request&,
            LoadContext& ctx);
        Shader*       get(ShaderHandle);
        const Shader* get(ShaderHandle) const;
        void unload(ShaderHandle);

    private:
        struct Impl;
        std::unique_ptr<Impl> impl;
    };
}