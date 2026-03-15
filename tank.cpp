#include "lib.h"

// Hàm Init: Sẽ chạy 1 lần khi bạn tạo đối tượng Tank
void Tank::Init() {
        b2BodyDef tankDef;                                    // hàm thuộc tính
        tankDef.type = b2_dynamicBody;                        // kiểu của vật thể
        tankDef.position.Set(400.0f / SCALE, 300.0f / SCALE); // vị trí ban đầu
        tankDef.linearDamping = 10.0f;                        // giảm trượt

        body = world.CreateBody(&tankDef);                    // cho vào thế giới

        // Hitbox
        b2PolygonShape shape;
        shape.SetAsBox(13.0f / SCALE, 15.0f / SCALE); // Đổi nửa chiều rộng thành 13 (Tổng rộng 26, dài 30)

        b2FixtureDef fix;
        fix.shape = &shape;
        fix.density = 1.0f;
        fix.friction = 0.0f;

        body->CreateFixture(&fix);
}

// Hàm Move: dùng để gọi mỗi khung hình
void Tank::Move() {

        // ===== INPUT =====
        float moveSpeed = 6.0f;
        float turnSpeed = 3.0f; // Tốc độ xoay (radians/giây)

        // 1. Xử lý xoay (Phím A và D)
        float angularVel = 0.0f;
        if (IsKeyDown(KEY_A)) angularVel += turnSpeed; // Xoay trái
        if (IsKeyDown(KEY_D)) angularVel -= turnSpeed; // Xoay phải
        body->SetAngularVelocity(angularVel);

        // 2. Xử lý tiến lùi (Phím W và S) theo hướng đang quay mặt
        b2Vec2 vel(0.0f, 0.0f);
        float currentAngle = body->GetAngle();
        // Tính toán véc tơ hướng lên dựa theo góc hiện tại
        b2Vec2 forwardDir(-sinf(currentAngle), cosf(currentAngle));

        if (IsKeyDown(KEY_W)) { vel.x += forwardDir.x * moveSpeed; vel.y += forwardDir.y * moveSpeed; }
        if (IsKeyDown(KEY_S)) { vel.x -= forwardDir.x * moveSpeed; vel.y -= forwardDir.y * moveSpeed; }

        body->SetLinearVelocity(vel);

        // 3. Xử lý bắn đạn (Phím Q)
        // Dùng IsKeyPressed để đảm bảo mỗi lần bấm chỉ bắn 1 viên (không bị bắn liên thanh)
        if (IsKeyPressed(KEY_Q)) {
            // Tính toạ độ mũi nòng súng để đạn không xuất hiện từ giữa thân xe (tránh tự bắn trúng mình)
            // Vị trí = Vị trí xe + Hướng * Khoảng cách (Giảm xuống 25 pixel cho khớp nòng súng mới)
            b2Vec2 spawnPos = body->GetPosition() + (25.0f / SCALE) * forwardDir;
            
            // Tính toán vận tốc đạn bắn ra (gấp khoảng 2.5 lần tốc độ xe tăng)
            b2Vec2 bulletVel = 15.0f * forwardDir;
            
            bullets.push_back(new Bullet(spawnPos, bulletVel));
        }
}

void Tank::Info() {

}

Tank tank;
