#pragma once
#include "constants.h"
#include "bullet.h"
#include "item.h"

// Lớp đại diện cho xe tăng được điều khiển bởi người chơi
class Tank {
public:
    b2Body* body;           // Con trỏ tới hệ thống vật lý Box2D của thân xe
    int playerIndex;        // Số thứ tự của người chơi (0 = Player 1, 1 = Player 2...)
    
    // Biến lưu trữ mã phím điều khiển do người chơi tự cấu hình
    int forwardKey, backwardKey, turnLeftKey, turnRightKey, shootKey;
    float shootCooldownTimer; // Bộ đếm lùi thời gian giữa các lần bắn đạn
    bool isDestroyed;       // Cờ đánh dấu xe tăng đã bị trúng đạn và cần loại bỏ

    ItemType currentWeapon; // Vũ khí đặc biệt đang trang bị
    int ammo;               // Lượng đạn còn lại của vũ khí đặc biệt

    // Hàm khởi tạo xe tăng cùng thông số người chơi và gán phím
    Tank(b2World& world, int _playerIndex, int _fw, int _bw, int _tl, int _tr, int _sh);
    
    // Xử lý logic mỗi khung hình: nhận phím tiến/lùi/xoay, bắn đạn và xét va chạm
    void Move(b2World& world, std::vector<Bullet*>& bullets, std::vector<Item*>& items);
    // Dựng hình xe tăng dựa vào tọa độ và góc xoay từ Box2D
    void Draw();
};