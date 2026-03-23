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

    // --- VẼ CỔNG DỊCH CHUYỂN (PORTAL) ---
    if (teleportPortal.isActive) {
        float ax = teleportPortal.posA.x * SCALE;
        float ay = SCREEN_HEIGHT - teleportPortal.posA.y * SCALE;
        float bx = teleportPortal.posB.x * SCALE;
        float by = SCREEN_HEIGHT - teleportPortal.posB.y * SCALE;
        
        // Vẽ hiệu ứng cho cổng A (Màu Tím)
        DrawCircle((int)ax, (int)ay, 20.0f, Fade(PURPLE, 0.5f));
        DrawCircleLines((int)ax, (int)ay, 22.0f, PURPLE);
        DrawText("A", (int)ax - 6, (int)ay - 10, 20, WHITE);

        // Vẽ hiệu ứng cho cổng B (Màu Xanh dương)
        DrawCircle((int)bx, (int)by, 20.0f, Fade(BLUE, 0.5f));
        DrawCircleLines((int)bx, (int)by, 22.0f, BLUE);
        DrawText("B", (int)bx - 6, (int)by - 10, 20, WHITE);
    }

    // Vẽ tất cả người chơi (Xe tăng) bằng các khối hình học khớp với Hitbox
    for (Tank* t : tanks) {
        b2Vec2 pos = t->body->GetPosition();
        float pAngle = t->body->GetAngle();

        float x = pos.x * SCALE;
        float y = SCREEN_HEIGHT - pos.y * SCALE; // Tự động lật trục Y
        float rot = -pAngle * RAD2DEG; // Đổi sang độ và đảo chiều quay cho Raylib

        // Phân loại màu sắc theo người chơi
        Color hullColor = DARKGRAY;
        if (t->playerIndex == 0) hullColor = DARKGREEN;
        else if (t->playerIndex == 1) hullColor = DARKBLUE;
        else if (t->playerIndex == 2) hullColor = MAROON;
        else if (t->playerIndex == 3) hullColor = GOLD;

        // 1. Vẽ nòng súng (Barrel) - Rộng 6, Cao 14. Box2D offset Y=14 -> Raylib offset Y=-14.
        // Khoảng cách từ góc trên bên trái nòng súng đến tâm xe tăng là X=3, Y=21
        Rectangle barrelRec = { x, y, 6.0f, 14.0f };
        Vector2 barrelOrigin = { 3.0f, 21.0f };
        DrawRectanglePro(barrelRec, barrelOrigin, rot, GRAY);

        // 2. Vẽ thân xe (Hull) - Rộng 28, Cao 28. Box2D offset Y=-7 -> Raylib offset Y=7.
        // Khoảng cách từ góc trên bên trái thân xe đến tâm xe tăng là X=14, Y=7
        Rectangle hullRec = { x, y, 28.0f, 28.0f };
        Vector2 hullOrigin = { 14.0f, 7.0f };
        DrawRectanglePro(hullRec, hullOrigin, rot, hullColor);
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

    // --- VẼ NÚT SETTINGS ---
    DrawRectangle(SCREEN_WIDTH - 120, 10, 110, 30, LIGHTGRAY);
    DrawRectangleLines(SCREEN_WIDTH - 120, 10, 110, 30, DARKGRAY);
    DrawText("SETTINGS", SCREEN_WIDTH - 105, 16, 20, DARKGRAY);

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