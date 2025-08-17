#pragma once

#include <queue>
#include "core/ecs/types.hpp"

namespace luster::ecs
{
    class EntityManager
    {
    public:
        EntityManager()
        {
            freeList_ = {};
        }

        Entity create()
        {
            if (!freeList_.empty())
            {
                Entity id = freeList_.front();
                freeList_.pop();
                return id;
            }
            return next_++;
        }

        void destroy(Entity id)
        {
            freeList_.push(id);
        }

    private:
        Entity next_ = 1;
        std::queue<Entity> freeList_;
    };
}


