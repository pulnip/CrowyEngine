#include "Component.hpp"

#include <format>

#include "Assert.hpp"

namespace Crowy
{
    ComponentRegistry& ComponentRegistry::Get() {
        static ComponentRegistry singleton;
        return singleton;
    }

    void ComponentRegistry::Register(ComponentInfo info) {
        const auto* desc = info.desc;
        if(!infoOfType.try_emplace(desc, std::move(info)).second) {
            detail::assertFail(
                "inserted",
                std::source_location::current(),
                std::format(
                    "'{}' is registered as a component twice",
                    desc->name
                )
            );
        }
    }

    const ComponentInfo* ComponentRegistry::Find(
        const TypeDesc& desc
    ) const noexcept {
        const auto it = infoOfType.find(&desc);
        return it == infoOfType.end() ? nullptr : &it->second;
    }
}
