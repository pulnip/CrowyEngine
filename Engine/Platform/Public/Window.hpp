#pragma once

#include <memory>

namespace Crowy
{
    class Window{
    public:
        Window();
        ~Window();

    private:
        struct Impl;
        std::unique_ptr<Impl> impl;
    };
}