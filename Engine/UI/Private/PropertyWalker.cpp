#include "PropertyWalker.hpp"

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
