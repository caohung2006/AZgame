#include "game.h"

Game::Game() : world(b2Vec2(0.0f, 0.0f)), numPlayers(2), needsRestart(true) {
    itemSpawnTimer = 5.0f;
    for(int i=0; i<4; i++) playerScores[i] = 0;
    configs.resize(4);
    configs[0] = {KEY_W, KEY_S, KEY_A, KEY_D, KEY_Q, KEY_E};
    configs[1] = {KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT, KEY_SLASH, KEY_PERIOD};
    configs[2] = {KEY_I, KEY_K, KEY_J, KEY_L, KEY_U, KEY_O};
    configs[3] = {KEY_KP_8, KEY_KP_5, KEY_KP_4, KEY_KP_6, KEY_KP_7, KEY_KP_9};
}
Game::~Game() {}

void Game::Run() {
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "AZ Game"); SetTargetFPS(60);
    while (!WindowShouldClose()) {
        if (UI::CheckSettingsButtonClicked()) {
            numPlayers = UI::ShowPlayerCountScreen();
            for (int i = 0; i < numPlayers; i++) UI::ShowKeyBindingScreen(configs[i].fw, configs[i].bw, configs[i].tl, configs[i].tr, configs[i].sh, i + 1);
            for (int i = 0; i < 4; i++) playerScores[i] = 0;
            needsRestart = true;
        }
        if (needsRestart) ResetMatch();
        float dt = GetFrameTime(); Update(dt); Draw();
    }
    CloseWindow();
}

void Game::ResetMatch() {
    map.Clear(world);
    for (Tank* t : tanks) { world.DestroyBody(t->body); delete t; } tanks.clear();
    for (Bullet* b : bullets) { world.DestroyBody(b->body); delete b; } bullets.clear();
    for (Item* item : items) { world.DestroyBody(item->body); delete item; } items.clear();
    itemSpawnTimer = 5.0f;
    map.Build(world);
    std::vector<b2Vec2> spawnCells;
    while (spawnCells.size() < numPlayers) {
        b2Vec2 p = map.GetRandomCellCenter();
        bool ok = true;
        for (b2Vec2 sp : spawnCells) {
            if ((p - sp).LengthSquared() < 1.0f) { ok = false; break; }
        }
        if (ok) spawnCells.push_back(p);
    }
    
    for (int i = 0; i < numPlayers; i++) {
        Tank* t = new Tank(world, i, configs[i].fw, configs[i].bw, configs[i].tl, configs[i].tr, configs[i].sh, configs[i].shieldKey);
        t->body->SetTransform(spawnCells[i], (rand() % 4) * PI / 2.0f); // Random góc quay 90 độ
        tanks.push_back(t);
    }
    portal.Reset(); needsRestart = false;
}

void Game::Update(float dt) {
    itemSpawnTimer -= dt;
    if (itemSpawnTimer <= 0.0f) {
        b2Vec2 spawnPos = map.GetRandomCellCenter();
        ItemType rType = static_cast<ItemType>(1 + rand() % 4); // GATLING, FRAG, MISSILE, DEATH_RAY
        items.push_back(new Item(world, spawnPos, rType));
        itemSpawnTimer = 5.0f;
    }

    for (auto it = tanks.begin(); it != tanks.end(); ) {
        Tank* t = *it; 
        t->Update(world, bullets, items, dt);
        if (t->isDestroyed) { world.DestroyBody(t->body); delete t; it = tanks.erase(it); }
        else ++it;
    }
    
    // Tên lửa đuổi theo kẻ địch mềm mại
    for (Bullet* b : bullets) {
        if (b->isMissile) {
            float elapsed = 5.0f - b->time;
            b2Vec2 currentVel = b->body->GetLinearVelocity();
            float currentSpeed = currentVel.Length();
            if (currentSpeed > 0.0f) {
                if (elapsed < 2.0f) {
                    // Trong 2.0s đầu, lượn lờ hình sin (sử dụng cos để xoay vận tốc)
                    float waveTurn = cosf(elapsed * 12.0f) * 4.0f * dt;
                    float angle = atan2f(currentVel.y, currentVel.x) + waveTurn;
                    b->body->SetLinearVelocity(b2Vec2(cosf(angle) * currentSpeed, sinf(angle) * currentSpeed));
                } else {
                    // Tìm kiếm xe tăng BẤT KỲ gần nhất (kể cả xe của chính mình)
                    Tank* target = nullptr;
                    float minDist = 9999.0f;
                    for (Tank* t : tanks) {
                        if (!t->isDestroyed) { 
                            float dist = (t->body->GetPosition() - b->body->GetPosition()).Length();
                            if (dist < minDist) { minDist = dist; target = t; }
                        }
                    }
                    if (target) {
                        b2Vec2 toTarget = target->body->GetPosition() - b->body->GetPosition();
                        toTarget.Normalize();
                        b2Vec2 normVel = currentVel;
                        normVel.Normalize();
                        
                        // Tính cross product 2D để tìm hướng cần xoay (-1 hoặc 1)
                        float cross = normVel.x * toTarget.y - normVel.y * toTarget.x;
                        float turnSpeed = 3.5f * dt; // tốc độ bẻ lái
                        
                        float angle = atan2f(normVel.y, normVel.x);
                        if (cross > 0.1f) angle += turnSpeed;
                        else if (cross < -0.1f) angle -= turnSpeed;
                        
                        b->body->SetLinearVelocity(b2Vec2(cosf(angle) * currentSpeed, sinf(angle) * currentSpeed));
                    }
                }
            }
        }
    }

    if ((numPlayers > 1 && tanks.size() <= 1) || (numPlayers == 1 && tanks.size() == 0)) {
        if (numPlayers > 1 && tanks.size() == 1) playerScores[tanks[0]->playerIndex]++;
        needsRestart = true;
    }
    if (!needsRestart) portal.Update(dt, tanks, bullets);
    world.Step(dt, 6, 2); CleanUpBullets(dt); CleanUpItems();
}

void Game::CleanUpBullets(float dt) {
    std::vector<Bullet*> newBullets;
    for (auto it = bullets.begin(); it != bullets.end(); ) {
        Bullet* b = *it; b->time -= dt;
        if (b->time <= 0.0f || b->explodeFrag) { 
            if (b->isFrag) {
                b2Vec2 pos = b->body->GetPosition();
                for(int i = 0; i < 8; i++) {
                    float angle = i * PI / 4.0f;
                    b2Vec2 dir(cosf(angle), sinf(angle));
                    // Đạn chùm (mảnh vỡ) bay cực nhanh
                    Bullet* shrapnel = new Bullet(world, pos, 12.0f * dir, false, false, false, b->ownerPlayerIndex);
                    // Giảm thời gian tồn tại của đạn chùm (như game gốc)
                    shrapnel->time = 1.2f; 
                    newBullets.push_back(shrapnel);
                }
            }
            world.DestroyBody(b->body); delete b; it = bullets.erase(it); 
        }
        else ++it;
    }
    for (Bullet* nb : newBullets) bullets.push_back(nb);
}

void Game::CleanUpItems() {
    for (auto it = items.begin(); it != items.end(); ) {
        Item* i = *it;
        if (i->isDestroyed) { world.DestroyBody(i->body); delete i; it = items.erase(it); }
        else ++it;
    }
}