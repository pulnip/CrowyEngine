#include "PropertyWalker.hpp"

#include <format>
#include <optional>
#include <utility>
#include <vector>

#include "JsonLoader.hpp"

namespace
{
    using namespace Crowy;

    // where a value is found again at every edit, from its owner as it is
    // then; nullptr when it is gone. Widgets start on the values seen at build
    struct ValueTarget {
        PropertyTarget::Writer write;
    };

    Widget makeValue(
        Str label,
        const TypeOps& type,
        const PropertyMeta& meta,
        const ValueTarget& target,
        const void* member,
        const DirtyCallback& onDirty
    );

    ValueTarget memberOf(
        const ValueTarget& owner,
        const PropertyAccessor* accessor
    ) {
        return ValueTarget{.write = [write = owner.write, accessor]() -> void* {
            const auto object = write();
            return object == nullptr ? nullptr : accessor->Get(object);
        }};
    }

    // an element past the list's length as it is then is gone
    ValueTarget elementOf(
        const ValueTarget& list,
        const ListOps& ops,
        usize index
    ) {
        return ValueTarget{
            .write = [write = list.write, &ops, index]() -> void* {
                const auto items = write();
                return items != nullptr && index < ops.count(items)
                           ? ops.element(items, index)
                           : nullptr;
            }
        };
    }

    // seed is the owner as resolved at build, for the values widgets start on
    void appendProperties(
        std::vector<Widget>& out,
        const ValueTarget& owner,
        const void* seed,
        const TypeDesc& desc,
        const DirtyCallback& onDirty
    ) {
        if(desc.parent != nullptr) {
            appendProperties(out, owner, seed, *desc.parent, onDirty);
        }

        for(const auto& prop: desc.properties) {
            const auto* accessor = prop.accessor.get();
            out.push_back(makeValue(
                prop.name,
                prop.type,
                prop.meta,
                memberOf(owner, accessor),
                accessor->Get(seed),
                onDirty
            ));
        }
    }

    // an edit to a value that is gone is dropped
    template<typename V, typename F>
    auto editThrough(
        const ValueTarget& target,
        const DirtyCallback& onDirty,
        F write
    ) {
        return [target, onDirty, write](UIContext&, V v) {
            auto member = target.write();
            if(member == nullptr)
                return;

            write(member, std::move(v));
            onDirty();
        };
    }

    template<typename T>
    auto writeThrough(const ValueTarget& target, const DirtyCallback& onDirty) {
        return editThrough<T>(target, onDirty, [](void* member, T v) {
            *static_cast<T*>(member) = std::move(v);
        });
    }

    template<typename T, typename W>
    W makeField(
        Str label,
        const ValueTarget& target,
        const void* member,
        const DirtyCallback& onDirty
    ) {
        return W{
            .label = std::move(label),
            .onChanged = writeThrough<T>(target, onDirty),
            .v = *static_cast<const T*>(member)
        };
    }

    // a length change through the tree; the tree shows it at its next build
    TextButton resizeButton(
        Str label,
        const ValueTarget& target,
        const ListOps& ops,
        isize step,
        const DirtyCallback& onDirty
    ) {
        return TextButton{
            .label = std::move(label),
            .onPressed = [target, &ops, step, onDirty](UIContext&) {
                auto items = target.write();
                if(items == nullptr)
                    return;

                const auto count = static_cast<isize>(ops.count(items));
                if(count + step < 0)
                    return;

                ops.resize(items, static_cast<usize>(count + step));
                onDirty();
            }
        };
    }

    Widget makeValue(
        Str label,
        const TypeOps& type,
        const PropertyMeta& meta,
        const ValueTarget& target,
        const void* member,
        const DirtyCallback& onDirty
    ) {
        const auto* info = &type;

        if(info == getTypeOps<f32>()) {
            if(meta.uiRange.has_value()) {
                return Slider{
                    .label = std::move(label),
                    .onChanged = writeThrough<f32>(target, onDirty),
                    .v = *static_cast<const f32*>(member),
                    .v_min = meta.uiRange->first,
                    .v_max = meta.uiRange->second
                };
            }

            return makeField<f32, DragFloat>(label, target, member, onDirty);
        }
        if(info == getTypeOps<bool>()) {
            return makeField<bool, Checkbox>(label, target, member, onDirty);
        }
        if(info == getTypeOps<i32>()) {
            return makeField<i32, IntField>(label, target, member, onDirty);
        }
        if(info == getTypeOps<Vec2>()) {
            return makeField<Vec2, Float2Field>(label, target, member, onDirty);
        }
        if(info == getTypeOps<Vec3>()) {
            return makeField<Vec3, Float3Field>(label, target, member, onDirty);
        }
        if(info == getTypeOps<Vec4>()) {
            return makeField<Vec4, Float4Field>(label, target, member, onDirty);
        }
        if(info == getTypeOps<Str>()) {
            return SearchBar{
                .label = std::move(label),
                .onChanged = editThrough<StrView>(
                    target,
                    onDirty,
                    [](void* field, StrView v) {
                        *static_cast<Str*>(field) = v;
                    }
                ),
                .str = *static_cast<const Str*>(member)
            };
        }
        if(info->enumerators != nullptr) {
            const auto enumerators = info->enumerators();
            const auto value = info->enumLoad(member);

            std::vector<Str> entries;
            entries.reserve(enumerators.size());
            std::optional<usize> current;

            for(usize i = 0; i < enumerators.size(); ++i) {
                entries.emplace_back(enumerators[i].name);
                if(!current.has_value() && enumerators[i].value == value) {
                    current = i;
                }
            }

            return Dropdown{
                .label = std::move(label),
                .onChanged = editThrough<usize>(
                    target,
                    onDirty,
                    [info, enumerators](void* field, usize index) {
                        info->enumStore(field, enumerators[index].value);
                    }
                ),
                .entries = std::move(entries),
                .current = current
            };
        }
        if(const auto* nested = nestedDesc(type)) {
            std::vector<Widget> children;
            appendProperties(children, target, member, *nested, onDirty);

            return Collapsing{
                .label = std::move(label),
                .children = std::move(children),
                .defaultOpen = true,
                .scopeId = member
            };
        }
        if(const auto* list = info->list) {
            const auto& element = *list->elementOps();
            const auto count = list->count(member);

            // each element shares the list's metadata, so a range ranges
            // every element
            std::vector<Widget> children;
            children.reserve(count + 2);
            for(usize i = 0; i < count; ++i) {
                children.push_back(makeValue(
                    std::format("[{}]", i),
                    element,
                    meta,
                    elementOf(target, *list, i),
                    list->constElement(member, i),
                    onDirty
                ));
            }
            children.push_back(resizeButton("+", target, *list, 1, onDirty));
            children.push_back(resizeButton("-", target, *list, -1, onDirty));

            return Collapsing{
                .label = std::format("{} ({})", label, count),
                .children = std::move(children),
                .defaultOpen = true,
                .scopeId = member
            };
        }
        // a leaf with no widget of its own still shows what it holds
        if(info->serialize != nullptr) {
            DOM::Value value;
            info->serialize(member, value);

            return Text{.data = std::format("{}: {}", label, emitJson(value))};
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
        // getDesc made an empty desc: the registration was never linked
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
        const ValueTarget owner{.write = [target] { return target.Write(); }};
        appendProperties(children, owner, seed, target.Desc(), onDirty);

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
