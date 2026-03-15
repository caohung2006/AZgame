#include "lib.h"

// Hàm khởi tạo: Chạy 1 lần duy nhất để thiết lập thông số cho xe tăng khi bắt đầu game
void Tank::Init() {
        b2BodyDef tankDef; // Định nghĩa các thuộc tính vật lý cơ bản cho xe tăng
        tankDef.type = b2_dynamicBody; // Đặt là vật thể động (có thể di chuyển, bị đẩy lùi và nảy)
        tankDef.position.Set((SCREEN_WIDTH / 2.0f) / SCALE, (SCREEN_HEIGHT / 2.0f) / SCALE); // Đặt vị trí sinh ra ở chính giữa màn hình
        tankDef.linearDamping = 10.0f; // Lực cản (ma sát mặt đất) giúp xe tự dừng lại dần khi nhả phím di chuyển

        body = world.CreateBody(&tankDef); // Đưa xe tăng vào thế giới mô phỏng vật lý Box2D

        // --- Cấu hình Hitbox (Khu vực va chạm) ---
        b2PolygonShape shape;
        shape.SetAsBox(13.0f / SCALE, 15.0f / SCALE); // Thiết lập hitbox hình chữ nhật (Kích thước thực: Rộng 26, Dài 30)

        b2FixtureDef fix;
        fix.shape = &shape;
        fix.density = 1.0f;
        fix.friction = 0.0f;

        body->CreateFixture(&fix); // Gắn hitbox và các thuộc tính (khối lượng, ma sát) vào thân xe tăng
}

// Hàm xử lý logic: Lắng nghe phím bấm để di chuyển và bắn đạn (được gọi liên tục mỗi khung hình)
void Tank::Move() {

        // --- 1. THIẾT LẬP TỐC ĐỘ ---
        float moveSpeed = 6.0f;
        float turnSpeed = 3.0f; // Tốc độ xoay (radians/giây)

        // --- 2. XỬ LÝ XOAY (Phím A và D) ---
        float angularVel = 0.0f;
        if (IsKeyDown(KEY_A)) angularVel += turnSpeed; // Xoay trái
        if (IsKeyDown(KEY_D)) angularVel -= turnSpeed; // Xoay phải
        body->SetAngularVelocity(angularVel);

        // --- 3. XỬ LÝ TIẾN LÙI (Phím W và S) ---
        b2Vec2 vel(0.0f, 0.0f);
        float currentAngle = body->GetAngle(); // Lấy góc xoay hiện tại của thân xe
        
        // Dùng lượng giác (sin, cos) để tính ra hướng mũi tên chỉ thẳng về phía trước mặt xe tăng
        b2Vec2 forwardDir(-sinf(currentAngle), cosf(currentAngle));

        // Cộng/trừ vận tốc dọc theo hướng chỉ định để tiến hoặc lùi
        if (IsKeyDown(KEY_W)) { vel.x += forwardDir.x * moveSpeed; vel.y += forwardDir.y * moveSpeed; }
        if (IsKeyDown(KEY_S)) { vel.x -= forwardDir.x * moveSpeed; vel.y -= forwardDir.y * moveSpeed; }

        body->SetLinearVelocity(vel);

        // --- 4. XỬ LÝ BẮN ĐẠN (Phím Q) ---
        // Sử dụng IsKeyPressed (chỉ nhận 1 lần chạm) giúp người chơi không bị xả đạn liên tục nếu lỡ giữ phím
        if (IsKeyPressed(KEY_Q)) {
            // Đẩy vị trí sinh ra đạn ra xa 25 pixel theo hướng nòng súng
            // Việc này cực kỳ quan trọng để tránh lỗi vật lý: đạn sinh ra ở giữa xe và tự va chạm với chính xe tăng
            b2Vec2 spawnPos = body->GetPosition() + (25.0f / SCALE) * forwardDir;
            
            // Gán vận tốc cho đạn bay thẳng theo hướng nòng súng (Tốc độ bay: 15.0f)
            b2Vec2 bulletVel = 15.0f * forwardDir;
            
            bullets.push_back(new Bullet(spawnPos, bulletVel));
        }
}

void Tank::Info() {

}

Tank tank;
