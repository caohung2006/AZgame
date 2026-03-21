#include "lib.h"

// Hàm khởi tạo: Chạy 1 lần duy nhất để thiết lập thông số cho xe tăng khi bắt đầu game
void Tank::Init(int _playerIndex, int _forwardKey, int _backwardKey, int _turnLeftKey, int _turnRightKey, int _shootKey) {

        playerIndex = _playerIndex; // Lưu lại ID của người chơi này
        // Gán phím bấm được truyền vào, nếu tham số bằng 0 thì lấy mặc định
        forwardKey = (_forwardKey != 0) ? _forwardKey : KEY_W;
        backwardKey = (_backwardKey != 0) ? _backwardKey : KEY_S;
        turnLeftKey = (_turnLeftKey != 0) ? _turnLeftKey : KEY_A;
        turnRightKey = (_turnRightKey != 0) ? _turnRightKey : KEY_D;
        shootKey = (_shootKey != 0) ? _shootKey : KEY_Q;
        shootCooldownTimer = 0.0f; // Đặt bằng 0 để xe tăng có thể bắn ngay lập tức khi vừa vào game
        isDestroyed = false; // Mặc định xe tăng còn sống khi mới sinh ra

        b2BodyDef tankDef; // Định nghĩa các thuộc tính vật lý cơ bản cho xe tăng
        tankDef.type = b2_dynamicBody; // Đặt là vật thể động (có thể di chuyển, bị đẩy lùi và nảy)
        tankDef.position.Set((SCREEN_WIDTH / 2.0f) / SCALE, (SCREEN_HEIGHT / 2.0f) / SCALE); // Đặt vị trí sinh ra ở chính giữa màn hình
        tankDef.fixedRotation = true; // Khoá xoay vật lý: Ngăn xe bị văng đuôi hoặc quay mòng mòng khi quẹt góc tường

        body = world.CreateBody(&tankDef); // Đưa xe tăng vào thế giới mô phỏng vật lý Box2D

        // --- Cấu hình Hitbox (Khu vực va chạm) ---
        b2PolygonShape shape;
        shape.SetAsBox(14.0f / SCALE, 21.0f / SCALE); // Thiết lập hitbox hình chữ nhật (Kích thước thực: Rộng 20, Dài 23)

        b2FixtureDef fix;
        fix.shape = &shape;
        fix.density = 1.0f;
        fix.friction = 0.0f;
        fix.restitution = 0.0f; // Đặt độ nảy về 0 để xe không bị nảy dội lại khi tông tường

        body->CreateFixture(&fix); // Gắn hitbox và các thuộc tính (khối lượng, ma sát) vào thân xe tăng
}

// Hàm xử lý logic: Lắng nghe phím bấm để di chuyển và bắn đạn (được gọi liên tục mỗi khung hình)
void Tank::Move() {

        // --- 1. THIẾT LẬP TỐC ĐỘ ---
        float moveSpeed = 2.5f;
        float turnSpeed = 2.5f; // Tốc độ xoay (radians/giây)

        // --- 2. XỬ LÝ XOAY (Phím A và D) ---
        float angularVel = 0.0f;
        if (IsKeyDown(turnLeftKey)) angularVel += turnSpeed; // Xoay trái
        if (IsKeyDown(turnRightKey)) angularVel -= turnSpeed; // Xoay phải
        body->SetAngularVelocity(angularVel);

        // --- 3. XỬ LÝ TIẾN LÙI (Phím W và S) ---
        b2Vec2 vel(0.0f, 0.0f);
        float currentAngle = body->GetAngle(); // Lấy góc xoay hiện tại của thân xe
        
        // Dùng lượng giác (sin, cos) để tính ra hướng mũi tên chỉ thẳng về phía trước mặt xe tăng
        b2Vec2 forwardDir(-sinf(currentAngle), cosf(currentAngle));

        // Cộng/trừ vận tốc dọc theo hướng chỉ định để tiến hoặc lùi
        if (IsKeyDown(forwardKey)) { vel.x += forwardDir.x * moveSpeed; vel.y += forwardDir.y * moveSpeed; }
        if (IsKeyDown(backwardKey)) { vel.x -= forwardDir.x * moveSpeed; vel.y -= forwardDir.y * moveSpeed; }

        body->SetLinearVelocity(vel);

        // --- 4. XỬ LÝ BẮN ĐẠN (Phím Q) ---
        // Giảm dần thời gian chờ mỗi khung hình (GetFrameTime() trả về thời gian tính bằng giây)
        if (shootCooldownTimer > 0.0f) shootCooldownTimer -= GetFrameTime();

        // Sử dụng IsKeyPressed (chỉ nhận 1 lần chạm) giúp người chơi không bị xả đạn liên tục nếu lỡ giữ phím
        if (IsKeyPressed(shootKey) && shootCooldownTimer <= 0.0f) {
            b2Vec2 startPos = body->GetPosition();
            b2Vec2 spawnPos = startPos + (24.5f / SCALE) * forwardDir;

            // Kiểm tra Raycast: Phóng tia từ tâm xe đến vị trí nòng súng
            // Nếu có bức tường (b2_staticBody) nào nằm giữa, nghĩa là súng đang bị kẹt
            class WallRayCastCallback : public b2RayCastCallback {
            public:
                bool hitWall = false;
                float ReportFixture(b2Fixture* fixture, const b2Vec2& point, const b2Vec2& normal, float fraction) override {
                    if (fixture->GetBody()->GetType() == b2_staticBody) { // Phát hiện tường
                        hitWall = true;
                        return fraction; // Dừng tia dò ngay khi chạm tường
                    }
                    return -1.0f; // Bỏ qua nếu chạm trúng đạn hoặc xe tăng khác
                }
            };

            WallRayCastCallback callback;
            world.RayCast(&callback, startPos, spawnPos);

            // Chỉ cho phép sinh đạn nếu không có tường chắn ngang nòng súng
            if (!callback.hitWall) {
                b2Vec2 bulletVel = 5.0f * forwardDir;
                bullets.push_back(new Bullet(spawnPos, bulletVel));
                shootCooldownTimer = 0.5f; // Đặt lại thời gian chờ là 1.0 giây sau khi bắn đạn thành công
            }
        }

        // --- 5. KIỂM TRA XE TĂNG BỊ TRÚNG ĐẠN ---
        // Box2D tự động lưu danh sách các vật thể đang chạm vào xe tăng
        for (b2ContactEdge* edge = body->GetContactList(); edge; edge = edge->next) {
            if (edge->contact->IsTouching()) {
                b2Body* otherBody = edge->other;
                
                // Kiểm tra xem vật thể đang chạm vào xe tăng có phải là viên đạn nào không
                for (Bullet* bullet : bullets) {
                    if (otherBody == bullet->body) {
                        // TÌM THẤY VA CHẠM VỚI ĐẠN! In ra màn hình console (Terminal) để kiểm tra
                        TraceLog(LOG_WARNING, "!!! XE TANG BI TRUNG DAN !!!");
                        
                        // (Tuỳ chọn) Ép thời gian sống của đạn về 0 để hàm Lifetime() tự động xoá nó ngay sau khi trúng
                        bullet->time = 0.0f;
                        isDestroyed = true; // Bật cờ tiêu diệt xe tăng này
                    }
                }
            }
        }
}

void Tank::Info() {

}
