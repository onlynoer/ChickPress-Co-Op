#include "logicHandler.hpp"
#include "Color.hpp"
#include "Model.hpp"
#include "Music.hpp"
#include "Vector3.hpp"
#include "component.hpp"
#include "network.hpp"
#include "raylib.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <iomanip>
#include <sstream>

raylib::BoundingBox GetEntityBoundingBox(Context& ctx, entity e) {
    auto& render = ctx.GetComponent<RenderComponent>(e);
    auto& transform = ctx.GetComponent<TransformComponent>(e);

    raylib::Transform backupTransform = render.model->transform;
    render.model->transform = raylib::Transform(render.model->transform)
        .Translate(transform.position)
        .Rotate(transform.rotation);

    raylib::BoundingBox box = render.model->GetTransformedBoundingBox();
    render.model->transform = backupTransform;
    return box;
}

bool DrawCenteredButton(const char* label, int y, int width, int height) {
    int screenW = GetScreenWidth();
    int x = screenW / 2 - width / 2;
    raylib::Rectangle rect = {(float)x, (float)y, (float)width, (float)height};

    raylib::Vector2 mouse = GetMousePosition();
    bool hover = CheckCollisionPointRec(mouse, rect);
    raylib::Color bg = hover ? Color{40, 40, 40, 255} : Color{20, 20, 20, 255};
    DrawRectangleRec(rect, bg);
    DrawRectangleLines((int)rect.x, (int)rect.y, (int)rect.width, (int)rect.height, WHITE);

    int fontSize = 24;
    int textWidth = MeasureText(label, fontSize);
    DrawText(label, (int)(rect.x + rect.width * 0.5f - textWidth * 0.5f), (int)(rect.y + rect.height * 0.5f - fontSize * 0.5f), fontSize, WHITE);

    return hover && IsMouseButtonPressed(MOUSE_BUTTON_LEFT);
}

LogicHandler::LogicHandler() = default;
LogicHandler::~LogicHandler() = default;

raylib::BufferedInput setupInput(Context& ctx){
    raylib::BufferedInput input;

    input["jump"] = raylib::Action::key(KEY_SPACE).AddPressedCallback([&ctx]() {
        if(!ctx.HasComponent<KinematicsComponent>(ctx.selected)) return;

        auto& kine = ctx.GetComponent<KinematicsComponent>(ctx.selected);
        kine.targetSpeed = 0;
        kine.moveInput = 0;
    }).move();

    return input;
}

entity LogicHandler::CreatePlayerEntity(const raylib::Vector3& position) {
    entity newPlayer = ctx.CreateEntity();

    auto& render = ctx.AddComponent<RenderComponent>(newPlayer);
    render.model = &chicken;

    ctx.AddComponent<TransformComponent>(newPlayer).position = position;

    auto& kine = ctx.AddComponent<KinematicsComponent>(newPlayer);
    kine.maxSpeed = 550.0f;
    kine.acceleration = 324.0f;
    kine.targetSpeed = 0.0f;

    auto& phys2d = ctx.AddComponent<Physics2DComponent>(newPlayer);
    phys2d.heading = raylib::Degree(0);
    phys2d.turningRate = 180.0f;

    return newPlayer;
}

void LogicHandler::ApplyInputToPlayer(entity target, const PlayerInput& playerInput) {
    if(!ctx.HasComponent<KinematicsComponent>(target) || !ctx.HasComponent<Physics2DComponent>(target)) return;

    auto& kine = ctx.GetComponent<KinematicsComponent>(target);
    auto& phys = ctx.GetComponent<Physics2DComponent>(target);

    kine.moveInput = playerInput.move;
    phys.turnInput = playerInput.turn;

    if(playerInput.brakePressed) {
        kine.moveInput = 0.0f;
        kine.targetSpeed = 0.0f;
    }
}

void LogicHandler::Init(){
    // std::srand((unsigned)(std::time(nullptr))); //for random stuff

    btnPress = raylib::Sound("audio/button_press.mp3");
    explosion = raylib::Sound("audio/explosion.mp3");

    button = raylib::Model("models/button.glb");
    button.transform = raylib::Transform(button.transform).Scale(10);

    chicken = raylib::Model("models/chicken.glb");
    chicken.transform = raylib::Transform(chicken.transform).Scale(25).RotateY(PI/2);

    camera = raylib::Camera({0, 120, 900}, {0, 0, 0});
    camera.SetFovy(camera.GetFovy() + 10);

    ground = raylib::Mesh::Plane(10000, 10000, 50, 50, 25).LoadModelFrom();
    ground_texture = raylib::Texture("textures/grass.jpg");
    ground.materials[0].maps[MATERIAL_MAP_DIFFUSE].texture = ground_texture;

    skybox.Load("textures/skybox.png");

    bgMusic = raylib::Music("audio/explosion.mp3");
    bgMusic.SetLooping(true);
    bgMusic.Play();
    bgMusic.SetVolume(0.5);

    playerEntity = CreatePlayerEntity({-100, 50, 200});
    localPlayerId = 0;
    playerEntities.clear();
    playerEntities[localPlayerId] = playerEntity;

    const int buttonCount = 5;
    buttons.clear();
    buttons.reserve(buttonCount);

    for(int i = 0; i < buttonCount; ++i) {
        entity buttonEntity = ctx.CreateEntity();

        auto& buttonRender = ctx.AddComponent<RenderComponent>(buttonEntity);
        buttonRender.model = &button;
        buttonRender.materialIndex = 1;
        buttonRender.tint = WHITE;
        buttonRender.useTint = true;

        ctx.AddComponent<TransformComponent>(buttonEntity).position = raylib::Vector3{0, 0, 200};

        auto& buttonKine = ctx.AddComponent<KinematicsComponent>(buttonEntity);
        buttonKine.maxSpeed = 0.0f;
        buttonKine.acceleration = 0.0f;

        auto& buttonPhys = ctx.AddComponent<Physics2DComponent>(buttonEntity);
        buttonPhys.turningRate = 0.0f;

        buttons.push_back({buttonEntity, false, false});
    }

    ctx.selected = playerEntity;

    schedule = sequential(
        parallel(SelectionSystem),
        parallel(Physics2DSystem),
        parallel(Physics3DSystem),
        parallel(KinematicsSystem),
        sequential(RenderSystem)
    );

    input = setupInput(ctx);
    ResetRun();
    gamestate = GameStatus::MENU;
    netStatus = "Choose Host or Join";
}

void LogicHandler::ResetRun() {
    score = 0;
    health = 100;
    timeLeft = 20.0f;
    won = false;

    int spawnIndex = 0;
    for(const auto& playerPair : playerEntities) {
        entity e = playerPair.second;
        if(!ctx.HasComponent<TransformComponent>(e)) continue;

        auto& trans = ctx.GetComponent<TransformComponent>(e);
        auto& kine = ctx.GetComponent<KinematicsComponent>(e);
        auto& phys = ctx.GetComponent<Physics2DComponent>(e);

        trans.position = raylib::Vector3{-100.0f + 120.0f * spawnIndex, 50.0f, 200.0f};
        kine.speed = 0.0f;
        kine.targetSpeed = 0.0f;
        kine.moveInput = 0.0f;
        phys.turnInput = 0.0f;
        phys.heading = raylib::Degree(0);
        spawnIndex++;
    }

    hostInputByPlayerId.clear();
    for(const auto& playerPair : playerEntities) {
        int id = playerPair.first;
        hostInputByPlayerId[id] = {};
    }

    for(auto& buttonData : buttons) {
        buttonData.wasTouching = false;
    }

    ctx.selected = playerEntity;
    ScatterButtons();
}

void LogicHandler::ScatterButtons() {
    auto randRange = [](float minVal, float maxVal) {
        float t = (float)(std::rand()) / (float)(RAND_MAX);
        return minVal + (maxVal - minVal) * t;
    };

    if(buttons.empty()) return;

    int goodIndex = std::rand() % (int)buttons.size();
    for(size_t i = 0; i < buttons.size(); ++i) {
        buttons[i].isGood = ((int)i == goodIndex);
        buttons[i].wasTouching = false;

        auto& render = ctx.GetComponent<RenderComponent>(buttons[i].entityIndex);
        auto& transform = ctx.GetComponent<TransformComponent>(buttons[i].entityIndex);
        auto& kine = ctx.GetComponent<KinematicsComponent>(buttons[i].entityIndex);
        auto& phys = ctx.GetComponent<Physics2DComponent>(buttons[i].entityIndex);

        render.tint = WHITE;

        transform.position = raylib::Vector3{
            randRange(-800.0f, 800.0f),
            0.0f,
            randRange(-800.0f, 800.0f)
        };

        kine.speed = 0.0f;
        kine.targetSpeed = 0.0f;
        kine.moveInput = 0.0f;
        kine.acceleration = 0.0f;
        kine.maxSpeed = 0.0f;

        phys.turnInput = 0.0f;
        phys.turningRate = 0.0f;

        if(randRange(0.0f, 1000.0f) > 500.0f) {
            float speed = randRange(20.0f, 400.0f);
            kine.speed = speed;
            kine.targetSpeed = speed;
            kine.moveInput = 1.0f;
            kine.acceleration = 250.0f;
            kine.maxSpeed = speed;

            phys.turnInput = 1.0f;
            phys.turningRate = randRange(20.0f, 200.0f);
        }
    }
}

void LogicHandler::HandleButtonCollisions() {
    if(gamestate != GameStatus::PLAYING) return;

    bool needScatter = false;

    for(auto& buttonData : buttons) {
        auto& buttonRender = ctx.GetComponent<RenderComponent>(buttonData.entityIndex);
        raylib::BoundingBox buttonBox = GetEntityBoundingBox(ctx, buttonData.entityIndex);

        bool touchingAnyPlayer = false;
        for(const auto& playerPair : playerEntities) {
            entity player = playerPair.second;

            if(!ctx.HasComponent<RenderComponent>(player) || !ctx.HasComponent<TransformComponent>(player)) continue;

            raylib::BoundingBox chickenBox = GetEntityBoundingBox(ctx, player);
            if(CheckCollisionBoxes(chickenBox, buttonBox)) {
                touchingAnyPlayer = true;
                break;
            }
        }

        if(touchingAnyPlayer && !buttonData.wasTouching) {
            btnPress.Play();

            if(buttonData.isGood) {
                buttonRender.tint = GREEN;
                score += 10;
                timeLeft += 5.0f;
                if(score >= 100) won = true;
                needScatter = true;
            } else {
                buttonRender.tint = RED;
                explosion.Play();
                health = std::max(0, health - 10);
            }
        }

        buttonData.wasTouching = touchingAnyPlayer;
    }

    if(health <= 0 || timeLeft <= 0.0f || won) {
        gamestate = GameStatus::GAMEOVER;

        for(const auto& playerPair : playerEntities) {
            entity player = playerPair.second;
            auto& kine = ctx.GetComponent<KinematicsComponent>(player);
            auto& phys = ctx.GetComponent<Physics2DComponent>(player);
            kine.moveInput = 0.0f;
            kine.targetSpeed = 0.0f;
            phys.turnInput = 0.0f;
        }
    }

    if(needScatter && gamestate == GameStatus::PLAYING) {
        ScatterButtons();
    }
}

void LogicHandler::UpdateMenuInput() {
    int ch = GetCharPressed();
    while(ch > 0) {
        //char 32 is space and 126 is tilda
        if(ipFieldActive && ch >= 32 && ch <= 126 && joinIp.size() < 63) {
            joinIp.push_back((char)ch);
        }
        ch = GetCharPressed();
    }

    if(ipFieldActive && IsKeyPressed(KEY_BACKSPACE) && !joinIp.empty()) {
        joinIp.pop_back();
    }
}

void LogicHandler::BroadcastHostState() {
    //Host sends the whole game snapshot each frame to clients keeping the m in in sync.
    // Snapshot starts with shared stats then player states then button states.

    std::ostringstream state;
    state << std::fixed << std::setprecision(3); //set floats to 3 decimals
    int wonInt = won ? 1 : 0;
    state << "STATE " << score << ' ' << health << ' ' << timeLeft << ' ' << wonInt << ' ' << (int)gamestate; //convert gamestate to int so we dont have problems

    state << ' ' << playerEntities.size();
    for(const auto& playerPair : playerEntities) { // playerPair is id, entity
        int id = playerPair.first;
        entity player = playerPair.second;
        if(!ctx.HasComponent<TransformComponent>(player) || !ctx.HasComponent<Physics2DComponent>(player) || !ctx.HasComponent<KinematicsComponent>(player)) continue;

        const auto& trans = ctx.GetComponent<TransformComponent>(player);
        const auto& phys = ctx.GetComponent<Physics2DComponent>(player);
        const auto& kine = ctx.GetComponent<KinematicsComponent>(player);

        state << ' ' << id << ' ' << trans.position.x << ' ' << trans.position.y << ' ' << trans.position.z << ' ' << float(phys.heading) << ' ' << kine.speed;
    }

    state << ' ' << buttons.size();
    for(const auto& buttonData : buttons) {
        const auto& transform = ctx.GetComponent<TransformComponent>(buttonData.entityIndex);
        const auto& render = ctx.GetComponent<RenderComponent>(buttonData.entityIndex);

        state << ' ' << transform.position.x << ' ' << transform.position.y << ' ' << transform.position.z << ' ' << (int)render.tint.r << ' ' << (int)render.tint.g << ' ' << (int)render.tint.b << ' ' << (int)render.tint.a << ' ' << (buttonData.isGood ? 1 : 0);
    }

    state << '\n';
    std::string out = state.str();
    if(networkRuntime) {
        networkRuntime->SendToAllClients(out); //pass the snapshot to all clients
    }
}

void LogicHandler::ApplyServerSnapshot(const std::string& line) {
    // client read snapshot sent by host | host is server

    std::istringstream iss(line);
    std::string tag;
    iss >> tag;

    if(tag == "WELCOME") { //WELCOME is like a handshake agreement
        int assignedId = 0;
        iss >> assignedId;
        if(assignedId != localPlayerId) {
            entity localEntity = playerEntity;
            playerEntities.erase(localPlayerId);
            localPlayerId = assignedId;
            playerEntities[localPlayerId] = localEntity;
        }
        netStatus = "Connected as player " + std::to_string(localPlayerId);
        return;
    }

    if(tag != "STATE") return;

    int wonInt = 0;
    int gameInt = 0;
    size_t playerCount = 0;

    iss >> score >> health >> timeLeft >> wonInt >> gameInt;
    won = (wonInt != 0);
    gamestate = (GameStatus)gameInt;

    iss >> playerCount;
    for(size_t i = 0; i < playerCount; ++i) {
        int id = 0;
        float x = 0;
        float y = 0;
        float z = 0;
        float heading = 0;
        float speed = 0;
        iss >> id >> x >> y >> z >> heading >> speed;

        entity player = 0;
        auto foundPlayer = playerEntities.find(id);
        if(foundPlayer == playerEntities.end()) { //if cant find then create
            player = CreatePlayerEntity({x, y, z});
            playerEntities[id] = player;
        } else {
            player = foundPlayer->second;
        }

        auto& trans = ctx.GetComponent<TransformComponent>(player);
        auto& phys = ctx.GetComponent<Physics2DComponent>(player);
        auto& kine = ctx.GetComponent<KinematicsComponent>(player);

        trans.position = raylib::Vector3{x, y, z};
        phys.heading = raylib::Degree(heading);
        phys.turnInput = 0.0f;
        kine.speed = speed;
        kine.targetSpeed = speed;
        kine.moveInput = 0.0f;

        if(id == localPlayerId) {
            playerEntity = player;
            ctx.selected = player;
        }
    }

    size_t buttonCount = 0;
    iss >> buttonCount;
    size_t useBtnCount = std::min(buttonCount, buttons.size());
    for(size_t i = 0; i < useBtnCount; ++i) {
        float x = 0;
        float y = 0;
        float z = 0;
        int r = 255;
        int g = 255;
        int b = 255;
        int a = 255;
        int isGoodInt = 0;

        iss >> x >> y >> z >> r >> g >> b >> a >> isGoodInt;

        auto& trans = ctx.GetComponent<TransformComponent>(buttons[i].entityIndex);
        auto& render = ctx.GetComponent<RenderComponent>(buttons[i].entityIndex);

        trans.position = raylib::Vector3{x, y, z};
        render.tint = raylib::Color{(unsigned char)r, (unsigned char)g, (unsigned char)b, (unsigned char)a}; //fixed with raylib::Color //still throws without casting //{(unsigned char)r, (unsigned char)g, (unsigned char)b, (unsigned char)a}
        buttons[i].isGood = (isGoodInt != 0);
    }
}

void LogicHandler::StartHosting() {
    StopNetworking();

    try {
        networkRuntime = std::make_unique<NetworkRuntime>(); //makes the server 
        networkMode = NetworkMode::HOST;
        netStatus = "Hosting on port 9999";

        networkRuntime->StartHosting(9999,
            [this](int playerId) {
                entity player = CreatePlayerEntity({-100.0f + 100.0f * (float)playerId, 50.0f, 200.0f});
                playerEntities[playerId] = player;
                hostInputByPlayerId[playerId] = {};
                netStatus = "Client connected";
            },
            [this](int playerId) {
                playerEntities.erase(playerId);
                hostInputByPlayerId.erase(playerId);
                netStatus = "Client disconnected";
            },
            [this](int playerId, const std::string& line) { //onClientLine is when server gets input from client(s)
                if(!line.starts_with("INPUT ")) return;

                std::istringstream iss(line);
                std::string prefix;
                PlayerInput received{};
                int brake = 0;
                iss >> prefix >> received.move >> received.turn >> brake;
                received.brakePressed = (brake != 0);
                hostInputByPlayerId[playerId] = received;
            }
        );
    } catch(const std::exception& e) {
        networkMode = NetworkMode::NONE;
        netStatus = std::string("Host failed");
    }
}

void LogicHandler::StartJoining() {
    StopNetworking();

    try {
        networkRuntime = std::make_unique<NetworkRuntime>();
        networkMode = NetworkMode::CLIENT;
        netStatus = "Connected to " + joinIp;

        networkRuntime->StartJoining(
            joinIp,
            9999,
            [this](const std::string& line) {
                ApplyServerSnapshot(line);
            },
            [this]() {
                netStatus = "Disconnected from host";
                networkMode = NetworkMode::NONE;
                gamestate = GameStatus::MENU;
            }
        );
    } catch(const std::exception& e) {
        networkMode = NetworkMode::NONE;
        netStatus = std::string("Join failed");
    }
}

void LogicHandler::StopNetworking() {
    if(networkRuntime) {
        networkRuntime->Stop();
    }

    networkRuntime.reset();
    networkMode = NetworkMode::NONE;
}

void LogicHandler::PollNetwork() {
    if(networkRuntime) {
        networkRuntime->Poll();
    }
}

void LogicHandler::DrawUI() {
    if(gamestate == GameStatus::PLAYING) {
        char scoreText[64];
        std::snprintf(scoreText, sizeof(scoreText), "Score: %d", score);
        DrawText(scoreText, 10, 10, 24, RED);

        char healthText[64];
        std::snprintf(healthText, sizeof(healthText), "Health: %d", health);
        DrawText(healthText, 10, 38, 24, RED);

        char timerText[64];
        std::snprintf(timerText, sizeof(timerText), "Timer: %.1f", timeLeft);
        DrawText(timerText, GetScreenWidth() / 2 - MeasureText(timerText, 24) / 2, 10, 24, RED);

        DrawText(netStatus.c_str(), 10, GetScreenHeight() - 28, 20, WHITE); //debugging info
        return;
    }

    DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(BLACK, 0.6f));

    if(gamestate == GameStatus::MENU) {
        const char* title = "ChickPress Co-op";
        int titleSize = 42;
        DrawText(title, GetScreenWidth() / 2 - MeasureText(title, titleSize) / 2, 90, titleSize, WHITE);

        DrawText("Join IP:", GetScreenWidth() / 2 - 150, 176, 24, WHITE);

        raylib::Rectangle ipRect = {(float)GetScreenWidth() / 2 - 70.0f, 170.0f, 260.0f, 38.0f};
        if(IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            ipFieldActive = CheckCollisionPointRec(GetMousePosition(), ipRect);
        }

        DrawRectangleRec(ipRect, Color{20, 20, 20, 255});
        DrawRectangleLines((int)ipRect.x, (int)ipRect.y, (int)ipRect.width, (int)ipRect.height, ipFieldActive ? GREEN : WHITE);

        std::string shownIp = joinIp;
        
        DrawText(shownIp.c_str(), (int)ipRect.x + 8, (int)ipRect.y + 8, 22, WHITE);

        DrawText(netStatus.c_str(), GetScreenWidth() / 2 - MeasureText(netStatus.c_str(), 20) / 2, 222, 20, LIGHTGRAY);

        if(DrawCenteredButton("Host Game", 260, 220, 50)) {
            StartHosting();
            ResetRun();
            gamestate = GameStatus::PLAYING;
        }

        if(DrawCenteredButton("Join Game", 322, 220, 50)) {
            StartJoining();
            if(networkMode == NetworkMode::CLIENT) {
                gamestate = GameStatus::PLAYING;
            }
        }
        return;
    }

    const char* title = won ? "You Win" : "Game Over";
    int titleSize = 42;
    DrawText(title, GetScreenWidth() / 2 - MeasureText(title, titleSize) / 2, 110, titleSize, won ? GREEN : RED);

    char scoreText[64];
    std::snprintf(scoreText, sizeof(scoreText), "Score: %d", score);
    DrawText(scoreText, GetScreenWidth() / 2 - MeasureText(scoreText, 24) / 2, 170, 24, WHITE);

    char timeText[64];
    std::snprintf(timeText, sizeof(timeText), "Time Left: %.1f", timeLeft);
    DrawText(timeText, GetScreenWidth() / 2 - MeasureText(timeText, 24) / 2, 200, 24, WHITE);

    if(networkMode != NetworkMode::CLIENT) {
        if(DrawCenteredButton("Play Again", 260, 220, 50)) {
            ResetRun();
            gamestate = GameStatus::PLAYING;
        }
    } else {
        DrawText("Waiting for host...", GetScreenWidth() / 2 - MeasureText("Waiting for host...", 24) / 2, 260, 24, WHITE);
    }
}

void LogicHandler::Update(float deltaTime){
    lastDt = deltaTime;
    PollNetwork();

    if(gamestate == GameStatus::MENU) {
        UpdateMenuInput();
    }

    PlayerInput localInput{};
    localInput.move = (IsKeyDown(KEY_W) || IsKeyDown(KEY_UP) ? 1.0f : 0.0f) - (IsKeyDown(KEY_S) || IsKeyDown(KEY_DOWN) ? 1.0f : 0.0f);
    localInput.turn = (IsKeyDown(KEY_A) || IsKeyDown(KEY_LEFT) ? 1.0f : 0.0f) - (IsKeyDown(KEY_D) || IsKeyDown(KEY_RIGHT) ? 1.0f : 0.0f);
    localInput.brakePressed = IsKeyPressed(KEY_SPACE);

    if(gamestate == GameStatus::PLAYING) {
        if(networkMode == NetworkMode::CLIENT) {
            ctx.deltaTime = 0.0f;
            if(networkRuntime) {
                std::ostringstream out;
                out << "INPUT " << localInput.move << ' ' << localInput.turn << ' ' << (localInput.brakePressed ? 1 : 0) << '\n';
                networkRuntime->SendToServer(out.str());
            }
        } else {
            ctx.deltaTime = deltaTime;
            timeLeft = std::max(0.0f, timeLeft - deltaTime);

            ApplyInputToPlayer(playerEntity, localInput);

            if(networkMode == NetworkMode::HOST) {
                for(auto& inputPair : hostInputByPlayerId) {
                    int id = inputPair.first;
                    PlayerInput& inputState = inputPair.second;
                    if(id == localPlayerId) continue;

                    auto foundPlayer = playerEntities.find(id);
                    if(foundPlayer == playerEntities.end()) continue;

                    ApplyInputToPlayer(foundPlayer->second, inputState);
                    inputState.brakePressed = false;
                }
            }
        }
    } else {
        ctx.deltaTime = 0.0f;
    }

    // bgMusic.Update();

    if(!ctx.HasComponent<TransformComponent>(playerEntity)) return;
    const auto &trans = ctx.GetComponent<TransformComponent>(playerEntity);

    camera.target = trans.position;

    if(ctx.HasComponent<Physics2DComponent>(playerEntity)){
        const auto &phys = ctx.GetComponent<Physics2DComponent>(playerEntity);
        camera.position = trans.position + raylib::Vector3{-cos(phys.heading.RadianValue()), -0.5, sin(phys.heading.RadianValue())} * 500 + raylib::Vector3{0, 400, 0};

    }else if (ctx.HasComponent<Physics3DComponent>(playerEntity)){
        const auto &phys = ctx.GetComponent<Physics3DComponent>(playerEntity);
        Matrix rot = QuaternionToMatrix(phys.rotation);
        camera.position = trans.position + (raylib::Vector3(Vector3Normalize(Vector3Transform({1, 0, 0}, rot))) * -1) * 500 + raylib::Vector3{0, 400, 0};
    }
}

void LogicHandler::Render(){
    camera.BeginMode(); {
        skybox.Draw();
        ground.Draw({});

        schedule(ctx);
        if(networkMode != NetworkMode::CLIENT) {
            HandleButtonCollisions();
        }
    } camera.EndMode();

    if(networkMode == NetworkMode::HOST && gamestate == GameStatus::PLAYING) {
        BroadcastHostState();
    }

    DrawUI();
}
