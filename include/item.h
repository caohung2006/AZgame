#pragma once
#include "constants.h"

enum class ItemType {
    NORMAL,
    GATLING,
    FRAG,
    MISSILE,
    DEATH_RAY
};

class Item {
public:
    b2Body* body;
    ItemType type;
    bool isDestroyed;

    Item(b2World& world, b2Vec2 position, ItemType _type);
    void Draw();
};
