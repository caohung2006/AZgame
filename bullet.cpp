#include "bullet.h"

Bullet::Bullet(b2World& world, b2Vec2 position, b2Vec2 velocity, bool _isLaser, bool _isFrag, bool _isMissile, int _owner) {
    isLaser = _isLaser;
    isFrag = _isFrag;
    isMissile = _isMissile;
    explodeFrag = false;
    ownerPlayerIndex = _owner;
    b2BodyDef def;
    def.type = b2_dynamicBody;
    def.position = position;
    def.bullet = true;
    time = 10.0f;
    body = world.CreateBody(&def);

    b2CircleShape shape; 
    shape.m_radius = (isFrag || isMissile) ? 5.0f / SCALE : 3.0f / SCALE;
    b2FixtureDef fix; fix.shape = &shape; fix.density = 1.0f; fix.friction = 0.0f; fix.restitution = 1.0f;
    fix.filter.groupIndex = -1;
    body->CreateFixture(&fix);
    
    if (isLaser) {
        velocity.x *= 8.0f; // Laser đi cực nhanh
        velocity.y *= 8.0f;
        time = 0.5f; // laser tồn tại ngắn
    } else if (isMissile) {
        velocity.x *= 1.5f;
        velocity.y *= 1.5f;
        time = 5.0f; // Rút ngắn thời gian tồn tại tên lửa
    } else if (isFrag) {
        time = 1.5f; // Đạn to tự động nổ sau 1.5 giây nếu không kích
    }
    
    body->SetLinearVelocity(velocity);
}

void Bullet::Draw() {
    b2Vec2 pos = body->GetPosition();
    float x = pos.x * SCALE;
    float y = SCREEN_HEIGHT - pos.y * SCALE;
    
    if (isLaser) {
        b2Vec2 v = body->GetLinearVelocity();
        float len = v.Length();
        if (len > 0.0f) { v.x /= len; v.y /= len; }
        DrawLineEx({x - v.x * 20.0f, y + v.y * 20.0f}, {x + v.x * 20.0f, y - v.y * 20.0f}, 4.0f, RED);
    } else if (isFrag) {
        DrawCircle(x, y, 5.0f, BLACK);
        DrawCircleLines(x, y, 6.0f, RED);
    } else if (isMissile) {
        b2Vec2 v = body->GetLinearVelocity();
        float angle = atan2f(-v.y, v.x) * RAD2DEG;
        DrawRectanglePro({x, y, 10.0f, 6.0f}, {5.0f, 3.0f}, angle, ORANGE);
        DrawCircle(x, y, 2.0f, RED); // Mũi đỏ
    } else {
        DrawCircle(x, y, 3.0f, BLACK);
    }
}
