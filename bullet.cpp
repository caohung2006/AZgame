#include "bullet.h"

Bullet::Bullet(b2World& world, b2Vec2 position, b2Vec2 velocity) {
    b2BodyDef def;
    def.type = b2_dynamicBody;
    def.position = position;
    def.bullet = true;
    time = 10.0f;
    body = world.CreateBody(&def);

    b2CircleShape shape; shape.m_radius = 3.0f / SCALE;
    b2FixtureDef fix; fix.shape = &shape; fix.density = 1.0f; fix.friction = 0.0f; fix.restitution = 1.0f;
    fix.filter.groupIndex = -1;
    body->CreateFixture(&fix);
    body->SetLinearVelocity(velocity);
}

void Bullet::Draw() {
    b2Vec2 pos = body->GetPosition();
    float x = pos.x * SCALE;
    float y = SCREEN_HEIGHT - pos.y * SCALE;
    DrawCircle(x, y, 3.0f, BLACK);
}
