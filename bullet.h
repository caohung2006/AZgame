#pragma once
#include "constants.h"

// Lớp đại diện cho viên đạn trong trò chơi
class Bullet {
public:
    b2Body* body;   // Con trỏ quản lý vật thể vật lý Box2D của viên đạn
    float time;     // Thời gian tồn tại còn lại của đạn (giây)

    // Khởi tạo viên đạn tại một vị trí cụ thể với lực/vận tốc ban đầu
    Bullet(b2World& world, b2Vec2 position, b2Vec2 velocity);
    
    // Dựng hình viên đạn lên màn hình
    void Draw();
};