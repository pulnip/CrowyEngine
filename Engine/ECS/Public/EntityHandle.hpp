#pragma once

#include "ComponentDefinitions.hpp"
#include "ptr_util.hpp"
#include "Component.hpp"

namespace Crowy
{
    struct EntityHandle{
        void* const ptr;
        const ArchetypeBit bit;

        template<typename T>
        const T* getComponent() const{
            auto offset = offset_of<T>(bit);
            if(offset == -1)
                return nullptr;

            return reinterpret_cast<T*>(
                ptrAdd(ptr, offset)
            );
        }

        template<typename T>
        T* getComponent(){
            return const_cast<T*>(
                static_cast<const EntityHandle&>(*this).getComponent<T>()
            );
        }

        CharacterController* getCharacterController(){
            return getComponent<CharacterController>();
        }
    };
}