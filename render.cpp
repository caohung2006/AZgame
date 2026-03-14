#include "lib.h"

void Render() {
    BeginDrawing();
    ClearBackground(RAYWHITE);

    // Vẽ các bức tường
    for (b2Body* wall : walls) {
        b2Fixture* fixture = wall->GetFixtureList();
        b2PolygonShape* shape = (b2PolygonShape*)fixture->GetShape();

        // Lấy nửa chiều rộng và nửa chiều cao từ shape
        // Đối với hình hộp, m_vertices[1].x là nửa chiều rộng, m_vertices[2].y là nửa chiều cao
        float halfWidth = shape->m_vertices[1].x;
        float halfHeight = shape->m_vertices[2].y;

        b2Vec2 pos = wall->GetPosition();
        float angle = wall->GetAngle();

        Rectangle rec = {
            pos.x * SCALE,
            600 - pos.y * SCALE,
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
    float y = 600 - pos.y * SCALE;

    Rectangle pRec = { x, y, 40, 40 };
    Vector2 pOrigin = { 20, 20 };
    DrawRectanglePro(pRec, pOrigin, -pAngle * RAD2DEG, RED); // Vẽ thân xe

    // Vẽ nòng súng (để nhìn rõ hướng đang quay mặt)
    Vector2 endPoint = { x - sinf(pAngle) * 30.0f, y - cosf(pAngle) * 30.0f };
    DrawLineEx((Vector2) { x, y }, endPoint, 4.0f, BLACK);

    // Vẽ tất cả các viên đạn
    for (Bullet* bullet : bullets) {
        bullet->Draw();
    }

    DrawText("WASD to move", 10, 10, 20, BLACK);

    EndDrawing();
}