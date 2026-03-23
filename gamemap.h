#pragma once
#include "constants.h"

class GameMap {
private:
    std::vector<b2Body*> walls;
public:
    void Build(b2World& world);
    void Draw();
    void Clear(b2World& world);
};