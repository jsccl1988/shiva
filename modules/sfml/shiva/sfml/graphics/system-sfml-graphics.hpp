//
// Created by roman Sztergbaum on 15/06/2018.
//

#pragma once

#include <SFML/Graphics.hpp>
#include <shiva/ecs/system.hpp>
#include <shiva/event/after_load_systems_plugins.hpp>
#include <shiva/sfml/event/set_render_window.hpp>

namespace shiva::plugins
{
    class render_system : public shiva::ecs::post_update_system<render_system>
    {
    public:
        void receive([[maybe_unused]] const shiva::event::after_load_systems_plugins &evt)
        {
            this->dispatcher_.trigger<shiva::sfml::event::set_render_window>(win_);
        }

        ~render_system() noexcept override
        {
            //dispatcher_.sink<shiva::event::after_load_systems_plugins>().disconnect(this);
        }

        render_system(shiva::entt::dispatcher& dispatcher, shiva::entt::entity_registry &registry, const float& fixed_delta_time) noexcept :
        system(dispatcher, registry, fixed_delta_time)
        {
            dispatcher_.sink<shiva::event::after_load_systems_plugins>().connect(this);
        }

        static std::unique_ptr<shiva::ecs::base_system> system_creator(entt::dispatcher &dispatcher,
                                                                       entt::entity_registry &registry,
                                                                       const float& fixed_delta_time) noexcept;
        void update() noexcept final;


    public:
        reflect_class(render_system)

        static constexpr auto reflected_functions() noexcept
        {
            return meta::makeMap(reflect_function(&render_system::update));
        }

        static constexpr auto reflected_members() noexcept
        {
            return meta::makeMap();
        }

    private:
        //TODO: Parse a config file
        sf::RenderWindow win_{sf::VideoMode(800, 600), "SFML window"};
    };
}