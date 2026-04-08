#include "game.h"

Game::Game() : world(b2Vec2(0.0f, 0.0f)), numPlayers(2), needsRestart(true) {
    itemSpawnTimer = 5.0f;
    for(int i=0; i<4; i++) playerScores[i] = 0;
    configs.resize(4);
    configs[0] = {KEY_W, KEY_S, KEY_A, KEY_D, KEY_Q};
    configs[1] = {KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT, KEY_SLASH};
    configs[2] = {KEY_I, KEY_K, KEY_J, KEY_L, KEY_U};
    configs[3] = {KEY_KP_8, KEY_KP_5, KEY_KP_4, KEY_KP_6, KEY_KP_7};
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
    for (int i = 0; i < numPlayers; i++) {
        Tank* t = new Tank(world, i, configs[i].fw, configs[i].bw, configs[i].tl, configs[i].tr, configs[i].sh);
        b2Vec2 pos = t->body->GetPosition(); t->body->SetTransform(b2Vec2(pos.x - 2.0f + (i * 2.0f), pos.y), 0.0f);
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
        itemSpawnTimer = 10.0f;
    }

    for (auto it = tanks.begin(); it != tanks.end(); ) {
        Tank* t = *it; t->Move(world, bullets, items);
        if (t->isDestroyed) { world.DestroyBody(t->body); delete t; it = tanks.erase(it); }
        else ++it;
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
                    // Đạn con bay nhanh
                    newBullets.push_back(new Bullet(world, pos, 6.0f * dir, false, false, false, b->ownerPlayerIndex));
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