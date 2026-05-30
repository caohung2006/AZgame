#include "test_game.h"

// ============================================================================
// CONSTRUCTOR & DESTRUCTOR
// ============================================================================
TestGame::TestGame()
    : world(b2Vec2(0.0f, 0.0f)),
      dodger(nullptr), shooter(nullptr),
      survivalTime(0.0f), bestTime(0.0f),
      roundNumber(0), needsRestart(true) {}

TestGame::~TestGame() {}

// ============================================================================
// RUN — Vòng lặp game chính (không có Settings, chạy liên tục)
// ============================================================================
void TestGame::Run() {
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "AZ Game - DODGE TEST");
    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        if (needsRestart) ResetMatch();

        float dt = GetFrameTime();
        Update(dt);
        Draw();
    }
    CloseWindow();
}

// ============================================================================
// RESET MATCH — Spawn lại cả hai bot ở vị trí ngẫu nhiên
// ============================================================================
void TestGame::ResetMatch() {
    map.Clear(world);

    // Dọn đạn
    for (Bullet* b : bullets) { world.DestroyBody(b->body); delete b; }
    bullets.clear();

    // Dọn dodger
    if (dodger) { world.DestroyBody(dodger->body); delete dodger; dodger = nullptr; }

    // Dọn shooter
    if (shooter) { world.DestroyBody(shooter->body); delete shooter; shooter = nullptr; }

    // Xây bản đồ
    map.Build(world);

    // === Chọn 2 ô kề nhau ngẫu nhiên ===
    float cellW = 90.0f, cellH = 90.0f;
    float offsetX = (SCREEN_WIDTH - (8 * cellW)) / 2.0f;
    float offsetY = (SCREEN_HEIGHT - (6 * cellH)) / 2.0f - 50.0f;

    int c1 = GetRandomValue(0, 7), r1 = GetRandomValue(0, 5);
    int c2 = c1, r2 = r1;

    // Chọn ngẫu nhiên hướng kề ngang hoặc dọc
    bool horizontal = GetRandomValue(0, 1) == 0;
    if (horizontal) {
        if (c1 == 0) c2 = c1 + 1;
        else if (c1 == 7) c2 = c1 - 1;
        else c2 = (GetRandomValue(0, 1) == 0) ? c1 - 1 : c1 + 1;
    } else {
        if (r1 == 0) r2 = r1 + 1;
        else if (r1 == 5) r2 = r1 - 1;
        else r2 = (GetRandomValue(0, 1) == 0) ? r1 - 1 : r1 + 1;
    }

    // Spawn dodger (RED — bot né đạn)
    dodger = new BotTank(world, 0);
    float dx = (offsetX + c1 * cellW + cellW / 2.0f) / SCALE;
    float dy = (SCREEN_HEIGHT - (offsetY + r1 * cellH + cellH / 2.0f)) / SCALE;

    // Spawn shooter (PURPLE — bot bắn bất tử)
    shooter = new ShooterBot(world, 1);
    float sx = (offsetX + c2 * cellW + cellW / 2.0f) / SCALE;
    float sy = (SCREEN_HEIGHT - (offsetY + r2 * cellH + cellH / 2.0f)) / SCALE;

    // Tính góc xoay để hai xe hướng mặt vào nhau ngay lập tức
    float angleToShooter = atan2f(-(sx - dx), sy - dy);
    float angleToDodger = atan2f(-(dx - sx), dy - sy);

    dodger->body->SetTransform(b2Vec2(dx, dy), angleToShooter);
    shooter->body->SetTransform(b2Vec2(sx, sy), angleToDodger);

    survivalTime = 0.0f;
    roundNumber++;
    needsRestart = false;
}

// ============================================================================
// UPDATE — Cập nhật logic mỗi khung hình
// ============================================================================
void TestGame::Update(float dt) {
    // Cập nhật shooter (ngắm vào dodger)
    if (shooter && dodger) {
        shooter->Update(world, bullets, dodger->body);
    }

    // Cập nhật dodger (né đạn, không có mục tiêu Tank* để bắn lại)
    std::vector<Tank*> noEnemies;  // Vector rỗng — dodger chỉ tập trung né
    if (dodger && !dodger->isDestroyed) {
        dodger->Update(world, bullets, noEnemies);

        if (dodger->isDestroyed) {
            // Dodger bị bắn hạ!
            if (survivalTime > bestTime) bestTime = survivalTime;
            world.DestroyBody(dodger->body);
            delete dodger;
            dodger = nullptr;
            needsRestart = true;
        } else {
            survivalTime += dt;
        }
    }

    // Bước vật lý
    world.Step(dt, 6, 2);
    CleanUpBullets(dt);
}

// ============================================================================
// DRAW — Vẽ game + bảng thống kê thời gian sống sót
// ============================================================================
void TestGame::Draw() {
    BeginDrawing();
    ClearBackground(RAYWHITE);

    // Vẽ bản đồ + đối tượng
    map.Draw();
    if (shooter) shooter->Draw();
    if (dodger) dodger->Draw();
    for (Bullet* b : bullets) b->Draw();

    // ── HUD — Bảng thống kê kiểm tra né đạn ──
    // Tiêu đề
    DrawText("DODGE TEST MODE", 10, 10, 24, DARKPURPLE);

    // Thời gian sống sót hiện tại
    const char* timeTxt = TextFormat("Survival: %.1fs", survivalTime);
    DrawText(timeTxt, 10, 42, 20, RED);

    // Kỷ lục
    const char* bestTxt = TextFormat("Best: %.1fs", bestTime);
    DrawText(bestTxt, 10, 68, 20, DARKGREEN);

    // Số ván
    const char* roundTxt = TextFormat("Round: %d", roundNumber);
    DrawText(roundTxt, 10, 94, 20, DARKGRAY);

    // Chú thích màu sắc
    DrawRectangle(SCREEN_WIDTH - 200, 10, 14, 14, RED);
    DrawText("Dodger (BotTank)", SCREEN_WIDTH - 180, 10, 14, DARKGRAY);
    DrawRectangle(SCREEN_WIDTH - 200, 30, 14, 14, PURPLE);
    DrawText("Shooter (Immortal)", SCREEN_WIDTH - 180, 30, 14, DARKGRAY);

    EndDrawing();
}

// ============================================================================
// CLEANUP BULLETS — Loại bỏ đạn hết hạn
// ============================================================================
void TestGame::CleanUpBullets(float dt) {
    for (auto it = bullets.begin(); it != bullets.end(); ) {
        Bullet* b = *it;
        b->time -= dt;
        if (b->time <= 0.0f) {
            world.DestroyBody(b->body);
            delete b;
            it = bullets.erase(it);
        } else {
            ++it;
        }
    }
}
