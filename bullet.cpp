#include "lib.h"

std::vector<Bullet*> bullets;

// Hàm khởi tạo viên đạn mới với vị trí và vận tốc truyền vào từ nòng súng
Bullet::Bullet(b2Vec2 position, b2Vec2 velocity) {
    b2BodyDef def;
    def.type = b2_dynamicBody; // Đạn là vật thể có thể di chuyển
    time = 10.0f; // Đặt thời gian tồn tại tối đa là 10 giây
    def.position = position;

    // Tính năng Bullet (Continuous Collision Detection): 
    // Yêu cầu Box2D tính toán va chạm chi tiết hơn để đạn bay ở tốc độ cao không bị lỗi bay xuyên qua tường mỏng
    def.bullet = true;

    body = world.CreateBody(&def);

    // --- Cấu hình Hitbox cho đạn ---
    b2CircleShape shape;
    shape.m_radius = 3.0f / SCALE; // Đạn có dạng hình tròn, bán kính 3 pixel

    b2FixtureDef fix;
    fix.shape = &shape;
    fix.density = 1.0f;      // Khối lượng đạn
    fix.friction = 0.0f;     // Không có ma sát, giúp đạn trượt nhẹ nhàng mà không mất hướng
    fix.restitution = 1.0f;  // Độ nảy 1.0 (100%): Đạn đập tường sẽ nảy lại bảo toàn lực hoàn toàn (Giống game AZ)

    // Hệ thống lọc va chạm: Các vật thể có cùng groupIndex âm sẽ KHÔNG va chạm với nhau (Đạn bay xuyên qua đạn)
    fix.filter.groupIndex = -1;

    body->CreateFixture(&fix);

    // Cấp phát lực bắn ban đầu để đạn bay đi
    body->SetLinearVelocity(velocity);
}
void Lifetime(float dt) {
    for (auto it = bullets.begin(); it != bullets.end(); ) {
        Bullet* bullet = *it;
        bullet->time -= dt; // Trừ dần thời gian tồn tại

        if (bullet->time <= 0.0f) {
            world.DestroyBody(bullet->body); // Xoá vật thể đạn khỏi thế giới vật lý Box2D
            delete bullet;                   // Xoá đạn khỏi bộ nhớ (tránh rò rỉ RAM)
            it = bullets.erase(it);          // Xoá con trỏ đạn khỏi danh sách quản lý
        }
        else {
            ++it; // Đi tiếp đến viên đạn tiếp theo nếu chưa hết hạn
        }
    }
    return;
}
