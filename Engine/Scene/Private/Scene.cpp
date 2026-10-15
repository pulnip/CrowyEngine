#include "Scene.hpp"

#include <algorithm>
#include <format>
#include <fstream>
#include <initializer_list>
#include <optional>
#include <stdexcept>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include "ActorRef.hpp"
#include "Assert.hpp"
#include "ClassRegistry.hpp"
#include "Component.hpp"
#include "ErasedValue.hpp"
#include "JsonLoader.hpp"

namespace Crowy
{
    namespace
    {
        using Errors = std::vector<Str>;

        constexpr i64 SceneVersion = 1;
        constexpr CStr SceneType = "scene";
        // shares a component's table with its properties
        constexpr CStr TypeKey = "type";

        bool hasProperties(const TypeDesc& desc) {
            for(const auto* chain = &desc; chain != nullptr;
                chain = chain->parent) {
                if(!chain->properties.empty())
                    return true;
            }
            return false;
        }

        [[noreturn]] void throwErrors(StrView what, const Errors& errors) {
            auto message = Str(what);
            for(const auto& error: errors)
                message += std::format("\n{}", error);

            throw std::runtime_error(message);
        }

        void checkKeys(
            const DOM::Table& table,
            std::initializer_list<StrView> known,
            StrView where,
            Errors& errors
        ) {
            for(const auto& [key, value]: table) {
                if(std::ranges::find(known, StrView(key)) == known.end())
                    errors.push_back(
                        std::format("{} has no key '{}'", where, key)
                    );
            }
        }

        DOM::Array saveComponents(
            const World& world,
            ActorId actor,
            Errors& errors
        ) {
            struct Saved {
                const TypeDesc* desc = nullptr;
                DOM::Value value;
            };

            const auto id = world.FileId(actor);
            std::vector<Saved> saved;
            world.EachComponent(
                actor,
                [&](const ComponentInfo& info, const void* row) {
                    if(info.transient)
                        return;

                    const auto& desc = *info.desc;
                    if(!hasProperties(desc)) {
                        errors.push_back(
                            std::format(
                                "actor {}: '{}' has no properties to save; "
                                "reflect them or mark it Transient",
                                id,
                                desc.name
                            )
                        );
                        return;
                    }
                    if(desc.FindInChain(TypeKey) != nullptr) {
                        errors.push_back(
                            std::format(
                                "actor {}: '{}' has a property '{}', the key a "
                                "component's type is written under",
                                id,
                                desc.name,
                                TypeKey
                            )
                        );
                        return;
                    }

                    DOM::Value properties;
                    SerializeProperties(desc, row, properties);

                    DOM::Table component;
                    component.emplace(TypeKey, desc.name);
                    for(auto& [key, value]: *properties.asTable())
                        component.emplace(key, std::move(value));

                    saved.push_back({&desc, DOM::Value(std::move(component))});
                }
            );

            // the World meets types in an order its file would not keep
            std::ranges::sort(saved, {}, [](const Saved& entry) {
                return StrView(entry.desc->name);
            });

            DOM::Array components;
            for(auto& entry: saved)
                components.push_back(std::move(entry.value));

            return components;
        }

        // spawned from the file; its components load once every id is bound
        struct LoadedActor {
            ActorId actor;
            i64 id = 0;
            DOM::Array* components = nullptr;
        };

        void loadComponents(
            World& world,
            const LoadedActor& loaded,
            Errors& errors
        ) {
            std::unordered_set<const TypeDesc*> given;
            for(usize i = 0; i < loaded.components->size(); ++i) {
                auto& component = (*loaded.components)[i];
                auto* fields = component.asTable();
                if(fields == nullptr) {
                    errors.push_back(
                        std::format(
                            "actor {}: components[{}] is not a table",
                            loaded.id,
                            i
                        )
                    );
                    continue;
                }

                const auto typeEntry = fields->find(TypeKey);
                const Str* typeName = typeEntry == fields->end()
                                          ? nullptr
                                          : typeEntry->second.asString();
                if(typeName == nullptr) {
                    errors.push_back(
                        std::format(
                            "actor {}: components[{}] has no '{}' string",
                            loaded.id,
                            i,
                            TypeKey
                        )
                    );
                    continue;
                }
                const auto type = *typeName;

                const auto* desc = ClassRegistry::FindType(type);
                const auto* info = desc != nullptr
                                       ? ComponentRegistry::Get().Find(*desc)
                                       : nullptr;
                if(info == nullptr) {
                    errors.push_back(
                        std::format(
                            "actor {}: '{}' is not a component",
                            loaded.id,
                            type
                        )
                    );
                    continue;
                }
                if(info->transient) {
                    errors.push_back(
                        std::format(
                            "actor {}: '{}' is never saved, so no file gives "
                            "it",
                            loaded.id,
                            type
                        )
                    );
                    continue;
                }
                if(!given.insert(desc).second) {
                    errors.push_back(
                        std::format(
                            "actor {}: '{}' is given twice",
                            loaded.id,
                            type
                        )
                    );
                    continue;
                }
                if(desc->ops->construct == nullptr) {
                    errors.push_back(
                        std::format(
                            "actor {}: '{}' has no default constructor to load "
                            "into",
                            loaded.id,
                            type
                        )
                    );
                    continue;
                }

                fields->erase(TypeKey);
                ErasedValue value(*desc->ops);
                for(const auto& error:
                    ApplyProperties(*desc, value.Get(), component)) {
                    errors.push_back(
                        std::format("actor {}: {}: {}", loaded.id, type, error)
                    );
                }
                world.Add(loaded.actor, value);
            }
        }

        void checkHeader(const DOM::Value& scene) {
            const auto* root = scene.asTable();
            if(root == nullptr)
                throw std::runtime_error("a scene is a table");

            // another format altogether: nothing below would read right
            const auto type = scene.get<Str>("metadata.type");
            if(type != SceneType) {
                throw std::runtime_error(
                    std::format(
                        "expected a \"{}\" document, got \"{}\"",
                        SceneType,
                        type.value_or("")
                    )
                );
            }
            const auto version = scene.get<i64>("metadata.version");
            if(version != SceneVersion) {
                throw std::runtime_error(
                    std::format(
                        "a scene of version {} cannot be read; this reads "
                        "version {}",
                        version.value_or(0),
                        SceneVersion
                    )
                );
            }
        }
    }

    DOM::Value saveScene(const World& world) {
        std::vector<std::pair<ActorId, ActorId>> actors;
        world.EachActor([&](ActorId actor, ActorId parent) {
            actors.emplace_back(actor, parent);
        });

        // a reference writes the id of the actor it names
        ActorRefScope scope;
        for(const auto& [actor, parent]: actors)
            scope.Bind(world.FileId(actor), actor);

        Errors errors;
        DOM::Array rows;
        for(const auto& [actor, parent]: actors) {
            DOM::Value transform;
            SerializeValue(
                *GetTypeOps<Transform>(),
                &world.LocalTransform(actor),
                transform
            );

            DOM::Table row;
            row.emplace("id", world.FileId(actor));
            row.emplace("name", world.Name(actor));
            row.emplace(
                "parent",
                parent.IsValid() ? DOM::Value(world.FileId(parent))
                                 : DOM::Value()
            );
            row.emplace("transform", std::move(transform));
            row.emplace("components", saveComponents(world, actor, errors));
            rows.emplace_back(std::move(row));
        }
        if(!errors.empty())
            throwErrors("the scene cannot be saved:", errors);

        DOM::Table metadata;
        metadata.emplace("version", SceneVersion);
        metadata.emplace("type", SceneType);

        DOM::Table root;
        root.emplace("metadata", std::move(metadata));
        root.emplace("actors", std::move(rows));

        return DOM::Value(std::move(root));
    }

    void loadScene(World& world, DOM::Value scene) {
        bool empty = true;
        world.EachActor([&](ActorId, ActorId) { empty = false; });
        CROWY_ASSERT(empty, "a scene loads into an empty World");

        checkHeader(scene);
        auto& root = *scene.asTable();

        Errors errors;
        checkKeys(root, {"metadata", "actors"}, "the scene", errors);
        // the header check found its type, so the metadata is a table
        checkKeys(
            *root.find("metadata")->second.asTable(),
            {"version", "type", "name"},
            "the scene's metadata",
            errors
        );

        const auto actorsEntry = root.find("actors");
        auto* actors =
            actorsEntry == root.end() ? nullptr : actorsEntry->second.asArray();
        if(actors == nullptr)
            throw std::runtime_error("the scene has no 'actors' array");

        // the whole file is bound before a component reads a reference
        ActorRefScope scope;
        std::unordered_map<i64, ActorId> actorOfId;
        std::vector<ActorId> roots;
        std::vector<LoadedActor> loaded;

        for(usize i = 0; i < actors->size(); ++i) {
            auto* fields = (*actors)[i].asTable();
            const auto where = std::format("actors[{}]", i);
            if(fields == nullptr) {
                errors.push_back(std::format("{} is not a table", where));
                continue;
            }
            checkKeys(
                *fields,
                {"id", "name", "parent", "transform", "components"},
                where,
                errors
            );

            const auto idEntry = fields->find("id");
            const auto id = idEntry == fields->end() ? std::nullopt
                                                     : idEntry->second.asInt();
            if(!id.has_value() || *id < 1) {
                errors.push_back(
                    std::format("{}: 'id' expects an integer from 1", where)
                );
                continue;
            }
            if(actorOfId.contains(*id)) {
                errors.push_back(
                    std::format("{}: id {} is given twice", where, *id)
                );
                continue;
            }

            auto parent = ActorId::InvalidHandle();
            if(const auto entry = fields->find("parent");
               entry != fields->end() && !entry->second.is_none()) {
                const auto parentId = entry->second.asInt();
                if(!parentId.has_value() || !actorOfId.contains(*parentId)) {
                    errors.push_back(
                        std::format(
                            "actor {}: the parent is no actor before it",
                            *id
                        )
                    );
                    continue;
                }
                parent = actorOfId.at(*parentId);
            }

            auto local = Transform::Identity();
            if(const auto entry = fields->find("transform");
               entry != fields->end()) {
                for(const auto& error: ApplyValue(
                        *GetTypeOps<Transform>(),
                        &local,
                        entry->second,
                        "transform"
                    )) {
                    errors.push_back(std::format("actor {}: {}", *id, error));
                }
            }

            Str name;
            if(const auto entry = fields->find("name");
               entry != fields->end()) {
                if(const auto* text = entry->second.asString())
                    name = *text;
                else
                    errors.push_back(
                        std::format("actor {}: 'name' expects a string", *id)
                    );
            }

            DOM::Array* components = nullptr;
            if(const auto entry = fields->find("components");
               entry != fields->end()) {
                components = entry->second.asArray();
                if(components == nullptr)
                    errors.push_back(
                        std::format(
                            "actor {}: 'components' expects an array",
                            *id
                        )
                    );
            }

            const auto actor = world.Spawn(local, parent);
            if(!parent.IsValid())
                roots.push_back(actor);
            world.SetName(actor, std::move(name));
            world.SetFileId(actor, *id);
            scope.Bind(*id, actor);
            actorOfId.emplace(*id, actor);

            if(components != nullptr)
                loaded.push_back({actor, *id, components});
        }

        for(const auto& actor: loaded)
            loadComponents(world, actor, errors);

        if(!errors.empty()) {
            // the components added die with their actors at the next Sync
            for(const auto actor: roots)
                world.Destroy(actor);

            throwErrors("the scene cannot be loaded:", errors);
        }
    }

    void saveSceneFile(const World& world, const std::filesystem::path& path) {
        const auto text = emitJson(saveScene(world), JsonStyle::Pretty) + "\n";

        std::ofstream file(path, std::ios::binary);
        file << text;
        if(!file) {
            throw std::runtime_error(
                std::format("cannot write scene file: {}", path.string())
            );
        }
    }

    void loadSceneFile(World& world, const std::filesystem::path& path) {
        loadScene(world, parseJsonFile(path));
    }
}
