#pragma once
#include "constants.h"
#include "tank.h"
#include "bullet.h"
#include "gamemap.h"
#include "portal.h"
#include "ui.h"

struct PlayerConfig { int fw, bw, tl, tr, sh; };

class Game {
private:
    b2World world;
    std::vector<Tank*> tanks;
    std::vector<Bullet*> bullets;
    GameMap map;
    Portal portal;

    int playerScores[4];
    int numPlayers;
    bool needsRestart;
    std::vector<PlayerConfig> configs;

    void ResetMatch();
    void Update(float dt);
    void Draw();
    void CleanUpBullets(float dt);

public:
    Game();
    ~Game();
    void Run();
};