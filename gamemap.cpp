#include "lib.h"

std::vector<b2Body*> walls;

void map()  // ===== TẠO TƯỜNG BAO QUANH =====
{
    // Viết một hàm tiện ích (lambda) giúp tạo tường cực nhanh bằng Pixel
    auto addWall = [](float x, float y, float width, float height) {
        b2BodyDef def;
        def.type = b2_staticBody;
        def.position.Set(x / SCALE, y / SCALE); // Đặt vị trí Tâm

        b2PolygonShape shape;
        shape.SetAsBox((width / 2.0f) / SCALE, (height / 2.0f) / SCALE); // Kích thước

        b2FixtureDef fixture;
        fixture.shape = &shape;
        fixture.density = 0.0f;

        b2Body* body = world.CreateBody(&def);
        body->CreateFixture(&fixture);
        walls.push_back(body);
    };

    // ===== TƯỜNG KHUNG BẢN ĐỒ BAO QUANH (600x400) =====
    addWall(400, 500, 620, 2); // Tường trên
    addWall(400, 100, 620, 2); // Tường dưới
    addWall(100, 300, 2, 400); // Tường trái (Đổi chiều dọc thành 400, độ dày thành 2)
    addWall(700, 300, 2, 400); // Tường phải

    // ===== CÁC VÁCH NGĂN MÊ CUNG BÊN TRONG =====
    // Góc trên bên trái
    addWall(250, 400, 150, 2); // Vách ngang
    addWall(250, 335, 2, 150); // Vách dọc nối (Độ dày 2, chiều dài 150)

    // Góc dưới bên phải
    addWall(550, 200, 150, 2); // Vách ngang
    addWall(550, 265, 2, 150); // Vách dọc nối

    // Vật cản trung tâm (Bảo vệ điểm sinh ra của xe tăng ở 400, 300)
    addWall(400, 200, 150, 2);
    addWall(335, 235, 2, 50); // Vách dọc nối
    addWall(465, 235, 2, 50); // Vách dọc nối

    // Các khối chướng ngại vật riêng lẻ
    addWall(600, 400, 50, 50); // Khối vuông cản đường 1 (Rộng 50, Cao 50)
    addWall(200, 200, 50, 50); // Khối vuông cản đường 2 (Rộng 50, Cao 50)
}