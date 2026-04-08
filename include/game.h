#pragma once
#include "constants.h"
#include "tank.h"
#include "bullet.h"
#include "map.h"
#include "portal.h"
#include "ui.h"
#include "item.h"

// Cấu trúc lưu tạm các mã phím được gán của từng người chơi
struct PlayerConfig { int fw, bw, tl, tr, sh; };

// Lớp quản lý trung tâm (Core / Engine), chịu trách nhiệm điều phối vòng lặp Game
class Game {
private:
    b2World world;                  // Đối tượng quản lý thế giới vật lý 2D cốt lõi
    std::vector<Tank*> tanks;       // Danh sách các xe tăng đang tồn tại
    std::vector<Bullet*> bullets;   // Danh sách các viên đạn đang bay
    std::vector<Item*> items;       // Danh sách các hộp vật phẩm trên bản đồ
    GameMap map;                    // Đối tượng xử lý bản đồ
    Portal portal;                  // Đối tượng xử lý cơ chế cổng
    
    float itemSpawnTimer;           // Thời gian đếm ngược để sinh vật phẩm mới

    int playerScores[4];            // Mảng lưu điểm tổng kết của tối đa 4 người chơi
    int numPlayers;                 // Số lượng người chơi tham gia ván hiện tại
    bool needsRestart;              // Cờ tín hiệu yêu cầu làm mới ván đấu (khi có người chết hoặc reset)
    std::vector<PlayerConfig> configs; // Dữ liệu phím bấm thiết lập qua màn hình Settings

    // Khởi tạo một round đấu mới (xóa dữ liệu cũ, sinh lại map, spawn xe tăng)
    void ResetMatch();
    
    // Bước xử lý logic một khung hình: cập nhật xe tăng, đạn, cổng, và gọi world.Step()
    void Update(float dt);
    
    // Bước dựng hình: chỉ huy các đối tượng tự vẽ chính mình lên Raylib Canvas
    void Draw();
    
    // Quét qua mảng và loại bỏ những viên đạn bị va chạm hoặc hết thời gian sống
    void CleanUpBullets(float dt);
    
    // Quét qua mảng và dọn dẹp vật phẩm đã bị nhặt
    void CleanUpItems();

public:
    // Khởi tạo các thông số game và cài đặt phím mặc định ban đầu
    Game();
    ~Game();
    // Khởi động giao diện màn hình và Vòng lặp trò chơi (Game Loop)
    void Run();
};