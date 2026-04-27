#pragma once

#include "RHIAPI.hpp"
#include "RHIDefinitions.hpp"
#ifndef USE_STATIC_RHI
    #include "RHITextureView.hpp"
#endif

namespace Crowy
{
    class NullTextureView
#ifndef USE_STATIC_RHI
        : public RHITextureView
#endif
    {
    private:
        RHIBindingAccess access;
        RHIPixelFormat format;
        const std::string debugName;

    public:
        NullTextureView(const RHITextureViewDesc& desc, const std::string& name)
            : access(desc.access)
            , format(desc.format)
            , debugName(name)
        {}
        ~NullTextureView() = default;

        RHIBindingAccess getAccess() const noexcept RHI_OVERRIDE{
            return access;
        }
        RHIPixelFormat getFormat() const noexcept RHI_OVERRIDE{
            return format;
        }
    };
}
