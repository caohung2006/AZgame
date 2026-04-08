#pragma once
#include "constants.h"

// Lớp đại diện cho viên đạn trong trò chơi
class Bullet {
public:
    b2Body* body;
    float time;

    bool isLaser;   // Đánh dấu đây là đạn laser
    bool isFrag;
    bool isMissile;
    bool explodeFrag; // Đánh dấu đạn frag cần phát nổ thành nhiều mảnh
    int ownerPlayerIndex;

    // Khởi tạo viên đạn tại một vị trí cụ thể với lực/vận tốc ban đầu
    Bullet(b2World& world, b2Vec2 position, b2Vec2 velocity, bool _isLaser = false, bool _isFrag = false, bool _isMissile = false, int _owner = -1);
    
    // Dựng hình viên đạn lên màn hình
    void Draw();
};