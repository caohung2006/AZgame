#pragma once
#include <box2d/box2d.h>
#include <vector>
#include <string>
#include <cmath>
#include <cstdlib>

using namespace std;

// ========================================================================
// Hằng số toán học (không phụ thuộc Raylib)
// ========================================================================
#ifndef PI
#define PI 3.14159265358979323846f
#endif
#ifndef DEG2RAD
#define DEG2RAD (PI / 180.0f)
#endif
#ifndef RAD2DEG
#define RAD2DEG (180.0f / PI)
#endif

// ========================================================================
// Thông số game
// ========================================================================
const float SCALE = 30.0f;         // Tỷ lệ pixel (Raylib) <-> mét (Box2D)
const int SCREEN_WIDTH = 1024;     // Chiều rộng thế giới game
const int SCREEN_HEIGHT = 768;     // Chiều cao thế giới game

/**
 * @struct PlayerConfig
 * @brief Lưu mã phím cấu hình cho từng người chơi (chỉ dùng ở chế độ human play)
 */
struct PlayerConfig { int fw, bw, tl, tr, sh, shieldKey; };

/**
 * @struct TankActions
 * @brief Input trừu tượng cho xe tăng. Human play: từ bàn phím. RL: từ agent.
 * Tách rời input khỏi game logic, cho phép chạy headless khi train RL.
 */
struct TankActions {
    bool forward = false;
    bool backward = false;
    bool turnLeft = false;
    bool turnRight = false;
    bool shoot = false;
    bool shield = false;
};

/**
 * @struct DeathEvent
 * @brief Ghi lại vị trí xe tăng khi bị tiêu diệt, dùng cho hiệu ứng nổ.
 */
struct DeathEvent {
    b2Vec2 position;
    int playerIndex;
};