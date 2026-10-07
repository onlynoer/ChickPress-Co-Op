#include "component.hpp"
#include "BoundingBox.hpp"
#include <algorithm>

size_t globalComponentCounter = 0;

raylib::Degree angle_normalize(raylib::Degree angle){
    float decimal = float(angle) - int(angle);
    int whole = int(angle) % 360; // goes from -360-360
    whole += (angle < 0) * 360; // forces 0-360
    return decimal + whole;
}

void RenderSystem(Context& ctx, entity e) { // basicly drawFunc
    if(!ctx.HasComponent<RenderComponent>(e)) return;
    if(!ctx.HasComponent<TransformComponent>(e)) return;

    auto& render = ctx.GetComponent<RenderComponent>(e);
    auto& transform = ctx.GetComponent<TransformComponent>(e);

    raylib::Transform backupTransform = render.model->transform;
    raylib::Color backupColor = WHITE;

    if(render.useTint && render.model->materialCount > 0){
        int materialIndex = std::clamp(render.materialIndex, 0, render.model->materialCount - 1);
        backupColor = render.model->materials[materialIndex].maps[MATERIAL_MAP_DIFFUSE].color;
        render.model->materials[materialIndex].maps[MATERIAL_MAP_DIFFUSE].color = render.tint;
    }

    render.model->transform = raylib::Transform(render.model->transform)
        .Translate(transform.position)
        .Rotate(transform.rotation);

    render.model->Draw({0, 0, 0});

    if(render.drawBoundingBox){
        raylib::BoundingBox box = render.model->GetTransformedBoundingBox();
        box.Draw(RED);
    }

    if(render.useTint && render.model->materialCount > 0){
        int materialIndex = std::clamp(render.materialIndex, 0, render.model->materialCount - 1);
        render.model->materials[materialIndex].maps[MATERIAL_MAP_DIFFUSE].color = backupColor;
    }

    render.model->transform = backupTransform;
}

//for 2d phys
raylib::Vector3 ForwardFromHeading(raylib::Degree heading){
    return raylib::Vector3(
        cos(heading.RadianValue()),
        0,
        -sin(heading.RadianValue())
    );
}

//for 3d phys
raylib::Vector3 ForwardFromRotation(const raylib::Quaternion& rotation){
    Matrix rot = QuaternionToMatrix(rotation);
    return Vector3Normalize(Vector3Transform({1, 0, 0}, rot));
}

void SelectionSystem(Context& ctx, entity e){
    if(!ctx.HasComponent<RenderComponent>(e)) return;
    auto& render = ctx.GetComponent<RenderComponent>(e);
    render.drawBoundingBox = (e == ctx.selected);
}

void Physics2DSystem(Context& ctx, entity e){
    if(!ctx.HasComponent<TransformComponent>(e)) return;
    if(!ctx.HasComponent<Physics2DComponent>(e)) return;

    auto& transform = ctx.GetComponent<TransformComponent>(e);
    auto& physics = ctx.GetComponent<Physics2DComponent>(e);

    physics.heading += physics.turnInput * physics.turningRate * ctx.deltaTime;
    physics.heading = angle_normalize(physics.heading);

    transform.rotation = raylib::Quaternion::FromEuler({0, physics.heading.RadianValue(), 0});
}

void Physics3DSystem(Context& ctx, entity e){
    if(!ctx.HasComponent<TransformComponent>(e)) return;
    if(!ctx.HasComponent<Physics3DComponent>(e)) return;

    auto& transform = ctx.GetComponent<TransformComponent>(e);
    auto& physics = ctx.GetComponent<Physics3DComponent>(e);

    physics.eulerDegrees.x += physics.turnInput.x * physics.turningRate * ctx.deltaTime;
    physics.eulerDegrees.y += physics.turnInput.y * physics.turningRate * ctx.deltaTime;
    physics.eulerDegrees.z += physics.turnInput.z * physics.turningRate * ctx.deltaTime;

    raylib::Vector3 eulerRadians = {
        DEG2RAD * physics.eulerDegrees.x,
        DEG2RAD * physics.eulerDegrees.y,
        DEG2RAD * physics.eulerDegrees.z
    };

    physics.rotation = raylib::Quaternion::FromEuler(eulerRadians);
    transform.rotation = physics.rotation;
}

void KinematicsSystem(Context& ctx, entity e){
    if(!ctx.HasComponent<TransformComponent>(e)) return;
    if(!ctx.HasComponent<KinematicsComponent>(e)) return;

    auto& transform = ctx.GetComponent<TransformComponent>(e);
    auto& kine = ctx.GetComponent<KinematicsComponent>(e);

    //if input then perform the respective acceleration, else slow down to rest
    if(kine.moveInput != 0.0f){
        // Hold W/S to accelerate toward forward/reverse max speed.
        kine.targetSpeed += kine.moveInput * kine.acceleration * ctx.deltaTime;
    }else{
        // No throttle input: naturally coast back toward rest.
        if(kine.targetSpeed > 0.0f){
            kine.targetSpeed = std::max(0.0f, kine.targetSpeed - kine.acceleration * ctx.deltaTime);
        }else if(kine.targetSpeed < 0.0f){
            kine.targetSpeed = std::min(0.0f, kine.targetSpeed + kine.acceleration * ctx.deltaTime);
        }
    }

    kine.targetSpeed = std::clamp(kine.targetSpeed, -kine.maxSpeed, kine.maxSpeed);

    if(kine.speed < kine.targetSpeed){
        kine.speed += kine.acceleration * ctx.deltaTime;
        if(kine.speed > kine.targetSpeed) kine.speed = kine.targetSpeed;
    }else if(kine.speed > kine.targetSpeed){
        kine.speed -= kine.acceleration * ctx.deltaTime;
        if(kine.speed < kine.targetSpeed) kine.speed = kine.targetSpeed;
    }

    raylib::Vector3 forward = {1, 0, 0};
    
    if(ctx.HasComponent<Physics2DComponent>(e)){
        auto& physics2d = ctx.GetComponent<Physics2DComponent>(e);
        forward = ForwardFromHeading(physics2d.heading);
    }else if(ctx.HasComponent<Physics3DComponent>(e)){
        auto& physics3d = ctx.GetComponent<Physics3DComponent>(e);
        forward = ForwardFromRotation(physics3d.rotation);
    }else{
        forward = ForwardFromRotation(transform.rotation);
    }

    kine.velocity = forward * kine.speed;
    transform.position += kine.velocity * ctx.deltaTime;
}
