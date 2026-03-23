#pragma once
#include "constants.h"
#include "tank.h"
#include "bullet.h"

class Portal {
public:
    b2Vec2 posA;
    b2Vec2 posB;
    bool isActive;
    float cooldownTimer;

    Portal();
    void Update(float dt, std::vector<Tank*>& tanks, std::vector<Bullet*>& bullets);
    void Draw();
    void Reset();
};