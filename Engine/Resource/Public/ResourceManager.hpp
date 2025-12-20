#pragma once

#include "slot_map.hpp"
#include "ResourceDefinitions.hpp"

namespace Crowy
{
    template<typename T>
    class ResourceManager{
    public:
        using Traits     = ResourceTraits<T>;
        using Request    = typename Traits::Request;
        using Key        = typename Traits::Key;
        using KeyHash    = typename Traits::KeyHash;
        using Handle     = generic_handle<T>;
        using HandleHash = generic_handleHash<T>;

    private:
        slot_map<T> pool;
        std::unordered_map<Key, Handle,    KeyHash> keyToHandle;
        // map for unload
        std::unordered_map<Handle, Key, HandleHash> handleToKey;

    public:
        Handle getOrLoad(const Request& request){
            Key key = Traits::makeKey(request);

            if(auto it = keyToHandle.find(key); it != keyToHandle.end()){
                return it->second;
            }

            // Load via ResourceTraits
            auto resource = Traits::load(request);
            auto handle = pool.push(std::move(resource));

            keyToHandle.emplace(key, handle);
            handleToKey.emplace(handle, key);
            return handle;
        }

        T*       get(Handle handle)      { return &pool[handle]; }
        const T* get(Handle handle) const{ return &pool[handle]; }

        void unload(Handle handle){
            if(auto it = handleToKey.find(handle); it != handleToKey.end()){
                keyToHandle.erase(it->second);
                handleToKey.erase(it);

                pool.remove(handle);
            }
        }
    };
}