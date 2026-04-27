#pragma once

#include "RHIAPI.hpp"
#include "RHIDefinitions.hpp"
#ifndef USE_STATIC_RHI
    #include "RHIBufferView.hpp"
#endif

namespace Crowy
{
    class NullBufferView
#ifndef USE_STATIC_RHI
        : public RHIBufferView
#endif
    {
    private:
        RHIBindingAccess access;
        uint32_t offset = 0, size = 0;
        const std::string debugName;

    public:
        NullBufferView(const RHIBufferViewDesc& desc, const std::string& name)
            : access(desc.access)
            , offset(desc.offset), size(desc.size)
            , debugName(name)
        {}
        ~NullBufferView() = default;

        RHIBindingAccess getAccess() const noexcept RHI_OVERRIDE{
            return access;
        }
        uint32_t getOffset() const noexcept RHI_OVERRIDE{
            return offset;
        }
        uint32_t getSize() const noexcept RHI_OVERRIDE{
            return size;
        }
    };
}
