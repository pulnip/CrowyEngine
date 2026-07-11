#pragma once

#include <vector>
#include "GenericHandle.hpp"
#include "Resource.hpp"
#include "Semantics.hpp"

namespace Crowy
{
    template<Resource T>
    class ResourceManager;

    template<Resource T>
    class ResourceLoader{
    public:
        using Request = typename T::Request;
        using Handle = GenericHandle<T>;

        struct Completion{
            T data;
            Handle handle;
        };

    public:
        SMOL_DECLARE_INTERFACE(ResourceLoader)

        virtual void Submit(const Request&, Handle, ResourceManager<T>&) = 0;
        virtual void Poll(std::vector<Completion>&) = 0;
    };
}
