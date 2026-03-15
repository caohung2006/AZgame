#include "lib.h"

std::vector<b2Body*> walls;

// Hàm khởi tạo và xây dựng bản đồ (mê cung)
void BuildMap() {
    // Hàm lambda tiện ích để tạo nhanh một bức tường dựa trên toạ độ Pixel màn hình
    auto addWall = [](float x, float y, float width, float height) {
        b2BodyDef def;
        def.type = b2_staticBody; // Tường là vật thể tĩnh, không di chuyển và không bị ảnh hưởng bởi lực
        
        // Đảo trục Y (Vì Raylib dùng gốc toạ độ ở góc trên trái, còn Box2D dùng chuẩn toán học ở dưới trái)
        def.position.Set(x / SCALE, (SCREEN_HEIGHT - y) / SCALE); 

        b2PolygonShape shape;
        // Box2D nhận kích thước là một nửa chiều rộng (half-width) và một nửa chiều cao (half-height)
        shape.SetAsBox((width / 2.0f) / SCALE, (height / 2.0f) / SCALE); 

        b2FixtureDef fixture;
        fixture.shape = &shape;
        fixture.density = 0.0f; // Tường không cần khối lượng

        b2Body* body = world.CreateBody(&def);
        body->CreateFixture(&fixture);
        walls.push_back(body); // Lưu vào danh sách để sử dụng ở khâu Render
    };

    // --- 1. TẠO KHUNG TƯỜNG BAO QUANH MÀN HÌNH ---
    float centerX = SCREEN_WIDTH / 2.0f;
    float centerY = SCREEN_HEIGHT / 2.0f;
    
    addWall(centerX, 1.5f, SCREEN_WIDTH, 3);    // Tường trên 
    addWall(centerX, SCREEN_HEIGHT - 1.5f, SCREEN_WIDTH, 3);  // Tường dưới 
    addWall(1.5f, centerY, 3, SCREEN_HEIGHT);     // Tường trái 
    addWall(SCREEN_WIDTH - 1.5f, centerY, 3, SCREEN_HEIGHT);  // Tường phải 

    // --- 2. TẠO MÊ CUNG NGẪU NHIÊN KIỂU GAME AZ ---
    // Chia màn hình thành các ô lưới cách nhau 120 pixel để đặt vách ngăn
    for (int x = 120; x <= SCREEN_WIDTH - 120; x += 120) {
        for (int y = 120; y <= SCREEN_HEIGHT - 120; y += 120) {
            
            // Bỏ qua khu vực chính giữa màn hình để chừa lại "Vùng an toàn" cho xe tăng sinh ra không bị đè vào tường
            if (abs(x - centerX) <= 120 && abs(y - centerY) <= 120) {
                continue; 
            }

            // Chọn ngẫu nhiên một trong các mẫu vách ngăn
            int wallType = GetRandomValue(0, 4); 

            if (wallType == 0) {
                addWall(x, y, 140, 3); // Vách nằm ngang
            } 
            else if (wallType == 1) {
                addWall(x, y, 3, 140); // Vách nằm dọc
            } 
            else if (wallType == 2) {
                addWall(x, y, 20, 20); // Cột trụ vuông nhỏ
            } 
            else if (wallType == 3) {
                addWall(x, y, 140, 3); // Vách hình chữ thập (ngang)
                addWall(x, y, 3, 140); // Vách hình chữ thập (dọc)
            }
            // wallType == 4: Để trống không đặt tường nhằm tạo lối đi rộng
        }
    }
}