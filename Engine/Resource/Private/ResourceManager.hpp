#pragma once

#include <cstdint>
#include <unordered_map>
#include "slot_map.hpp"

namespace Crowy
{
    class LoadContext;

    template<typename T>
    class ResourceManager{
    public:
        using Request    = typename T::Request;
        using Key        = typename Request::Key;
        using KeyHash    = typename Request::KeyHash;
        using Handle     = generic_handle<T>;
        using HandleHash = generic_handle_hash<T>;

    private:
        slot_map<T> pool;
        std::unordered_map<Key, Handle,    KeyHash> keyToHandle;
        // map for unload
        std::unordered_map<Handle, Key, HandleHash> handleToKey;

    public:
        Handle getOrLoad(const Request& request, LoadContext& ctx){
            auto key = request.key();

            if(auto it = keyToHandle.find(key); it != keyToHandle.end()){
                return it->second;
            }

            auto resource = instantiate(request, ctx);
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