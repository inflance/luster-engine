#pragma once

#include <unordered_map>
#include <typeindex>
#include <memory>
#include <vector>
#include <functional>
#include "core/ecs/types.hpp"

namespace luster::ecs
{
    struct IComponentArray
    {
        virtual ~IComponentArray() = default;
        virtual void entityDestroyed(Entity e) = 0;
    };

    // Sparse-set style: dense values + sparse map (entity->index)
    template <typename T>
    class ComponentArray : public IComponentArray
    {
    public:
        void insert(Entity e, const T& component)
        {
            if (has(e)) { dense_[sparse_[e]] = component; return; }
            sparse_[e] = static_cast<std::uint32_t>(dense_.size());
            denseEntities_.push_back(e);
            dense_.push_back(component);
        }
        void remove(Entity e)
        {
            auto it = sparse_.find(e);
            if (it == sparse_.end()) return;
            const std::uint32_t idx = it->second;
            const std::uint32_t lastIdx = static_cast<std::uint32_t>(dense_.size() - 1);
            // swap-remove
            std::swap(dense_[idx], dense_[lastIdx]);
            std::swap(denseEntities_[idx], denseEntities_[lastIdx]);
            sparse_[denseEntities_[idx]] = idx;
            dense_.pop_back();
            denseEntities_.pop_back();
            sparse_.erase(it);
        }
        bool has(Entity e) const { return sparse_.find(e) != sparse_.end(); }
        T* get(Entity e)
        {
            auto it = sparse_.find(e);
            return it == sparse_.end() ? nullptr : &dense_[it->second];
        }
        const T* get(Entity e) const
        {
            auto it = sparse_.find(e);
            return it == sparse_.end() ? nullptr : &dense_[it->second];
        }
        void entityDestroyed(Entity e) override { remove(e); }

        // iterate packed
        const std::vector<T>& packed() const { return dense_; }
        const std::vector<Entity>& entities() const { return denseEntities_; }

    private:
        std::unordered_map<Entity, std::uint32_t> sparse_{};
        std::vector<T> dense_{};
        std::vector<Entity> denseEntities_{};
    };

    class ComponentStorage
    {
    public:
        template <typename T>
        void add(Entity e, const T& component)
        {
            auto& arr = getOrCreateArray<T>();
            arr.insert(e, component);
            notifyConstruct<T>(e);
        }

        template <typename T>
        void remove(Entity e)
        {
            auto arr = getArray<T>();
            if (arr && arr->has(e))
            {
                arr->remove(e);
                notifyRemove<T>(e);
            }
        }

        template <typename T>
        T* get(Entity e)
        {
            auto arr = getArray<T>();
            return arr ? arr->get(e) : nullptr;
        }

        template <typename T>
        bool has(Entity e) const
        {
            auto arr = getArray<T>();
            return arr ? arr->has(e) : false;
        }

        void entityDestroyed(Entity e)
        {
            for (auto& [_, arr] : arrays_) arr->entityDestroyed(e);
        }

        // subscriptions
        template <typename T>
        using Callback = std::function<void(Entity)>;

        template <typename T>
        void onConstruct(Callback<T> cb) { constructSubs_[componentTypeId<T>()].push_back(std::move(cb)); }
        template <typename T>
        void onRemove(Callback<T> cb) { removeSubs_[componentTypeId<T>()].push_back(std::move(cb)); }

    private:
        template <typename T>
        ComponentArray<T>& getOrCreateArray()
        {
            auto key = std::type_index(typeid(T));
            auto it = arrays_.find(key);
            if (it == arrays_.end())
            {
                auto ptr = std::make_unique<ComponentArray<T>>();
                auto* raw = ptr.get();
                arrays_[key] = std::move(ptr);
                return *raw;
            }
            return *static_cast<ComponentArray<T>*>(it->second.get());
        }

        template <typename T>
        ComponentArray<T>* getArray() const
        {
            auto key = std::type_index(typeid(T));
            auto it = arrays_.find(key);
            if (it == arrays_.end()) return nullptr;
            return static_cast<ComponentArray<T>*>(it->second.get());
        }

        std::unordered_map<std::type_index, std::unique_ptr<IComponentArray>> arrays_{};
        std::unordered_map<std::size_t, std::vector<std::function<void(Entity)>>> constructSubs_{};
        std::unordered_map<std::size_t, std::vector<std::function<void(Entity)>>> removeSubs_{};

        template <typename T>
        void notifyConstruct(Entity e)
        {
            auto it = constructSubs_.find(componentTypeId<T>());
            if (it == constructSubs_.end()) return;
            for (auto& cb : it->second) cb(e);
        }
        template <typename T>
        void notifyRemove(Entity e)
        {
            auto it = removeSubs_.find(componentTypeId<T>());
            if (it == removeSubs_.end()) return;
            for (auto& cb : it->second) cb(e);
        }
    };
}


