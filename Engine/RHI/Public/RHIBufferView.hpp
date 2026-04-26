#pragma once

#include <memory>
#include "semantics.hpp"
#include "RHIFWD.hpp"
#include "RHIDefinitions.hpp"

#ifdef USE_STATIC_RHI
    #ifdef USE_METAL_BACKEND
        #include "MetalBufferView.hpp"
    #else
        #include "NullBufferView.hpp"
    #endif
#endif


namespace Crowy
{
#ifdef USE_STATIC_RHI
    template<typename T>
    concept RHIBufferViewType = requires(T view){
        { view.getAccess() } -> std::same_as<RHIBindingAccess>;
        { view.getOffset() } -> std::same_as<uint32_t>;
        { view.getSize() } -> std::same_as<uint32_t>;
    };
    static_assert(RHIBufferViewType<RHIBufferView>);
#else
    class RHIBufferView{
    public:
        CROWY_DECLARE_INTERFACE_NOEXCEPT(RHIBufferView)

        virtual RHIBindingAccess getAccess() const noexcept = 0;
        virtual uint32_t getOffset() const noexcept = 0;
        virtual uint32_t getSize() const noexcept = 0;
    };
#endif

    using RHIBufferViewPtr = std::unique_ptr<RHIBufferView>;
}