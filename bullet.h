#pragma once
#include "constants.h"

class Bullet {
public:
    b2Body* body;
    float time;

    Bullet(b2World& world, b2Vec2 position, b2Vec2 velocity);
    void Draw();
};