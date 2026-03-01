#pragma once

#include "ComponentDefinitions.hpp"
#include "ptr_util.hpp"
#include "Component.hpp"

namespace Crowy
{
    static constexpr size_t ID_REGION = 0;
    static constexpr auto COMPONENT_REGION = ID_REGION + sizeof(EntityID);

    struct EntityHandle{
        void* const ptr;
        const ArchetypeBit bit;

        template<typename T>
        const T* getComponent() const{
            auto offset = COMPONENT_REGION + offset_of<T>(bit);
            if(offset == -1)
                return nullptr;

            return reinterpret_cast<T*>(
                ptr_add(ptr, offset)
            );
        }

        template<typename T>
        T* getComponent(){
            return const_cast<T*>(
                static_cast<const EntityHandle&>(*this).getComponent<T>()
            );
        }

        TransformComponent* getTransformComponent(){
            return getComponent<TransformComponent>();
        }

        CharacterController* getCharacterController(){
            return getComponent<CharacterController>();
        }
    };
}