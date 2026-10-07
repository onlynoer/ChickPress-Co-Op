#pragma once

#include "Music.hpp"
#include "component.hpp"
#include "raylib-cpp.hpp"
#include "raylib.h"
#include <BufferedRaylib.hpp>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "skybox.hpp"

class NetworkRuntime;

struct ButtonData {
    entity entityIndex = 0;
    bool isGood = false;
    bool wasTouching = false;
};

struct PlayerInput {
  float move = 0.0f;
  float turn = 0.0f;
  bool brakePressed = false;
};

class LogicHandler {
  public:
    LogicHandler();
    ~LogicHandler();
    void Init();
    void Update(float deltaTime);
    void Render();

  private:
    enum class GameStatus {
        MENU,
        PLAYING,
        GAMEOVER
    };

    enum class NetworkMode {
      NONE,
      HOST,
      CLIENT
    };

    void ResetRun();
    void ScatterButtons();
    void HandleButtonCollisions();
    void DrawUI();
    void UpdateMenuInput();

    entity CreatePlayerEntity(const raylib::Vector3& position);
    void ApplyInputToPlayer(entity target, const PlayerInput& playerInput);

    void StartHosting();
    void StartJoining();
    void StopNetworking();
    void PollNetwork();
    void BroadcastHostState();
    void ApplyServerSnapshot(const std::string& line);

    Context ctx;
    raylib::Camera camera;
    raylib::Model chicken;
    raylib::Model button;
    raylib::Model ground;
    raylib::Texture ground_texture;
    cs381::SkyBox skybox;
    raylib::BufferedInput input;
    std::function<void(Context&)> schedule;
    std::vector<ButtonData> buttons;

    std::unordered_map<int, entity> playerEntities;
    std::unordered_map<int, PlayerInput> hostInputByPlayerId;

    entity playerEntity = 0;
    int localPlayerId = 0;

    int score = 0;
    int health = 100;
    float timeLeft = 20.0f;
    bool won = false;
    float lastDt = 0.0f;
    GameStatus gamestate = GameStatus::MENU;
    NetworkMode networkMode = NetworkMode::NONE;
    bool ipFieldActive = false;
    std::string joinIp = "127.0.0.1";
    std::string netStatus = "Choose Host or Join";
    std::unique_ptr<NetworkRuntime> networkRuntime;

    raylib::Music bgMusic;
    raylib::Sound btnPress;
    raylib::Sound explosion;
};

raylib::BufferedInput setupInput(Context& ctx);