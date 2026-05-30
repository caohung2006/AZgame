#pragma once
#include "constants.h"
#include "bullet.h"

// Bot bắn thử nghiệm — bất tử, chỉ có nhiệm vụ ngắm bắn mục tiêu
class ShooterBot {
public:
    b2Body* body;
    int playerIndex;
    float shootCooldownTimer;

    ShooterBot(b2World& world, int _playerIndex);

    // Cập nhật: ngắm mục tiêu, bắn đạn, tuần tra vị trí, hấp thụ đạn (không chết)
    void Update(b2World& world, std::vector<Bullet*>& bullets, b2Body* target);

    // Vẽ xe tăng bắn thử nghiệm (màu tím)
    void Draw();

private:
    static constexpr float SHOOT_COOLDOWN  = 0.18f;  // Bắn cực nhanh để kiểm tra né đạn
    static constexpr float TURN_SPEED      = 4.0f;   // Xoay nhanh
    static constexpr float MOVE_SPEED      = 2.0f;   // Di chuyển tuần tra
    static constexpr float BULLET_SPEED    = 5.0f;
    static constexpr float AIM_TOLERANCE   = 0.12f;  // Ngắm chính xác hơn

    float patrolTimer;      // Đếm lùi đến khi chọn vị trí tuần tra mới
    b2Vec2 patrolTarget;    // Vị trí đang di chuyển đến

    float NormalizeAngle(float angle);
    void PickNewPatrolTarget();
};
