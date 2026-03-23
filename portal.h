#pragma once
#include "constants.h"
#include "tank.h"
#include "bullet.h"

// Lớp quản lý cơ chế Cổng dịch chuyển
class Portal {
public:
    b2Vec2 posA;            // Vị trí của cổng A (tọa độ Box2D)
    b2Vec2 posB;            // Vị trí của cổng B (tọa độ Box2D)
    bool isActive;          // Cờ xác định xem cổng có đang hiện trên bản đồ không
    float cooldownTimer;    // Thời gian chờ để sinh ra cặp cổng mới sau khi bị sử dụng

    Portal();
    
    // Sinh cổng ngẫu nhiên và xét va chạm dịch chuyển đối với Xe tăng hoặc Đạn
    void Update(float dt, std::vector<Tank*>& tanks, std::vector<Bullet*>& bullets);
    
    // Vẽ hiệu ứng đồ hoạ Cổng A và Cổng B lên bản đồ
    void Draw();
    
    // Đặt lại cổng về trạng thái tắt (dùng khi reset ván đấu)
    void Reset();
};