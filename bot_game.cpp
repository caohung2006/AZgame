#include "bot_game.h"

// ============================================================================
// CONSTRUCTOR & DESTRUCTOR
// ============================================================================
BotGame::BotGame() : world(b2Vec2(0.0f, 0.0f)), bot(nullptr), numHumanPlayers(1), needsRestart(true) {
    for (int i = 0; i < 5; i++) playerScores[i] = 0;
    // Phím mặc định giống game gốc
    configs.resize(4);
    configs[0] = {KEY_W, KEY_S, KEY_A, KEY_D, KEY_Q};
    configs[1] = {KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT, KEY_SLASH};
    configs[2] = {KEY_I, KEY_K, KEY_J, KEY_L, KEY_U};
    configs[3] = {KEY_KP_8, KEY_KP_5, KEY_KP_4, KEY_KP_6, KEY_KP_7};
}

BotGame::~BotGame() {}

// ============================================================================
// RUN — Vòng lặp game chính
// ============================================================================
void BotGame::Run() {
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "AZ Game - BOT Mode");
    SetTargetFPS(60);

    while (!WindowShouldClose()) {
        // Mở Settings nếu click nút
        if (UI::CheckSettingsButtonClicked()) {
            int count = UI::ShowPlayerCountScreen();
            if (count > 3) count = 3;   // Giữ 1 chỗ cho bot
            if (count < 1) count = 1;
            numHumanPlayers = count;
            for (int i = 0; i < numHumanPlayers; i++) {
                UI::ShowKeyBindingScreen(configs[i].fw, configs[i].bw,
                                         configs[i].tl, configs[i].tr,
                                         configs[i].sh, i + 1);
            }
            for (int i = 0; i < 5; i++) playerScores[i] = 0;
            needsRestart = true;
        }

        if (needsRestart) ResetMatch();

        float dt = GetFrameTime();
        Update(dt);
        Draw();
    }
    CloseWindow();
}

// ============================================================================
// RESET MATCH — Dọn sạch và tạo ván đấu mới
// ============================================================================
void BotGame::ResetMatch() {
    // Dọn bản đồ cũ
    map.Clear(world);

    // Dọn xe tăng người chơi
    for (Tank* t : tanks) { world.DestroyBody(t->body); delete t; }
    tanks.clear();

    // Dọn đạn
    for (Bullet* b : bullets) { world.DestroyBody(b->body); delete b; }
    bullets.clear();

    // Dọn bot
    if (bot) { world.DestroyBody(bot->body); delete bot; bot = nullptr; }

    // Xây bản đồ mới
    map.Build(world);

    int totalPlayers = numHumanPlayers + 1;

    // === Sinh vị trí ngẫu nhiên trong lưới mê cung 8×6 ===
    float cellW = 90.0f, cellH = 90.0f;
    float offsetX = (SCREEN_WIDTH - (8 * cellW)) / 2.0f;
    float offsetY = (SCREEN_HEIGHT - (6 * cellH)) / 2.0f - 50.0f;

    // Chọn các ô ngẫu nhiên không trùng lặp cho mỗi người chơi
    std::vector<std::pair<int,int>> spawnCells;
    while ((int)spawnCells.size() < totalPlayers) {
        int col = GetRandomValue(0, 7);
        int row = GetRandomValue(0, 5);
        bool duplicate = false;
        for (size_t j = 0; j < spawnCells.size(); j++) {
            if (spawnCells[j].first == col && spawnCells[j].second == row) {
                duplicate = true;
                break;
            }
        }
        if (!duplicate) spawnCells.push_back(std::make_pair(col, row));
    }

    // Spawn xe tăng người chơi tại vị trí ngẫu nhiên
    for (int i = 0; i < numHumanPlayers; i++) {
        Tank* t = new Tank(world, i, configs[i].fw, configs[i].bw,
                           configs[i].tl, configs[i].tr, configs[i].sh);
        float px = offsetX + spawnCells[i].first * cellW + cellW / 2.0f;
        float py = offsetY + spawnCells[i].second * cellH + cellH / 2.0f;
        float bx = px / SCALE;
        float by = (SCREEN_HEIGHT - py) / SCALE;
        float angle = (float)GetRandomValue(0, 628) / 100.0f - b2_pi;
        t->body->SetTransform(b2Vec2(bx, by), angle);
        tanks.push_back(t);
    }

    // Spawn bot tại vị trí ngẫu nhiên (slot cuối)
    int botCell = numHumanPlayers;
    bot = new BotTank(world, numHumanPlayers);
    float bpx = offsetX + spawnCells[botCell].first * cellW + cellW / 2.0f;
    float bpy = offsetY + spawnCells[botCell].second * cellH + cellH / 2.0f;
    float bbx = bpx / SCALE;
    float bby = (SCREEN_HEIGHT - bpy) / SCALE;
    float botAngle = (float)GetRandomValue(0, 628) / 100.0f - b2_pi;
    bot->body->SetTransform(b2Vec2(bbx, bby), botAngle);

    portal.Reset();
    needsRestart = false;
}

// ============================================================================
// UPDATE — Cập nhật logic mỗi khung hình
// ============================================================================
void BotGame::Update(float dt) {
    int totalAlive = 0;
    int lastAliveIndex = -1;

    // --- Cập nhật xe tăng người chơi ---
    for (auto it = tanks.begin(); it != tanks.end(); ) {
        Tank* t = *it;
        t->Move(world, bullets);
        if (t->isDestroyed) {
            world.DestroyBody(t->body);
            delete t;
            it = tanks.erase(it);
        } else {
            totalAlive++;
            lastAliveIndex = t->playerIndex;
            ++it;
        }
    }

    // --- Cập nhật bot AI ---
    if (bot && !bot->isDestroyed) {
        bot->Update(world, bullets, tanks);
        if (bot->isDestroyed) {
            world.DestroyBody(bot->body);
            delete bot;
            bot = nullptr;
        } else {
            totalAlive++;
            lastAliveIndex = bot->playerIndex;
        }
    }

    // --- Kiểm tra điều kiện thắng ---
    int totalPlayers = numHumanPlayers + 1;
    if ((totalPlayers > 1 && totalAlive <= 1) || (totalPlayers == 1 && totalAlive == 0)) {
        if (totalPlayers > 1 && totalAlive == 1 && lastAliveIndex >= 0) {
            playerScores[lastAliveIndex]++;
        }
        needsRestart = true;
    }

    // --- Cập nhật cổng dịch chuyển ---
    if (!needsRestart) {
        portal.Update(dt, tanks, bullets);

        // Kiểm tra cổng dịch chuyển cho bot (portal.Update chỉ xử lý Tank*)
        if (bot && portal.isActive) {
            b2Vec2 bp = bot->body->GetPosition();
            if ((bp - portal.posA).Length() < 25.0f / SCALE) {
                bot->body->SetTransform(portal.posB, bot->body->GetAngle());
                portal.isActive = false;
                portal.cooldownTimer = (float)GetRandomValue(3, 10);
            } else if ((bp - portal.posB).Length() < 25.0f / SCALE) {
                bot->body->SetTransform(portal.posA, bot->body->GetAngle());
                portal.isActive = false;
                portal.cooldownTimer = (float)GetRandomValue(3, 10);
            }
        }
    }

    // Bước vật lý Box2D
    world.Step(dt, 6, 2);
    CleanUpBullets(dt);
}

// ============================================================================
// DRAW — Vẽ toàn bộ lên màn hình (bản đồ, cổng, xe tăng, đạn, HUD)
// ============================================================================
void BotGame::Draw() {
    BeginDrawing();
    ClearBackground(RAYWHITE);

    map.Draw();
    portal.Draw();
    for (Tank* t : tanks) t->Draw();
    if (bot) bot->Draw();
    for (Bullet* b : bullets) b->Draw();

    // ── HUD tuỳ chỉnh (hiển thị "BOT" thay vì "Player N") ──
    int totalPlayers = numHumanPlayers + 1;

    // Nút SETTINGS (giống game gốc)
    DrawRectangle(SCREEN_WIDTH - 120, 10, 110, 30, LIGHTGRAY);
    DrawRectangleLines(SCREEN_WIDTH - 120, 10, 110, 30, DARKGRAY);
    DrawText("SETTINGS", SCREEN_WIDTH - 105, 16, 20, DARKGRAY);

    // Bảng điểm
    int secW = SCREEN_WIDTH / totalPlayers;
    for (int i = 0; i < numHumanPlayers; i++) {
        const char* txt = TextFormat("Player %d: %d", i + 1, playerScores[i]);
        DrawText(txt, (i * secW) + (secW / 2) - MeasureText(txt, 24) / 2,
                 SCREEN_HEIGHT - 40, 24, DARKBLUE);
    }
    // Điểm bot — hiển thị bằng màu đỏ
    int botIdx = numHumanPlayers;
    const char* botTxt = TextFormat("BOT: %d", playerScores[botIdx]);
    DrawText(botTxt, (botIdx * secW) + (secW / 2) - MeasureText(botTxt, 24) / 2,
             SCREEN_HEIGHT - 40, 24, RED);

    EndDrawing();
}

// ============================================================================
// CLEANUP BULLETS — Loại bỏ đạn hết thời gian tồn tại
// ============================================================================
void BotGame::CleanUpBullets(float dt) {
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
