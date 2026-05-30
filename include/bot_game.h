#pragma once
#include "constants.h"
#include "tank.h"
#include "bullet.h"
#include "map.h"
#include "portal.h"
#include "ui.h"
#include "bot_tank.h"

// Cấu hình phím cho người chơi (tách riêng khỏi game.h để tránh xung đột)
struct BotGamePlayerConfig { int fw, bw, tl, tr, sh; };

// Lớp quản lý game có tích hợp bot AI — chạy song song với người chơi thật
class BotGame {
private:
    b2World world;                          // Thế giới vật lý Box2D
    std::vector<Tank*> tanks;               // Xe tăng của người chơi thật
    BotTank* bot;                           // Xe tăng AI
    std::vector<Bullet*> bullets;           // Tất cả đạn đang bay
    GameMap map;                            // Bản đồ mê cung
    Portal portal;                          // Cổng dịch chuyển

    int playerScores[5];                    // Điểm: tối đa 4 người + 1 bot
    int numHumanPlayers;                    // Số người chơi thật
    bool needsRestart;                      // Cờ yêu cầu bắt đầu ván mới
    std::vector<BotGamePlayerConfig> configs; // Cấu hình phím người chơi

    void ResetMatch();                      // Khởi tạo ván đấu mới
    void Update(float dt);                  // Cập nhật logic game
    void Draw();                            // Vẽ toàn bộ lên màn hình
    void CleanUpBullets(float dt);          // Dọn đạn hết hạn

public:
    BotGame();
    ~BotGame();
    void Run();                             // Vòng lặp game chính
};
