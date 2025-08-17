#pragma once

#include <functional>
#include <vector>
#include "core/ecs/entity_manager.hpp"
#include "core/ecs/component_storage.hpp"

namespace luster::ecs
{
    // A very simple system runner that iterates entities with required components
    template <typename... Components>
    class System
    {
    public:
        using Func = std::function<void(Entity, Components&...)>;

        explicit System(Func f) : func_(std::move(f)) {}

        void update(const std::vector<Entity>& entities, ComponentStorage& storage)
        {
            for (Entity e : entities)
            {
                if ((storage.has<Components>(e) && ...))
                {
                    func_(e, *storage.get<Components>(e)...);
                }
            }
        }

    private:
        Func func_;
    };
}


