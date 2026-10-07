#pragma once

#include "Camera3D.hpp"
#include "Keyboard.hpp"
#include "Matrix.hpp"
#include "Model.hpp"
#include "RadiansDegrees.hpp"
#include "Vector3.hpp"
#include "Vector4.hpp"
#include "raylib-cpp.hpp"
#include "raylib.h"
#include <concepts>
#include <execution>
#include <limits>
#include <optional>
#include <memory>
#include <numeric>
#include <vector>

extern size_t globalComponentCounter;

template<typename T>
size_t GetComponentID(/* T reference = {} */){
    static size_t id = globalComponentCounter++;
    return id;
}

raylib::Degree angle_normalize(raylib::Degree angle);

using entity = size_t;

struct ComponentStorageBase {
    virtual ~ComponentStorageBase() {}
    virtual size_t element_size() { return 0; }

    virtual void* Get(size_t index) = 0;
    virtual void* GetOrAllocate(size_t index) = 0;
};

template<typename Tcomponent>
struct ComponentStorage : public ComponentStorageBase, std::vector<Tcomponent> {
    //Inherit from vector 
    using std::vector<Tcomponent>::vector;

    size_t element_size() override { return sizeof(Tcomponent); }

    void* Get(size_t index) override { 
        return &this->at(index);
    }

    void* GetOrAllocate(size_t index) override {
        if(this->size() <= index)
            this->resize(index + 1);
        return Get(index);
    }
};

struct Context {
    std::vector<std::vector<bool>> entityMasks;
    std::vector<std::shared_ptr<ComponentStorageBase>> storages = {nullptr};
    float deltaTime = 0;
    entity selected = 0;

    template<typename Tcomponent>
    ComponentStorageBase& GetStorage() {
        size_t id = GetComponentID<Tcomponent>();
        if(storages.size() <= id)
            storages.insert(
                storages.end(), 
                std::max<int64_t>(id - storages.size(), 1), 
                nullptr
            );
        if(!storages[id] || storages[id]->element_size() == 0)
            storages[id] = std::make_shared<ComponentStorage<Tcomponent>>();
        return *storages[id];
    }

    entity CreateEntity() {
        entity e = entityMasks.size();
        entityMasks.emplace_back(std::vector<bool>{false});
        return e;
    }

    // EC Homework: how do we remove entities?

    template<typename Tcomponent>
    Tcomponent& AddComponent(entity e) {
        size_t id = GetComponentID<Tcomponent>();
        auto& mask = entityMasks[e];
        if(mask.size() <= id)
            mask.resize(id + 1, false);
        mask[id] = true;
        return *(Tcomponent*)GetStorage<Tcomponent>().GetOrAllocate(e);
    }

    template<typename Tcomponent>
    Tcomponent& GetComponent(entity e) {
        size_t id = GetComponentID<Tcomponent>();
        assert(HasComponent<Tcomponent>(e));
        return *(Tcomponent*)GetStorage<Tcomponent>().Get(e);
    }

    template<typename Tcomponent>
    bool HasComponent(entity e) {
        size_t id = GetComponentID<Tcomponent>();
        return entityMasks.size() > e && entityMasks[e].size() > id && entityMasks[e][id];
    }
};

struct RenderComponent {
    raylib::Model* model = nullptr;
    bool drawBoundingBox = false;
    int materialIndex = 1;
    raylib::Color tint = WHITE;
    bool useTint = false;
};

struct TransformComponent {
    raylib::Vector3 position = {0, 0, 0};
    raylib::Quaternion rotation = raylib::Quaternion::Identity();
};

struct KinematicsComponent {
    raylib::Vector3 velocity = {0, 0, 0};
    float speed = 0;
    float targetSpeed = 0;
    float maxSpeed = 0;
    float acceleration = 0;
    float moveInput = 0;
};

struct Physics2DComponent {
    raylib::Degree heading = 0;
    float turningRate = 0;
    float turnInput = 0;
};

struct Physics3DComponent {
    raylib::Quaternion rotation = raylib::Quaternion::Identity();
    raylib::Vector3 eulerDegrees = {0, 0, 0}; // pitch, yaw, roll
    float turningRate = 0;
    raylib::Vector3 turnInput = {0, 0, 0}; // pitch, yaw, roll
};

void RenderSystem(Context& ctx, entity e);
void SelectionSystem(Context& ctx, entity e);
void KinematicsSystem(Context& ctx, entity e);
void Physics2DSystem(Context& ctx, entity e);
void Physics3DSystem(Context& ctx, entity e);

template <typename T>
auto sequential(T func) {
    return [func](Context& ctx) {
        // Bulk process
        for(entity e = 0; e < ctx.entityMasks.size(); ++e) {
            func(ctx, e);
        }
    };
}

template <typename T>
auto parallel(T func) {
    return [func](Context& ctx) {
        std::vector<entity> entities(ctx.entityMasks.size());
        std::iota(entities.begin(), entities.end(), 0);
        std::for_each(std::execution::par_unseq, entities.begin(), entities.end(), [func, &ctx](entity e){
            func(ctx, e);
        });
    };
}

template<std::invocable<Context&>... Tsystems>
auto sequential(Tsystems... systems) {
    return [=](Context& ctx) {
        (systems(ctx), ...);
    };
}
