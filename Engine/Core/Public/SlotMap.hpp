#pragma once

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <iterator>
#include <optional>
#include <vector>
#include "Assert.hpp"
#include "GenericHandle.hpp"
#include "Primitives.hpp"
#include "Semantics.hpp"

namespace Crowy
{
    struct IteratorEndTag{};

    template<typename T>
    class SlotMap{
    private:
        using Handle = GenericHandle<T>;

        class Slot{
        private:
            std::optional<T> data = std::nullopt;
            usize generation = 0;

        public:
            Slot() = default;
            ~Slot() = default;
            CROWY_DECLARE_MOVE_ONLY(Slot)

            void Reset() noexcept{
                ++generation;
                data.reset();
            }

            bool IsUsing() const noexcept{
                return data.has_value();
            }

            auto& GetData(this auto& self) noexcept{
                CROWY_ASSERT(self.IsUsing());
                return self.data.value();
            }

            T Release() noexcept{
                CROWY_ASSERT(IsUsing());
                T t = std::move(*data);
                Reset();

                return t;
            }

            usize GetGeneration() const noexcept{
                return generation;
            }

            template<typename... Args>
            void EmplaceData(Args&&... args){
                data.emplace(std::forward<Args>(args)...);
            }

            void SetData(T t){
                data = std::move(t);
            }
        };
        std::vector<Slot> slots;
        std::vector<usize> freeIndexes;

    public:
        SlotMap() = default;
        ~SlotMap() = default;
        CROWY_DECLARE_TRANSFERABLE(SlotMap)

        template<typename... Args>
        Handle Emplace(Args&&... args){
            auto freeIndex = acquireSlot();
            auto& slot = slots[freeIndex];

            slot.EmplaceData(std::forward<Args>(args)...);

            return Handle(freeIndex, slot.GetGeneration());
        }

        Handle NewSlot(){
            auto freeIndex = acquireSlot();
            auto& slot = slots[freeIndex];

            return Handle(freeIndex, slot.GetGeneration());
        }

        Handle Push(const T& t){
            return Emplace(T(t));
        }
        Handle Push(T&& t){
            return Emplace(std::move(t));
        }

        bool Remove(Handle handle) noexcept{
            auto index = handle.GetIndex();
            auto outOfIndex = index >= slots.size();
            if(outOfIndex)
                return false;

            auto& slot = slots[index];
            auto expiredGen = slot.GetGeneration() != handle.GetGeneration();
            if(expiredGen)
                return false;

            slot.Reset();
            freeIndexes.push_back(index);

            return true;
        }

        void Clear() noexcept{
            for(auto& slot: slots){
                if(slot.IsUsing()){
                    slot.Reset();
                }
            }

            freeIndexes.clear();
            freeIndexes.reserve(slots.size());
            for(usize i=0; i<slots.size(); ++i){
                freeIndexes.push_back(i);
            }
        }

        const T* Find(this auto& self, Handle handle) noexcept{
            auto index = handle.GetIndex();
            auto outOfIndex = index >= self.slots.size();
            if(outOfIndex)
                return nullptr;

            auto& slot = self.slots[index];
            auto expiredGen = slot.GetGeneration() != handle.GetGeneration();
            if(expiredGen)
                return nullptr;

            return &slot.GetData();
        }
        auto& GetRef(this auto& self, Handle handle) noexcept{
            auto index = handle.GetIndex();
            auto& slot = self.slots[index];

            CROWY_ASSERT(
                index < self.slots.size() &&
                slot.GetGeneration() == handle.GetGeneration()
            );
            return slot.GetData();
        }

        void Fulfill(this auto& self, Handle handle, T&& data) noexcept{
            auto index = handle.GetIndex();
            auto& slot = self.slots[index];

            CROWY_ASSERT(
                index < self.slots.size() &&
                slot.GetGeneration() == handle.GetGeneration()
            );

            slot.EmplaceData(std::move(data));
        }

        T Release(Handle handle) noexcept{
            auto index = handle.GetIndex();
            auto& slot = slots[index];

            CROWY_ASSERT(
                index < slots.size() &&
                slot.GetGeneration() == handle.GetGeneration()
            );

            return slot.Release();
        }

        void Reserve(usize size){
            if(size <= slots.size())
                return;
            for(auto i=slots.size(); i<size; ++i)
                freeIndexes.push_back(i);
            slots.resize(size);
        }

        usize Size() const noexcept{
            return slots.size() - freeIndexes.size();
        }
        usize Capacity() const noexcept{
            return slots.size();
        }

        class ConstIterator;

        class Iterator{
        public:
            using iterator_category = std::forward_iterator_tag;
            using value_type = T;
            using difference_type = std::ptrdiff_t;
            using pointer = T*;
            using reference = T&;

            friend ConstIterator;

        private:
            std::vector<Slot>* slots;
            usize index;

        public:
            Iterator(std::vector<Slot>* s, usize i)
                : slots(s)
                , index(i)
            {
                skipFree();
            }
            Iterator(std::vector<Slot>* s, usize i, IteratorEndTag)
                :slots(s), index(i){}

            reference operator*(){
                return (*slots)[index].GetData();
            }
            pointer operator->(){
                return &(*slots)[index].GetData();
            }

            Iterator& operator++() noexcept{
                ++index;
                skipFree();
                return *this;
            }
            Iterator operator++(int) noexcept{
                Iterator tmp = *this;
                ++(*this);
                return tmp;
            }

            bool operator==(const Iterator& other) const noexcept{
                CROWY_ASSERT(slots == other.slots);
                return index == other.index;
            }
            bool operator!=(const Iterator& other) const noexcept{
                CROWY_ASSERT(slots == other.slots);
                return index != other.index;
            }
            bool operator==(const ConstIterator& other) const noexcept{
                CROWY_ASSERT(slots == other.slots);
                return index == other.index;
            }
            bool operator!=(const ConstIterator& other) const noexcept{
                CROWY_ASSERT(slots == other.slots);
                return index != other.index;
            }

            Handle MakeHandle(){
                auto gen = (*slots)[index].GetGeneration();
                return Handle(index, gen);
            }

        private:
            void skipFree() noexcept{
                while(index < slots->size() && !(*slots)[index].IsUsing())
                    ++index;
            }
        };

        class ConstIterator{
        public:
            using iterator_category = std::forward_iterator_tag;
            using value_type = T;
            using difference_type = std::ptrdiff_t;
            using pointer = const T*;
            using reference = const T&;

            friend Iterator;

        private:
            const std::vector<Slot>* slots;
            usize index;

        public:
            ConstIterator(const std::vector<Slot>* s, usize i)
                : slots(s)
                , index(i)
            {
                skipFree();
            }
            ConstIterator(const std::vector<Slot>* s, usize i, IteratorEndTag)
                :slots(s), index(i){}

            reference operator*() const{
                return (*slots)[index].GetData();
            }
            pointer operator->() const{
                return &(*slots)[index].get();
            }

            ConstIterator& operator++() noexcept{
                ++index;
                skipFree();
                return *this;
            }
            ConstIterator operator++(int) noexcept{
                ConstIterator tmp = *this;
                ++(*this);
                return tmp;
            }

            bool operator==(const Iterator& other) const noexcept{
                CROWY_ASSERT(slots == other.slots);
                return index == other.index;
            }
            bool operator!=(const Iterator& other) const noexcept{
                CROWY_ASSERT(slots == other.slots);
                return index != other.index;
            }
            bool operator==(const ConstIterator& other) const noexcept{
                CROWY_ASSERT(slots == other.slots);
                return index == other.index;
            }
            bool operator!=(const ConstIterator& other) const noexcept{
                CROWY_ASSERT(slots == other.slots);
                return index != other.index;
            }

            Handle MakeHandle(){
                auto gen = (*slots)[index].GetGeneration();
                return Handle(index, gen);
            }

        private:
            void skipFree() noexcept{
                while(index < slots->size() && !(*slots)[index].IsUsing())
                    ++index;
            }
        };

        auto begin() noexcept{
            return Iterator(&slots, 0);
        }
        auto end() noexcept{
            return Iterator(&slots, slots.size(), IteratorEndTag{});
        }
        auto begin() const noexcept{
            return ConstIterator(&slots, 0);
        }
        auto end() const noexcept{
            return ConstIterator(&slots, slots.size(), IteratorEndTag{});
        }
        auto cbegin() const noexcept{
            return begin();
        }
        auto cend() const noexcept{
            return end();
        }

    private:
        usize acquireSlot(){
            if(!freeIndexes.empty()){
                auto idx = freeIndexes.back();
                freeIndexes.pop_back();
                return idx;
            }
            slots.emplace_back();
            return slots.size() - 1;
        }
    };
}
