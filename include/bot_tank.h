#pragma once
#include "constants.h"
#include "bullet.h"
#include "tank.h"

// Xe tăng được điều khiển bởi AI (thuật toán né đạn + ngắm bắn)
class BotTank {
public:
    b2Body* body;           // Box2D body (cùng hình dạng với Tank)
    int playerIndex;        // Chỉ số người chơi (dùng để tính điểm)
    float shootCooldownTimer;
    bool isDestroyed;

    // Khởi tạo bot tại vị trí trung tâm màn hình
    BotTank(b2World& world, int _playerIndex);

    // Cập nhật AI mỗi khung hình: né đạn, ngắm mục tiêu, di chuyển, bắn
    void Update(b2World& world, std::vector<Bullet*>& bullets, std::vector<Tank*>& enemies);

    // Vẽ xe tăng bot với màu sắc riêng biệt
    void Draw();

private:
    // === Thông số AI ===
    static constexpr float DANGER_RADIUS      = 3.8f;   // Bán kính phát hiện đạn nguy hiểm (mét Box2D) — tăng để phát hiện sớm hơn
    static constexpr float DANGER_TIME_HORIZON = 2.2f;  // Chỉ xét đạn sẽ đến trong khoảng thời gian này (giây)
    static constexpr float MOVE_SPEED         = 2.5f;   // Tốc độ di chuyển bình thường
    static constexpr float DODGE_SPEED        = 4.8f;   // Tốc độ khi né đạn — tăng để thoát nhanh hơn
    static constexpr float DODGE_SIM_DT       = 0.12f;  // Bước thời gian mô phỏng né (giây)
    static constexpr int   DODGE_SIM_STEPS    = 14;     // Số bước mô phỏng (14 × 0.12 = 1.68s look-ahead)
    static constexpr float TURN_SPEED         = 4.5f;   // Vận tốc góc xoay (rad/s)
    static constexpr float SHOOT_COOLDOWN     = 0.5f;   // Thời gian giữa các lần bắn
    static constexpr float AIM_TOLERANCE      = 0.15f;  // Sai số góc cho phép khi bắn (rad, ~8.6°)
    static constexpr float PURSUIT_DIST       = 7.0f;   // Truy đuổi nếu mục tiêu xa hơn ngưỡng này
    static constexpr float RETREAT_DIST       = 2.5f;   // Lùi lại nếu mục tiêu gần hơn ngưỡng này
    static constexpr float BULLET_SPEED       = 5.0f;   // Tốc độ đạn (phải khớp với game gốc)

    // Tính hướng né tổng hợp từ tất cả đạn đang đe doạ (trả về vec 0 nếu an toàn)
    b2Vec2 EvaluateThreats(b2World& world, std::vector<Bullet*>& bullets);

    // Đánh giá chi phí/mức độ nguy hiểm của cặp hành động né đạn ứng cử viên (vận tốc góc, vận tốc dài)
    float EvaluateDodgeCandidate(b2World& world, b2Vec2 botPos, float currentAngle, float omega, float vLinear, const std::vector<Bullet*>& bullets, b2Vec2 enemyPos);

    // Tìm xe tăng địch gần nhất
    Tank* SelectTarget(std::vector<Tank*>& enemies);

    // Raycast kiểm tra tường: trả về true nếu có tường tĩnh trong khoảng cách `dist`
    bool IsWallInDirection(b2World& world, b2Vec2 from, b2Vec2 dir, float dist);

    // Chuẩn hoá góc về [-π, π]
    float NormalizeAngle(float angle);
};
