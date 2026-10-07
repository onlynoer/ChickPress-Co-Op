#include "AudioDevice.hpp"
#include "logicHandler.hpp"

int main() {
    raylib::Window window(800, 600, "CS381 - Assignment 9", FLAG_WINDOW_RESIZABLE);
    raylib::AudioDevice audio;
    rlSetClipPlanes(0.1, 5000);

    LogicHandler handler;
    handler.Init();

    while(!window.ShouldClose()) {
        float dt = window.GetFrameTime();
        handler.Update(dt);

        window.BeginDrawing(); {
            window.ClearBackground(raylib::Color::RayWhite());
        
            handler.Render();
            
            // window.DrawFPS();
        } window.EndDrawing();
    }
}