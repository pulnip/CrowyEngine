#include "PropertyWalker.hpp"

#include <optional>
#include <utility>
#include <vector>

namespace
{
    using namespace Crowy;

    Widget makeLeaf(const PropertyDesc&, void* member, const DirtyCallback&);

    void appendProperties(
        std::vector<Widget>& out,
        void* target,
        const TypeDesc& desc,
        const DirtyCallback& onDirty
    ){
        if(desc.parent != nullptr){
            appendProperties(out, target, *desc.parent, onDirty);
        }

        for(const auto& prop: desc.properties){
            out.push_back(makeLeaf(prop, prop.accessor->Get(target), onDirty));
        }
    }

    // An enum's storage is whatever its underlying type is, and TypeOps
    // records the width but not the signedness. Comparing the raw bits of
    // that width settles both at once: a negative enumerator of a signed
    // enum and a large one of an unsigned enum each match themselves and
    // nothing else. The second writer (the remote port) is when these
    // belong in Reflection.
    u64 loadEnumBits(const void* member, usize size){
        switch(size){
        case 1: return *static_cast<const u8*>(member);
        case 2: return *static_cast<const u16*>(member);
        case 4: return *static_cast<const u32*>(member);
        case 8: return *static_cast<const u64*>(member);
        }
        CROWY_ASSERT(false, "enum of unsupported width");
        return 0;
    }

    u64 enumBits(i64 value, usize size){
        switch(size){
        case 1: return static_cast<u8>(value);
        case 2: return static_cast<u16>(value);
        case 4: return static_cast<u32>(value);
        case 8: return static_cast<u64>(value);
        }
        CROWY_ASSERT(false, "enum of unsupported width");
        return 0;
    }

    void storeEnum(void* member, usize size, i64 value){
        switch(size){
        case 1: *static_cast<u8*>(member) = static_cast<u8>(value); return;
        case 2: *static_cast<u16*>(member) = static_cast<u16>(value); return;
        case 4: *static_cast<u32*>(member) = static_cast<u32>(value); return;
        case 8: *static_cast<u64*>(member) = static_cast<u64>(value); return;
        }
        CROWY_ASSERT(false, "enum of unsupported width");
    }

    template<typename T>
    auto writeThrough(void* member, const DirtyCallback& onDirty){
        return [member, onDirty](UIContext&, T v){
            *static_cast<T*>(member) = std::move(v);
            onDirty();
        };
    }

    template<typename T, typename W>
    W makeField(
        const PropertyDesc& prop,
        void* member,
        const DirtyCallback& onDirty
    ){
        return W{
            .label = prop.name,
            .onChanged = writeThrough<T>(member, onDirty),
            .v = *static_cast<const T*>(member)
        };
    }

    Widget makeLeaf(
        const PropertyDesc& prop,
        void* member,
        const DirtyCallback& onDirty
    ){
        const auto* info = &prop.type;

        if(info == GetTypeOps<f32>()){
            if(prop.meta.uiRange.has_value()){
                return Slider{
                    .label = prop.name,
                    .onChanged = writeThrough<f32>(member, onDirty),
                    .v = *static_cast<const f32*>(member),
                    .v_min = prop.meta.uiRange->first,
                    .v_max = prop.meta.uiRange->second
                };
            }

            return makeField<f32, DragFloat>(prop, member, onDirty);
        }
        if(info == GetTypeOps<bool>()){
            return makeField<bool, Checkbox>(prop, member, onDirty);
        }
        if(info == GetTypeOps<i32>()){
            return makeField<i32, IntField>(prop, member, onDirty);
        }
        if(info == GetTypeOps<Vec2>()){
            return makeField<Vec2, Float2Field>(prop, member, onDirty);
        }
        if(info == GetTypeOps<Vec3>()){
            return makeField<Vec3, Float3Field>(prop, member, onDirty);
        }
        if(info == GetTypeOps<Vec4>()){
            return makeField<Vec4, Float4Field>(prop, member, onDirty);
        }
        if(info == GetTypeOps<Str>()){
            return SearchBar{
                .label = prop.name,
                .onChanged = [member, onDirty](UIContext&, StrView v){
                    *static_cast<Str*>(member) = v;
                    onDirty();
                },
                .str = *static_cast<const Str*>(member)
            };
        }
        if(info->enumerators != nullptr){
            const auto enumerators = info->enumerators();
            const auto size = info->size;
            const auto bits = loadEnumBits(member, size);

            std::vector<Str> entries;
            entries.reserve(enumerators.size());
            std::optional<usize> current;

            for(usize i = 0; i < enumerators.size(); ++i){
                entries.emplace_back(enumerators[i].name);
                if(!current.has_value() &&
                    enumBits(enumerators[i].value, size) == bits){
                    current = i;
                }
            }

            return Dropdown{
                .label = prop.name,
                .onChanged = [member, size, enumerators, onDirty](
                    UIContext&, usize index
                ){
                    storeEnum(member, size, enumerators[index].value);
                    onDirty();
                },
                .entries = std::move(entries),
                .current = current
            };
        }
        if(const auto* nested = NestedDesc(prop)){
            std::vector<Widget> children;
            appendProperties(children, member, *nested, onDirty);

            return Collapsing{
                .label = prop.name,
                .children = std::move(children),
                .defaultOpen = true,
                .scopeId = member
            };
        }

        return Text{.data = Str("unsupported: ") + info->name};
    }
}

namespace Crowy
{
    Widget buildPropertyTree(
        CStr label,
        void* target,
        const TypeDesc& desc,
        DirtyCallback onDirty
    ){
        std::vector<Widget> children;
        appendProperties(children, target, desc, onDirty);

        return Collapsing{
            .label = label,
            .children = std::move(children),
            .defaultOpen = true,
            .scopeId = target
        };
    }
}
