#include "bullet.h"
#include "tank.h"

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
    time = 7.0f;
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

void Bullet::Update(float dt, const std::vector<Tank*>& tanks) {
    time -= dt;
    if (IsDead()) return;

    // Logic xử lý đạn đuổi tìm mục tiêu (Tên lửa)
    if (isMissile) {
        float elapsed = 5.0f - time;
        b2Vec2 currentVel = body->GetLinearVelocity();
        float currentSpeed = currentVel.Length();
        
        if (currentSpeed > 0.0f) {
            if (elapsed < 2.0f) {
                // Trong 2.0s đầu tiên, đạn lượn lờ hình sin (sử dụng cos để xoay vận tốc) để tạo cảm giác thực
                float waveTurn = cosf(elapsed * 12.0f) * 4.0f * dt;
                float angle = atan2f(currentVel.y, currentVel.x) + waveTurn;
                body->SetLinearVelocity(b2Vec2(cosf(angle) * currentSpeed, sinf(angle) * currentSpeed));
            } else {
                // Sau 2.0s, tìm kiếm xe tăng BẤT KỲ gần nhất trên bản đồ
                Tank* target = nullptr;
                float minDist = 9999.0f;
                for (Tank* t : tanks) {
                    if (!t->isDestroyed) { 
                        float dist = (t->body->GetPosition() - body->GetPosition()).Length();
                        if (dist < minDist) { minDist = dist; target = t; }
                    }
                }
                
                // Thuật toán điều hướng dần dần về phía mục tiêu
                if (target) {
                    b2Vec2 toTarget = target->body->GetPosition() - body->GetPosition();
                    toTarget.Normalize();
                    b2Vec2 normVel = currentVel;
                    normVel.Normalize();
                    
                    // Tính cross product 2D để tìm hướng cần xoay (-1 hoặc 1)
                    float cross = normVel.x * toTarget.y - normVel.y * toTarget.x;
                    float turnSpeed = 3.5f * dt; // tốc độ bẻ lái
                    
                    float angle = atan2f(normVel.y, normVel.x);
                    if (cross > 0.1f) angle += turnSpeed;
                    else if (cross < -0.1f) angle -= turnSpeed;
                    
                    body->SetLinearVelocity(b2Vec2(cosf(angle) * currentSpeed, sinf(angle) * currentSpeed));
                }
            }
        }
    }
}

bool Bullet::IsDead() const {
    return time <= 0.0f || explodeFrag;
}
