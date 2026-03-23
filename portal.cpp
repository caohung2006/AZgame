#include "portal.h"

Portal::Portal() : posA(0.0f, 0.0f), posB(0.0f, 0.0f), isActive(false), cooldownTimer(5.0f) {}

void Portal::Update(float dt, std::vector<Tank*>& tanks, std::vector<Bullet*>& bullets) {
    if (!isActive) {
        cooldownTimer -= dt;
        if (cooldownTimer <= 0.0f) {
            int c1, r1, c2, r2;
            do { c1 = GetRandomValue(0,7); r1 = GetRandomValue(0,5); c2 = GetRandomValue(0,7); r2 = GetRandomValue(0,5); } while (c1==c2 && r1==r2);
            float cellW = 90.0f, cellH = 90.0f, offsetX = (SCREEN_WIDTH - 8*cellW)/2.0f, offsetY = (SCREEN_HEIGHT - 6*cellH)/2.0f - 50.0f;
            posA.Set((offsetX + c1*cellW + cellW/2.0f)/SCALE, (SCREEN_HEIGHT - (offsetY + r1*cellH + cellH/2.0f))/SCALE);
            posB.Set((offsetX + c2*cellW + cellW/2.0f)/SCALE, (SCREEN_HEIGHT - (offsetY + r2*cellH + cellH/2.0f))/SCALE);
            isActive = true;
        }
    } else {
        bool used = false;
        auto checkTeleport = [&](b2Body* b) {
            if ((b->GetPosition() - posA).Length() < 25.0f/SCALE) { b->SetTransform(posB, b->GetAngle()); used = true; }
            else if ((b->GetPosition() - posB).Length() < 25.0f/SCALE) { b->SetTransform(posA, b->GetAngle()); used = true; }
        };
        for (Tank* t : tanks) { checkTeleport(t->body); if (used) break; }
        if (!used) { for (Bullet* b : bullets) { checkTeleport(b->body); if (used) break; } }
        if (used) { isActive = false; cooldownTimer = (float)GetRandomValue(3, 10); }
    }
}

void Portal::Draw() {
    if (!isActive) return;
    float ax = posA.x * SCALE, ay = SCREEN_HEIGHT - posA.y * SCALE;
    float bx = posB.x * SCALE, by = SCREEN_HEIGHT - posB.y * SCALE;
    DrawCircle((int)ax, (int)ay, 20.0f, Fade(PURPLE, 0.5f)); DrawCircleLines((int)ax, (int)ay, 22.0f, PURPLE); DrawText("A", (int)ax-6, (int)ay-10, 20, WHITE);
    DrawCircle((int)bx, (int)by, 20.0f, Fade(BLUE, 0.5f)); DrawCircleLines((int)bx, (int)by, 22.0f, BLUE); DrawText("B", (int)bx-6, (int)by-10, 20, WHITE);
}

void Portal::Reset() {
    isActive = false;
    cooldownTimer = 5.0f;
}