#include "item.h"

Item::Item(b2World& world, b2Vec2 position, ItemType _type) {
    type = _type;
    isDestroyed = false;

    b2BodyDef def;
    def.type = b2_staticBody;
    def.position = position;
    body = world.CreateBody(&def);

    b2PolygonShape shape;
    shape.SetAsBox(10.0f / SCALE, 10.0f / SCALE);

    b2FixtureDef fix;
    fix.shape = &shape;
    fix.isSensor = true; // Does not act as a physical barrier
    body->CreateFixture(&fix);
}

void Item::Draw() {
    b2Vec2 pos = body->GetPosition();
    float x = pos.x * SCALE;
    float y = SCREEN_HEIGHT - pos.y * SCALE;

    DrawRectangle(x - 10, y - 10, 20, 20, DARKGRAY);
    DrawRectangleLines(x - 10, y - 10, 20, 20, BLACK);

    if (type == ItemType::DEATH_RAY) {
        // Vẽ tia lượn sóng chữ S (2 đoạn chéo)
        DrawLineEx({x - 4.0f, y - 6.0f}, {x + 4.0f, y - 2.0f}, 2.0f, BLACK);
        DrawLineEx({x + 4.0f, y - 2.0f}, {x - 4.0f, y + 2.0f}, 2.0f, BLACK);
        DrawLineEx({x - 4.0f, y + 2.0f}, {x + 4.0f, y + 6.0f}, 2.0f, BLACK);
    } else if (type == ItemType::GATLING) {
        // Vẽ 5 chấm xúc xắc
        DrawCircle(x - 4, y - 4, 1.5f, BLACK);
        DrawCircle(x + 4, y - 4, 1.5f, BLACK);
        DrawCircle(x, y, 1.5f, BLACK);
        DrawCircle(x - 4, y + 4, 1.5f, BLACK);
        DrawCircle(x + 4, y + 4, 1.5f, BLACK);
    } else if (type == ItemType::FRAG) {
        // Vẽ ngôi sao nổ, nhiều tia
        for(int i = 0; i < 8; i++) {
            float angle = i * PI / 4.0f;
            DrawLineEx({x, y}, {x + cosf(angle)*7.0f, y + sinf(angle)*7.0f}, 2.0f, BLACK);
        }
    } else if (type == ItemType::MISSILE) {
        // Vẽ tên lửa mũi nhọn hướng chéo
        DrawTriangle({x - 5, y + 5}, {x + 5, y - 5}, {x, y + 7}, BLACK);
        DrawTriangle({x - 5, y + 5}, {x + 5, y - 5}, {x - 7, y}, BLACK);
    }
}
