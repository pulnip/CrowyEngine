#include "PropertyWalker.hpp"

#include <optional>
#include <utility>
#include <vector>

namespace
{
    using namespace Crowy;

    Widget makeLeaf(
        const PropertyDesc&,
        const PropertyTarget& owner,
        const void* seed,
        const DirtyCallback&
    );

    // seed is the owner as resolved at build, for the values widgets start on
    void appendProperties(
        std::vector<Widget>& out,
        const PropertyTarget& owner,
        const void* seed,
        const TypeDesc& desc,
        const DirtyCallback& onDirty
    ) {
        if(desc.parent != nullptr){
            appendProperties(out, owner, seed, *desc.parent, onDirty);
        }

        for(const auto& prop: desc.properties){
            out.push_back(makeLeaf(prop, owner, seed, onDirty));
        }
    }

    // the member is found again from its owner at the moment of the edit;
    // an edit to a target that is gone is dropped
    template<typename V, typename F>
    auto editThrough(
        const PropertyTarget& owner,
        const PropertyAccessor* accessor,
        const DirtyCallback& onDirty,
        F write
    ) {
        return [owner, accessor, onDirty, write](UIContext&, V v) {
            auto object = owner.Write();
            if(object == nullptr)
                return;

            write(accessor->Get(object), std::move(v));
            onDirty();
        };
    }

    template<typename T>
    auto writeThrough(
        const PropertyTarget& owner,
        const PropertyAccessor* accessor,
        const DirtyCallback& onDirty
    ) {
        return editThrough<T>(owner, accessor, onDirty, [](void* member, T v) {
            *static_cast<T*>(member) = std::move(v);
        });
    }

    template<typename T, typename W>
    W makeField(
        const PropertyDesc& prop,
        const PropertyTarget& owner,
        const void* member,
        const DirtyCallback& onDirty
    ) {
        return W{
            .label = prop.name,
            .onChanged = writeThrough<T>(owner, prop.accessor.get(), onDirty),
            .v = *static_cast<const T*>(member)
        };
    }

    Widget makeLeaf(
        const PropertyDesc& prop,
        const PropertyTarget& owner,
        const void* seed,
        const DirtyCallback& onDirty
    ) {
        const auto* info = &prop.type;
        const auto* accessor = prop.accessor.get();
        const auto member = accessor->Get(seed);

        if(info == GetTypeOps<f32>()){
            if(prop.meta.uiRange.has_value()){
                return Slider{
                    .label = prop.name,
                    .onChanged = writeThrough<f32>(owner, accessor, onDirty),
                    .v = *static_cast<const f32*>(member),
                    .v_min = prop.meta.uiRange->first,
                    .v_max = prop.meta.uiRange->second
                };
            }

            return makeField<f32, DragFloat>(prop, owner, member, onDirty);
        }
        if(info == GetTypeOps<bool>()){
            return makeField<bool, Checkbox>(prop, owner, member, onDirty);
        }
        if(info == GetTypeOps<i32>()){
            return makeField<i32, IntField>(prop, owner, member, onDirty);
        }
        if(info == GetTypeOps<Vec2>()){
            return makeField<Vec2, Float2Field>(prop, owner, member, onDirty);
        }
        if(info == GetTypeOps<Vec3>()){
            return makeField<Vec3, Float3Field>(prop, owner, member, onDirty);
        }
        if(info == GetTypeOps<Vec4>()){
            return makeField<Vec4, Float4Field>(prop, owner, member, onDirty);
        }
        if(info == GetTypeOps<Str>()){
            return SearchBar{
                .label = prop.name,
                .onChanged = editThrough<StrView>(
                    owner,
                    accessor,
                    onDirty,
                    [](void* target, StrView v) {
                        *static_cast<Str*>(target) = v;
                    }
                ),
                .str = *static_cast<const Str*>(member)
            };
        }
        if(info->enumerators != nullptr){
            const auto enumerators = info->enumerators();
            const auto value = info->enumLoad(member);

            std::vector<Str> entries;
            entries.reserve(enumerators.size());
            std::optional<usize> current;

            for(usize i = 0; i < enumerators.size(); ++i){
                entries.emplace_back(enumerators[i].name);
                if(!current.has_value() && enumerators[i].value == value){
                    current = i;
                }
            }

            return Dropdown{
                .label = prop.name,
                .onChanged = editThrough<usize>(
                    owner,
                    accessor,
                    onDirty,
                    [info, enumerators](void* target, usize index) {
                        info->enumStore(target, enumerators[index].value);
                    }
                ),
                .entries = std::move(entries),
                .current = current
            };
        }
        if(const auto* nested = NestedDesc(prop)){
            const PropertyTarget inner(
                [owner, accessor]() -> const void* {
                    const auto object = owner.Read();
                    return object == nullptr ? nullptr : accessor->Get(object);
                },
                [owner, accessor]() -> void* {
                    const auto object = owner.Write();
                    return object == nullptr ? nullptr : accessor->Get(object);
                },
                *nested
            );

            std::vector<Widget> children;
            appendProperties(children, inner, member, *nested, onDirty);

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
        PropertyTarget target,
        DirtyCallback onDirty
    ) {
        // GetDesc made an empty desc: the registration was never linked
        CROWY_ASSERT(
            !target.Desc().name.empty(),
            "a type that was never registered"
        );

        auto seed = target.Read();
        if(seed == nullptr) {
            return Collapsing{
                .label = label,
                .children = {Text{.data = "gone"}},
                .defaultOpen = true
            };
        }

        std::vector<Widget> children;
        appendProperties(children, target, seed, target.Desc(), onDirty);

        // the address as of this build; a row that moves before the next
        // build only loses its open state
        return Collapsing{
            .label = label,
            .children = std::move(children),
            .defaultOpen = true,
            .scopeId = seed
        };
    }

    Widget buildPropertyTree(
        CStr label,
        void* target,
        const TypeDesc& desc,
        DirtyCallback onDirty
    ) {
        return buildPropertyTree(
            label,
            PropertyTarget::Fixed(target, desc),
            std::move(onDirty)
        );
    }
}
