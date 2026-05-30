#pragma once
#include "constants.h"
#include "bullet.h"
#include "map.h"
#include "bot_tank.h"
#include "shooter_bot.h"

// Chế độ thử nghiệm: đo khả năng né đạn của BotTank
// ShooterBot (bất tử) liên tục bắn — BotTank cố gắng sống sót
class TestGame {
private:
    b2World world;
    BotTank* dodger;                // Bot né đạn (đối tượng kiểm tra)
    ShooterBot* shooter;            // Bot bắn (bất tử, đối tượng tấn công)
    std::vector<Bullet*> bullets;
    GameMap map;

    float survivalTime;             // Thời gian sống sót ván hiện tại (giây)
    float bestTime;                 // Kỷ lục sống sót
    int roundNumber;                // Số ván đã chơi
    bool needsRestart;

    void ResetMatch();
    void Update(float dt);
    void Draw();
    void CleanUpBullets(float dt);

public:
    TestGame();
    ~TestGame();
    void Run();
};
