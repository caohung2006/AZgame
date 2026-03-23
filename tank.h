#pragma once
#include "constants.h"
#include "bullet.h"

class Tank {
public:
    b2Body* body;
    int playerIndex;
    int forwardKey, backwardKey, turnLeftKey, turnRightKey, shootKey;
    float shootCooldownTimer;
    bool isDestroyed;

    Tank(b2World& world, int _playerIndex, int _fw, int _bw, int _tl, int _tr, int _sh);
    
    void Move(b2World& world, std::vector<Bullet*>& bullets);
    void Draw();
};