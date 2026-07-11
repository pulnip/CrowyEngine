#pragma once

#include <thread>
#include <unordered_map>
#include <vector>
#include "Assert.hpp"
#include "GenericHandle.hpp"
#include "ResourceLoader.hpp"
#include "SlotMap.hpp"

namespace Crowy
{
    template<Resource T>
    class ResourceManager{
    private:
        using Loader = ResourceLoader<T>;
        using Completion = typename Loader::Completion;
        using Handle = GenericHandle<T>;
        using HandleHash = std::hash<Handle>;

        using Request = typename T::Request;
        using Key = typename Request::Key;
        using KeyHash = typename Request::KeyHash;

    private:
        const std::thread::id initThread;
        Loader& loader;
        SlotMap<T> pool;
        std::unordered_map<Key, Handle, KeyHash, std::equal_to<>> dedup;
        std::unordered_map<Handle, Key, HandleHash, std::equal_to<>> handleToKey;
        u32 pending = 0;

    public:
        ResourceManager(Loader& loader)
            : initThread(std::this_thread::get_id())
            , loader(loader){}

        Handle Find(StrView key) const noexcept{
            if(auto it = dedup.find(key); it != dedup.end()){
                return it->second;
            }

            return Handle::InvalidHandle();
        }

        Handle Load(StrView key, const Request& request){
            SMOL_ASSERT(initThread == std::this_thread::get_id());
            auto handle = Find(key);
            if(handle.IsValid()){
                return handle;
            }

            handle = pool.Push({});
            dedup[key] = handle;
            handleToKey[handle] = key;

            ++pending;
            loader.Submit(request, handle, *this);

            return handle;
        }

    private:
        std::vector<Completion> scratch;

    public:
        void DrainCompletions(){
            SMOL_ASSERT(initThread == std::this_thread::get_id());
            scratch.clear();

            loader.Poll(scratch);

            for(auto& [data, handle]: scratch){
                pool.Fulfill(handle, std::move(data));
                --pending;
            }
        }

        auto& GetRef(this auto& self, Handle handle) noexcept{
            return self.pool.GetRef(handle);
        }
    };
}
