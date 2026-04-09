#include "renderer.h"
#include <raylib.h>

// ========================================================================
// Vẽ toàn bộ thế giới game
// ========================================================================
void Renderer::DrawWorld(const Game& game) {
    DrawMap(game.map);
    DrawPortal(game.portal);
    for (const Tank* t : game.tanks) DrawTank(*t);
    for (const Item* item : game.items) DrawItem(*item);
    for (const Bullet* b : game.bullets) DrawBullet(*b);
}

// ========================================================================
// Vẽ xe tăng: nòng súng, thân, tia laser sight, khiên
// ========================================================================
void Renderer::DrawTank(const Tank& tank) {
    b2Vec2 pos = tank.body->GetPosition();
    float rot = -tank.body->GetAngle() * RAD2DEG;
    float x = pos.x * SCALE, y = SCREEN_HEIGHT - pos.y * SCALE;

    // Vẽ laser sight cho Death Ray
    if (tank.currentWeapon == ItemType::DEATH_RAY) {
        float currentAngle = tank.body->GetAngle();
        b2Vec2 forwardDir(-sinf(currentAngle), cosf(currentAngle));
        DrawLineEx({x + forwardDir.x * 30.0f, y - forwardDir.y * 30.0f}, 
                   {x + forwardDir.x * 800.0f, y - forwardDir.y * 800.0f}, 2.0f, ColorAlpha(RED, 0.4f));
    }

    // Màu thân theo player index
    Color hullColor = (tank.playerIndex == 0) ? DARKGREEN : 
                      (tank.playerIndex == 1) ? DARKBLUE : 
                      (tank.playerIndex == 2) ? MAROON : GOLD;

    // Nòng súng
    DrawRectanglePro({ x, y, 6.0f, 14.0f }, { 3.0f, 21.0f }, rot, GRAY);
    // Thân xe tăng
    DrawRectanglePro({ x, y, 28.0f, 28.0f }, { 14.0f, 7.0f }, rot, hullColor);

    // Khiên
    if (tank.hasShield) {
        DrawCircle((int)x, (int)y, 25.0f, ColorAlpha(SKYBLUE, 0.3f));
        DrawCircleLines((int)x, (int)y, 25.0f, BLUE);
    }
}

// ========================================================================
// Vẽ viên đạn theo loại: thường, laser, frag, missile
// ========================================================================
void Renderer::DrawBullet(const Bullet& bullet) {
    b2Vec2 pos = bullet.body->GetPosition();
    float x = pos.x * SCALE;
    float y = SCREEN_HEIGHT - pos.y * SCALE;

    if (bullet.isLaser) {
        b2Vec2 v = bullet.body->GetLinearVelocity();
        float len = v.Length();
        if (len > 0.0f) { v.x /= len; v.y /= len; }
        DrawLineEx({x - v.x * 20.0f, y + v.y * 20.0f}, {x + v.x * 20.0f, y - v.y * 20.0f}, 4.0f, RED);
    } else if (bullet.isFrag) {
        DrawCircle((int)x, (int)y, 5.0f, BLACK);
        DrawCircleLines((int)x, (int)y, 6.0f, RED);
    } else if (bullet.isMissile) {
        b2Vec2 v = bullet.body->GetLinearVelocity();
        float angle = atan2f(-v.y, v.x) * RAD2DEG;
        DrawRectanglePro({x, y, 10.0f, 6.0f}, {5.0f, 3.0f}, angle, ORANGE);
        DrawCircle((int)x, (int)y, 2.0f, RED);
    } else {
        DrawCircle((int)x, (int)y, 3.0f, BLACK);
    }
}

// ========================================================================
// Vẽ tất cả tường mê cung
// ========================================================================
void Renderer::DrawMap(const GameMap& map) {
    for (b2Body* wall : map.GetWalls()) {
        b2PolygonShape* shape = (b2PolygonShape*)wall->GetFixtureList()->GetShape();
        b2Vec2 pos = wall->GetPosition();
        Rectangle rec = { pos.x * SCALE, SCREEN_HEIGHT - pos.y * SCALE, 
                          shape->m_vertices[1].x * 2 * SCALE, shape->m_vertices[2].y * 2 * SCALE };
        DrawRectanglePro(rec, { rec.width / 2.0f, rec.height / 2.0f }, wall->GetAngle() * RAD2DEG, GRAY);
    }
}

// ========================================================================
// Vẽ cặp cổng dịch chuyển A và B
// ========================================================================
void Renderer::DrawPortal(const Portal& portal) {
    if (!portal.isActive) return;
    float ax = portal.posA.x * SCALE, ay = SCREEN_HEIGHT - portal.posA.y * SCALE;
    float bx = portal.posB.x * SCALE, by = SCREEN_HEIGHT - portal.posB.y * SCALE;
    DrawCircle((int)ax, (int)ay, 20.0f, Fade(PURPLE, 0.5f)); 
    DrawCircleLines((int)ax, (int)ay, 22.0f, PURPLE); 
    DrawText("A", (int)ax - 6, (int)ay - 10, 20, WHITE);
    DrawCircle((int)bx, (int)by, 20.0f, Fade(BLUE, 0.5f)); 
    DrawCircleLines((int)bx, (int)by, 22.0f, BLUE); 
    DrawText("B", (int)bx - 6, (int)by - 10, 20, WHITE);
}

// ========================================================================
// Vẽ hộp vũ khí với biểu tượng tương ứng loại vũ khí
// ========================================================================
void Renderer::DrawItem(const Item& item) {
    b2Vec2 pos = item.body->GetPosition();
    float x = pos.x * SCALE;
    float y = SCREEN_HEIGHT - pos.y * SCALE;

    DrawRectangle((int)(x - 10), (int)(y - 10), 20, 20, DARKGRAY);
    DrawRectangleLines((int)(x - 10), (int)(y - 10), 20, 20, BLACK);

    if (item.type == ItemType::DEATH_RAY) {
        // Tia lượn sóng chữ S
        DrawLineEx({x - 4.0f, y - 6.0f}, {x + 4.0f, y - 2.0f}, 2.0f, BLACK);
        DrawLineEx({x + 4.0f, y - 2.0f}, {x - 4.0f, y + 2.0f}, 2.0f, BLACK);
        DrawLineEx({x - 4.0f, y + 2.0f}, {x + 4.0f, y + 6.0f}, 2.0f, BLACK);
    } else if (item.type == ItemType::GATLING) {
        // 5 chấm xúc xắc
        DrawCircle((int)(x - 4), (int)(y - 4), 1.5f, BLACK);
        DrawCircle((int)(x + 4), (int)(y - 4), 1.5f, BLACK);
        DrawCircle((int)x, (int)y, 1.5f, BLACK);
        DrawCircle((int)(x - 4), (int)(y + 4), 1.5f, BLACK);
        DrawCircle((int)(x + 4), (int)(y + 4), 1.5f, BLACK);
    } else if (item.type == ItemType::FRAG) {
        // Ngôi sao nổ
        for(int i = 0; i < 8; i++) {
            float angle = i * PI / 4.0f;
            DrawLineEx({x, y}, {x + cosf(angle)*7.0f, y + sinf(angle)*7.0f}, 2.0f, BLACK);
        }
    } else if (item.type == ItemType::MISSILE) {
        // Tên lửa mũi nhọn
        DrawTriangle({x - 5, y + 5}, {x + 5, y - 5}, {x, y + 7}, BLACK);
        DrawTriangle({x - 5, y + 5}, {x + 5, y - 5}, {x - 7, y}, BLACK);
    }
}
