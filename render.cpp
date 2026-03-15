#include "lib.h"

// Hàm phụ trách toàn bộ việc vẽ đồ hoạ ra màn hình mỗi khung hình
void DrawGame() {
    BeginDrawing();
    ClearBackground(RAYWHITE); // Xoá khung hình cũ bằng màu nền trắng

    // --- 1. VẼ MÊ CUNG (TƯỜNG) ---
    for (b2Body* wall : walls) {
        b2Fixture* fixture = wall->GetFixtureList();
        b2PolygonShape* shape = (b2PolygonShape*)fixture->GetShape();

        // Lấy thông số nửa chiều rộng (half-width) và nửa chiều cao (half-height) từ hitbox
        float halfWidth = shape->m_vertices[1].x;
        float halfHeight = shape->m_vertices[2].y;

        b2Vec2 pos = wall->GetPosition();
        float angle = wall->GetAngle();

        Rectangle rec = {
            pos.x * SCALE,
            SCREEN_HEIGHT - pos.y * SCALE, // Tự động lật trục Y theo màn hình
            halfWidth * 2 * SCALE,
            halfHeight * 2 * SCALE
        };
        Vector2 origin = { rec.width / 2.0f, rec.height / 2.0f };

        DrawRectanglePro(rec, origin, angle * RAD2DEG, GRAY);
    }

    // Vẽ người chơi (Xe tăng)
    b2Vec2 pos = tank.body->GetPosition();
    float pAngle = tank.body->GetAngle();

    float x = pos.x * SCALE;
    float y = SCREEN_HEIGHT - pos.y * SCALE; // Tự động lật trục Y

    Rectangle pRec = { x, y, 26, 30 }; // Đổi kích thước hình vẽ thành 26x30
    Vector2 pOrigin = { 13, 15 };      // Tâm xoay tương ứng là 13 và 15
    DrawRectanglePro(pRec, pOrigin, -pAngle * RAD2DEG, RED); // Vẽ thân xe

    // Vẽ nòng súng (để nhìn rõ hướng đang quay mặt)
    Vector2 endPoint = { x - sinf(pAngle) * 22.0f, y - cosf(pAngle) * 22.0f }; // Thu ngắn nòng súng lại còn 22
    DrawLineEx((Vector2) { x, y }, endPoint, 4.0f, BLACK);

    
    // Vẽ tất cả các viên đạn

    for (Bullet* bullet : bullets) {
        b2Vec2 pos = bullet->body->GetPosition();
        float x = pos.x * SCALE;
        float y = SCREEN_HEIGHT - pos.y * SCALE; // Tự động lật trục Y
        DrawCircle(x, y, 3.0f, BLACK); // Đổi bán kính đạn thành 3.0
    }

    DrawText("WASD to move", 10, 10, 20, BLACK);

    EndDrawing();
}