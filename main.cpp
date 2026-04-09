#include <raylib.h>
#include <ctime>
#include <cstdlib>
#include "game.h"
#include "renderer.h"
#include "ui.h"

/**
 * @brief Entry point cho chế độ human play (có đồ họa).
 * 
 * Kiến trúc:
 * - Game: quản lý logic thuần (Box2D), KHÔNG phụ thuộc Raylib
 * - Renderer: vẽ thế giới game
 * - UI: giao diện cài đặt + HUD
 * - main.cpp: kết nối input (bàn phím → TankActions) và rendering
 * 
 * Để train RL, viết main khác: chỉ dùng Game + TankActions, không cần Renderer/UI.
 */
int main() {
    srand((unsigned int)time(NULL));

    Game game;
    // Cài đặt phím mặc định cho 4 người chơi (Raylib key codes)
    game.configs[0] = {KEY_W, KEY_S, KEY_A, KEY_D, KEY_Q, KEY_E};
    game.configs[1] = {KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT, KEY_SLASH, KEY_PERIOD};
    game.configs[2] = {KEY_I, KEY_K, KEY_J, KEY_L, KEY_U, KEY_O};
    game.configs[3] = {KEY_KP_8, KEY_KP_5, KEY_KP_4, KEY_KP_6, KEY_KP_7, KEY_KP_9};

    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "AZ Game");
    SetTargetFPS(60);
    UI::Init();

    while (!WindowShouldClose()) {
        // --- Xử lý Settings UI ---
        if (UI::CheckSettingsButtonClicked()) {
            int oldNumPlayers = game.numPlayers;
            UI::ShowSettingsScreen(game.numPlayers, game.portalsEnabled, game.itemsEnabled, game.configs);
            if (game.numPlayers != oldNumPlayers) {
                for (int i = 0; i < 4; i++) game.playerScores[i] = 0;
            }
            game.needsRestart = true;
        }

        if (game.needsRestart) game.ResetMatch();

        // --- Chuyển đổi bàn phím → TankActions ---
        std::vector<TankActions> actions(game.numPlayers);
        for (int i = 0; i < game.numPlayers; i++) {
            PlayerConfig& cfg = game.configs[i];
            actions[i].forward = IsKeyDown(cfg.fw);
            actions[i].backward = IsKeyDown(cfg.bw);
            actions[i].turnLeft = IsKeyDown(cfg.tl);
            actions[i].turnRight = IsKeyDown(cfg.tr);
            actions[i].shoot = IsKeyPressed(cfg.sh);
            actions[i].shield = IsKeyPressed(cfg.shieldKey);
        }

        // --- Cập nhật logic game ---
        float dt = GetFrameTime();
        game.Update(actions, dt);

        // --- Render ---
        BeginDrawing();
        ClearBackground(RAYWHITE);
        Renderer::DrawWorld(game);
        UI::DrawHUD(game.playerScores, game.numPlayers);
        EndDrawing();
    }

    UI::Cleanup();
    CloseWindow();
    return 0;
}