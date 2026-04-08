#pragma once
#include "constants.h"
#include "bullet.h"
#include "item.h"

/**
 * @class Tank
 * @brief Đại diện cho một xe tăng do người chơi điều khiển. Quản lý di chuyển, logic vũ khí và va chạm.
 */
class Tank {
public:
    b2Body* body;           ///< Con trỏ tới hệ thống vật lý Box2D của thân xe
    int playerIndex;        ///< Số thứ tự của người chơi (0 = Player 1, 1 = Player 2...)
    
    // Biến lưu trữ mã phím điều khiển
    int forwardKey, backwardKey, turnLeftKey, turnRightKey, shootKey, shieldKey;
    float shootCooldownTimer; ///< Bộ đếm lùi thời gian giữa các lần bắn đạn
    bool isDestroyed;       ///< Cờ đánh dấu xe tăng đã bị trúng đạn và cần loại bỏ

    ItemType currentWeapon; ///< Vũ khí đặc biệt đang trang bị
    int ammo;               ///< Lượng đạn còn lại của vũ khí đặc biệt

    bool hasShield;         ///< Cờ theo dõi trạng thái khiên
    float shieldTimer;      ///< Thời gian tồn tại của khiên
    float shieldCooldownTimer; ///< Thời gian chờ để bật lại khiên

    /**
     * @brief Khởi tạo xe tăng cùng thông số người chơi và gán phím
     */
    Tank(b2World& world, int _playerIndex, int _fw, int _bw, int _tl, int _tr, int _sh, int _shield);
    
    /**
     * @brief Vòng lặp cập nhật chính của xe tăng (logic thời gian, di chuyển, bắn đạn, va chạm)
     */
    void Update(b2World& world, std::vector<Bullet*>& bullets, std::vector<Item*>& items, float dt);
    
    /**
     * @brief Dựng hình xe tăng dựa vào tọa độ và góc xoay từ Box2D
     */
    void Draw();

private:
    /**
     * @brief Xử lý logic phím điều hướng hệ thống vật lý
     */
    void HandleMovement();

    /**
     * @brief Xử lý phím bắn, raycast tránh kẹt tường và tạo các viên đạn dựa theo vũ khí
     */
    void FireWeapon(b2World& world, std::vector<Bullet*>& bullets);

    /**
     * @brief Quét danh sách cọ xát Box2D để xử lý dính đạn hoặc ăn vật phẩm
     */
    void CheckCollisions(std::vector<Bullet*>& bullets, std::vector<Item*>& items);
};