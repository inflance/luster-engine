#pragma once

#include <unordered_set>
#include <vector>
#include "core/ecs/entity_manager.hpp"
#include "core/ecs/component_storage.hpp"

namespace luster::ecs
{
    class Registry
    {
    public:
        // Small view type that iterates entities having Components...
        template <typename... Components>
        class View
        {
        public:
            View(const std::vector<Entity>& all, ComponentStorage& storage)
                : all_(all), storage_(storage) {}
            struct Iterator
            {
                const std::vector<Entity>* all;
                ComponentStorage* storage;
                std::size_t i;
                void advance()
                {
                    while (i < all->size() && !(storage->template has<Components>((*all)[i]) && ...)) ++i;
                }
                Iterator& operator++() { ++i; advance(); return *this; }
                bool operator!=(const Iterator& other) const { return i != other.i; }
                Entity operator*() const { return (*all)[i]; }
            };
            Iterator begin() { Iterator it{ &all_, &storage_, 0 }; it.advance(); return it; }
            Iterator end() { return Iterator{ &all_, &storage_, all_.size() }; }
        private:
            const std::vector<Entity>& all_;
            ComponentStorage& storage_;
        };
        Entity create()
        {
            Entity e = entities_.create();
            alive_.insert(e);
            order_.push_back(e);
            return e;
        }

        void destroy(Entity e)
        {
            if (!alive_.count(e)) return;
            storage_.entityDestroyed(e);
            entities_.destroy(e);
            alive_.erase(e);
            // keep order_ simple; not compacted for O(1) remove
        }

        template <typename T>
        void add(Entity e, const T& c) { storage_.add<T>(e, c); }
        template <typename T>
        void remove(Entity e) { storage_.remove<T>(e); }
        template <typename T>
        T* get(Entity e) { return storage_.get<T>(e); }
        template <typename T>
        bool has(Entity e) const { return storage_.has<T>(e); }

        const std::vector<Entity>& entities() const { return order_; }
        ComponentStorage& storage() { return storage_; }

        template <typename... Components>
        View<Components...> view() { return View<Components...>(order_, storage_); }

    private:
        EntityManager entities_{};
        ComponentStorage storage_{};
        std::unordered_set<Entity> alive_{};
        std::vector<Entity> order_{};
    };
}


