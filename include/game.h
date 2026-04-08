#pragma once
#include "constants.h"
#include "tank.h"
#include "bullet.h"
#include "map.h"
#include "portal.h"
#include "ui.h"
#include "item.h"

/**
 * @struct PlayerConfig
 * @brief Cấu trúc chuyên dụng để lưu mã phím cấu hình từ UI cho từng người chơi
 */
struct PlayerConfig { int fw, bw, tl, tr, sh, shieldKey; };

/**
 * @class Game
 * @brief Động cơ nòng cốt của trò chơi, quản lý State và Vòng lặp chính Game Loop.
 * Sở hữu mọi hệ thống (Vật lý, Map, Player, Portal) và điều hướng các sự kiện logic.
 */
class Game {
private:
    b2World world;                  ///< Môi trường mô phỏng vật lý Box2D cốt lõi (Trọng lực 0)
    std::vector<Tank*> tanks;       ///< Danh sách các xe tăng (Player) hiện đang sống
    std::vector<Bullet*> bullets;   ///< Danh sách các viên đạn, tia laser, mảnh vỡ đang bay
    std::vector<Item*> items;       ///< Danh sách các hộp vũ khí văng trên đường
    GameMap map;                    ///< Hệ thống quản lý Mê cung cấp quyền thuật toán
    Portal portal;                  ///< Trình điều khiển cơ chế dịch chuyển không gian
    
    float itemSpawnTimer;           ///< Đồng hồ đếm ngược sinh hộp vũ khí kế tiếp

    int playerScores[4];            ///< Bảng điểm lưu trữ thành tích tích lũy 4 Slot
    int numPlayers;                 ///< Thống kê số lượng người tham gia hiện tại
    bool needsRestart;              ///< Cờ báo hiệu cần tiến hành dọn bàn & Setup vòng mới
    std::vector<PlayerConfig> configs; ///< Lưu trữ thiết lập các phím từ Menu cài đặt

    /**
     * @brief Khởi tạo bàn đấu mới:
     * Dọn sạch rác màn cũ (đạn, item, xác xe), Generate Map mới, Xếp vị trí xe tăng
     */
    void ResetMatch();
    
    /**
     * @brief Vòng lặp cập nhật Logic mỗi Khung Hình (Frame):
     * Cập nhật đếm ngược sinh vật phẩm, di chuyển đạn, xe tăng và quét tên lửa theo dõi
     */
    void Update(float dt);
    
    /**
     * @brief Gọi toàn bộ hàm Draw của đồ họa hiển thị lên Canvas
     */
    void Draw();
    
    /**
     * @brief Dọn dẹp Garbage Collection và xử lý hiệu ứng Nổ/Miểng
     */
    void CleanUpBullets(float dt);
    
    /**
     * @brief Dọn dẹp vệ sinh các cục Item bị cán thủng
     */
    void CleanUpItems();

public:
    // Khởi tạo các thông số game và cài đặt phím mặc định ban đầu
    Game();
    ~Game();
    // Khởi động giao diện màn hình và Vòng lặp trò chơi (Game Loop)
    void Run();
};