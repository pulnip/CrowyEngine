#pragma once

#include <memory>
#include "semantics.hpp"
#include "RHIFWD.hpp"
#include "RHIDefinitions.hpp"

#ifdef USE_STATIC_RHI
    #ifdef USE_METAL_BACKEND
        #include "MetalTextureView.hpp"
    #else
        #include "NullTextureView.hpp"
    #endif
#endif


namespace Crowy
{
#ifdef USE_STATIC_RHI
    template<typename T>
    concept RHITextureViewType = requires(T view){
        { view.getAccess() } -> std::same_as<RHIBindingAccess>;
        { view.getFormat() } -> std::same_as<RHIPixelFormat>;
    };
    static_assert(RHITextureViewType<RHITextureView>);
#else
    class RHITextureView{
    public:
        CROWY_DECLARE_INTERFACE_NOEXCEPT(RHITextureView)

        virtual RHIBindingAccess getAccess() const noexcept = 0;
        virtual RHIPixelFormat getFormat() const noexcept = 0;
    };
#endif

    using RHITextureViewPtr = std::unique_ptr<RHITextureView>;
}