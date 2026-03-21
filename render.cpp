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

    // Khởi tạo Texture một lần duy nhất (Tránh load ảnh liên tục mỗi khung hình gây tràn RAM)
    static Texture2D tankTexture = { 0 };
    if (tankTexture.id == 0) {
        tankTexture = LoadTexture("C:\\HungCao\\Project\\AZgame\\t-green-norm.png");
    }

    // Vẽ tất cả người chơi (Xe tăng)
    for (Tank* t : tanks) {
        b2Vec2 pos = t->body->GetPosition();
        float pAngle = t->body->GetAngle();

        float x = pos.x * SCALE;
        float y = SCREEN_HEIGHT - pos.y * SCALE; // Tự động lật trục Y

        Rectangle sourceRec = { 0.0f, 0.0f, (float)tankTexture.width, (float)tankTexture.height };
        Rectangle destRec = { x, y, 28.0f, 42.0f };
        Vector2 origin = { 14.0f, 21.0f };

        DrawTexturePro(tankTexture, sourceRec, destRec, origin, -pAngle * RAD2DEG, WHITE);
    }

    // --- VẼ HỐ ĐEN (BLACK HOLE) ---
    // DrawCircle(SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2, 15.0f, Fade(PURPLE, 0.6f));
    // DrawCircleLines(SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2, 17.0f, PURPLE);
    // DrawText("BLACK HOLE", SCREEN_WIDTH / 2 - 35, SCREEN_HEIGHT / 2 - 25, 10, PURPLE);
    
    // Vẽ tất cả các viên đạn

    for (Bullet* bullet : bullets) {
        b2Vec2 pos = bullet->body->GetPosition();
        float x = pos.x * SCALE;
        float y = SCREEN_HEIGHT - pos.y * SCALE; // Tự động lật trục Y
        DrawCircle(x, y, 3.0f, BLACK); // Đổi bán kính đạn thành 3.0
    }

    DrawText("WASD to move", 10, 10, 20, BLACK);

    // --- VẼ ĐIỂM SỐ Ở PHÍA DƯỚI MÀN HÌNH ---
    int sectionWidth = SCREEN_WIDTH / numPlayersGlobal; // Chia đều không gian hiển thị cho số người chơi
    for (int i = 0; i < numPlayersGlobal; i++) {
        const char* scoreText = TextFormat("Player %d: %d", i + 1, playerScores[i]);
        int textWidth = MeasureText(scoreText, 24); // Đo chiều rộng chữ để canh giữa đoạn
        int textX = (i * sectionWidth) + (sectionWidth / 2) - (textWidth / 2); // Toạ độ X căn giữa từng cột
        DrawText(scoreText, textX, SCREEN_HEIGHT - 40, 24, DARKBLUE);
    }

    EndDrawing();
}