#include "lib.h"

std::vector<Bullet*> bullets;

Bullet::Bullet(b2Vec2 position, b2Vec2 velocity) {
    b2BodyDef def;
    def.type = b2_dynamicBody;
    def.position = position;
    
    // Rất quan trọng: Bật tính năng Bullet (CCD) để đạn bay nhanh không bị xuyên qua tường mỏng
    def.bullet = true; 

    body = world.CreateBody(&def);

    b2CircleShape shape;
    shape.m_radius = 4.0f / SCALE; // Bán kính viên đạn

    b2FixtureDef fix;
    fix.shape = &shape;
    fix.density = 1.0f;
    fix.friction = 0.0f;     // Không có ma sát cản trở
    fix.restitution = 1.0f;  // Nảy 100% khi đập tường (Giống hệt game AZ)

    body->CreateFixture(&fix);
    
    // Bắn đạn đi với vận tốc truyền vào
    body->SetLinearVelocity(velocity);
}

void Bullet::Draw() {
    b2Vec2 pos = body->GetPosition();
    float x = pos.x * SCALE;
    float y = 600 - pos.y * SCALE;
    
    DrawCircle(x, y, 4.0f, BLACK); // Vẽ viên đạn màu đen
}
