//
// Created by roman Sztergbaum on 21/06/2018.
//

#pragma once

#if defined(fmt)
#undef fmt
#include <sol/state.hpp>
#else
#include <sol/state.hpp>
#endif
#include <shiva/filesystem/filesystem.hpp>
#include <shiva/ecs/system.hpp>
#include <shiva/event/add_base_system.hpp>
#include <shiva/input/input.hpp>
#include <shiva/lua/lua_helpers.hpp>
#include "lua_scripted_system.hpp"

namespace sol
{
    template <>
    struct is_automagical<shiva::entt::entity_registry> : std::false_type
    {
    };
}

namespace shiva::scripting
{
    class lua_system : public shiva::ecs::logic_update_system<lua_system>
    {
    private:
        template <typename Component>
        void register_component()
        {
            using namespace std::string_literals;
            log_->info("register component: {}", Component::class_name());

            (*state_)[entity_registry_.class_name()][Component::class_name() + "_id"] = [](
                shiva::entt::entity_registry &self) {
                return self.type<Component>();
            };

            (*state_)[entity_registry_.class_name()]["for_each_entities_which_have_" + Component::class_name() +
                                                     "_component"] = [](
                shiva::entt::entity_registry &self, sol::function functor) {
                self.view<Component>().each(functor);
            };

            (*state_)[entity_registry_.class_name()]["get_"s + Component::class_name() + "_component"s] = [](
                shiva::entt::entity_registry &self,
                shiva::entt::entity_registry::entity_type entity) {
                return std::ref(self.get<Component>(entity));
            };

            (*state_)[entity_registry_.class_name()]["has_"s + Component::class_name() + "_component"s] = [](
                shiva::entt::entity_registry &self,
                shiva::entt::entity_registry::entity_type entity) {
                return self.has<Component>(entity);
            };

            (*state_)[entity_registry_.class_name()]["remove_"s + Component::class_name() + "_component"s] = [](
                shiva::entt::entity_registry &self,
                shiva::entt::entity_registry::entity_type entity) {
                self.remove<Component>(entity);
            };

            if constexpr (std::is_default_constructible_v<Component>) {
                (*state_)[entity_registry_.class_name()]["add_"s + Component::class_name() + "_component"s] = [](
                    shiva::entt::entity_registry &self,
                    shiva::entt::entity_registry::entity_type entity) {
                    return std::ref(self.assign<Component>(entity));
                };
            }
        }

        template<typename Event>
        void register_event()
        {
            using namespace std::string_literals;

            if constexpr (std::is_default_constructible_v<Event> && shiva::refl::has_constructor_arg_type_v<Event>) {
                std::cout << Event::class_name() << std::endl;
                (*state_)["dispatcher"]["trigger_"s + Event::class_name() + "_event"s] = [](
                    shiva::entt::dispatcher &self,
                    typename Event::constructor_arg_type_t arg) {
                    return self.trigger<Event>(arg);
                };
            } else {
                (*state_)["dispatcher"]["trigger_"s + Event::class_name() + "_event"s] = [](
                    shiva::entt::dispatcher &self) {
                    return self.trigger<Event>();
                };
            }
        }

    public:
        lua_system(entt::dispatcher &dispatcher,
                   entt::entity_registry &entity_registry,
                   const float &fixed_delta_time,
                   shiva::fs::path scripts_directory = shiva::fs::current_path() / "assets/scripts/lua",
                   shiva::fs::path systems_scripts_directory = shiva::fs::current_path() /
                                                               "assets/scripts/systems/lua") noexcept :
            system(dispatcher, entity_registry, fixed_delta_time),
            script_directory_(std::move(scripts_directory)),
            systems_scripts_directory_(std::move(systems_scripts_directory))
        {
            state_->open_libraries();
            state_->new_enum<shiva::ecs::system_type>("system_type",
                                                      {
                                                          {"pre_update",   shiva::ecs::system_type::pre_update},
                                                          {"post_update",  shiva::ecs::system_type::post_update},
                                                          {"logic_update", shiva::ecs::system_type::logic_update}
                                                      });
            state_->new_enum<shiva::input::keyboard::TKey>("Keyboard", KEYBOARD_INIT_LIST);
            (*state_)["load_script"] = [this](std::string filename, std::string path) {
                return this->load_script(std::move(filename), fs::path(std::move(path)));
            };
            register_entity_registry();
            register_components(shiva::ecs::common_components{});
            this->state_->new_usertype<shiva::entt::dispatcher>("dispatcher");
            register_events(shiva::event::common_events_list{});
            register_world();
            disable();
        }

        void register_entity_registry() noexcept
        {
            shiva::lua::register_type<shiva::entt::entity_registry>(*state_, log_);
            using comp_type = shiva::entt::entity_registry::component_type;
            (*state_)[entity_registry_.class_name()]["for_each_runtime"] = [](shiva::entt::entity_registry &self,
                                                                              std::vector<comp_type> array,
                                                                              sol::function functor) {
                return self.view(std::cbegin(array), std::cend(array)).each([func = std::move(functor)](auto entity) {
                    func(entity);
                });
            };

            (*state_)[entity_registry_.class_name()]["nb_entities"] = [](shiva::entt::entity_registry &self) {
                return self.size();
            };
        }

        void register_world() noexcept
        {
            (*state_)["shiva"] = state_->create_table_with("entity_registry", std::ref(entity_registry_),
                                                           "dispatcher", std::ref(dispatcher_),
                                                           "fixed_delta_time", fixed_delta_time_);
        }

        bool load_script(const std::string &file_name, const fs::path &script_directory) noexcept
        {
            try {
                state_->script_file((script_directory / fs::path(file_name)).string());
                log_->info("successfully register script: {}", file_name);
            } catch (const std::exception &e) {
                log_->error("error when loading script {0}: {1}\n script_directory {2}", file_name,
                            e.what(), script_directory.string());
                return false;
            }
            return true;
        }

        bool load_script(const std::string &file_name) noexcept
        {
            return load_script(file_name, script_directory_);
        }

        template <typename ...Types>
        void register_types_list(meta::type_list<Types...>) noexcept
        {
            (shiva::lua::register_type<Types>(*state_, log_), ...);
        }

        template <typename ... Types>
        void register_components(meta::type_list<Types...>)
        {
            (shiva::lua::register_type<Types>(*state_, log_), ...);
            (register_component<Types>(), ...);
        }

        template<typename ... Types>
        void register_events(meta::type_list<Types...>)
        {
            (shiva::lua::register_type<Types>(*state_, log_), ...);
            (register_event<Types>(), ...);
        }

        void update() noexcept override
        {
        }

        bool create_scripted_system(const shiva::fs::path &script_name)
        {
            bool res = load_script(script_name.filename().string(), systems_scripts_directory_);
            if (!res)
                return false;
            auto table_name = script_name.filename().stem().string() + "_table";
            log_->info("table: {}", table_name);
            shiva::ecs::system_type sys_type = (*state_)[table_name]["current_system_type"];
            log_->info("system_type: {}", sys_type);
            switch (sys_type) {
                case shiva::ecs::post_update:
                    dispatcher_.trigger<shiva::event::add_base_system>(
                        std::make_unique<shiva::ecs::lua_post_scripted_system>(dispatcher_, entity_registry_,
                                                                               fixed_delta_time_, state_, table_name,
                                                                               script_name.filename().stem().string()));
                    break;
                case shiva::ecs::pre_update:
                    dispatcher_.trigger<shiva::event::add_base_system>(
                        std::make_unique<shiva::ecs::lua_pre_scripted_system>(dispatcher_, entity_registry_,
                                                                              fixed_delta_time_, state_, table_name,
                                                                              script_name.filename().stem().string()));
                    break;
                case shiva::ecs::logic_update:
                    dispatcher_.trigger<shiva::event::add_base_system>(
                        std::make_unique<shiva::ecs::lua_logic_scripted_system>(dispatcher_, entity_registry_,
                                                                                fixed_delta_time_, state_, table_name,
                                                                                script_name.filename().stem().string()));
                    break;
                case shiva::ecs::size:
                    break;
                default:
                    break;
            }
            return res;
        }

        bool load_all_scripted_systems() noexcept
        {
            bool res = true;
            fs::recursive_directory_iterator endit;
            for (fs::recursive_directory_iterator it(systems_scripts_directory_); it != endit; ++it) {
                if (!fs::is_regular_file(*it)) {
                    continue;
                }
                log_->info("path -> {}", it->path().string());
                res &= create_scripted_system(it->path());
            }
            return true;
        }

    public:
        sol::state &get_state() noexcept
        {
            return *state_;
        }

    public:
        //! Reflection
        reflect_class(lua_system)

        static constexpr auto reflected_functions() noexcept
        {
            return meta::makeMap(reflect_function(&lua_system::update));
        }

        static constexpr auto reflected_members() noexcept
        {
            return meta::makeMap();
        }

    private:
        std::shared_ptr<sol::state> state_{std::make_shared<sol::state>()};
        shiva::fs::path script_directory_;
        shiva::fs::path systems_scripts_directory_;
    };
}
