#pragma once

#include "InputProvider.hpp"

namespace Crowy
{
    class SDLInputProvider: public InputProvider{
    private:
        const bool* transitionKeys;

    public:
        SDLInputProvider();

        void poll() override;
    };
}