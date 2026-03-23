#include "lib.h"

Portal teleportPortal = { b2Vec2(0.0f, 0.0f), b2Vec2(0.0f, 0.0f), false, 5.0f }; // Khởi tạo cổng dịch chuyển

void UpdatePortal(float dt) {
    if (!teleportPortal.isActive) {
        teleportPortal.cooldownTimer -= dt;
        if (teleportPortal.cooldownTimer <= 0.0f) {
            // Chọn ngẫu nhiên 2 ô trống trong lưới mê cung (8 cột x 6 hàng)
            int c1 = GetRandomValue(0, 7), r1 = GetRandomValue(0, 5);
            int c2 = GetRandomValue(0, 7), r2 = GetRandomValue(0, 5);
            while (c1 == c2 && r1 == r2) { // Đảm bảo 2 cổng không trùng vị trí
                c2 = GetRandomValue(0, 7);
                r2 = GetRandomValue(0, 5);
            }
            float cellW = 90.0f, cellH = 90.0f;
            float offsetX = (SCREEN_WIDTH - (8 * cellW)) / 2.0f;
            float offsetY = (SCREEN_HEIGHT - (6 * cellH)) / 2.0f - 50.0f;

            // Tính toạ độ Pixel chuẩn giữa ô và đổi sang hệ mét Box2D
            teleportPortal.posA.Set((offsetX + c1 * cellW + cellW / 2.0f) / SCALE, (SCREEN_HEIGHT - (offsetY + r1 * cellH + cellH / 2.0f)) / SCALE);
            teleportPortal.posB.Set((offsetX + c2 * cellW + cellW / 2.0f) / SCALE, (SCREEN_HEIGHT - (offsetY + r2 * cellH + cellH / 2.0f)) / SCALE);
            teleportPortal.isActive = true;
        }
    } else {
        float portalRadius = 25.0f / SCALE; // Bán kính kích hoạt cổng là 25 pixel
        bool portalUsed = false;

        // Dùng Lambda Expression để dùng chung logic xét va chạm cho xe tăng và đạn
        auto checkTeleport = [&](b2Body* b) {
            float distA = (b->GetPosition() - teleportPortal.posA).Length();
            float distB = (b->GetPosition() - teleportPortal.posB).Length();
            if (distA < portalRadius) {
                b->SetTransform(teleportPortal.posB, b->GetAngle()); // Đưa sang B
                portalUsed = true;
            } else if (distB < portalRadius) {
                b->SetTransform(teleportPortal.posA, b->GetAngle()); // Đưa sang A
                portalUsed = true;
            }
        };

        for (Tank* t : tanks) { checkTeleport(t->body); if (portalUsed) break; }
        if (!portalUsed) { for (Bullet* b : bullets) { checkTeleport(b->body); if (portalUsed) break; } }

        if (portalUsed) {
            teleportPortal.isActive = false;
            teleportPortal.cooldownTimer = (float)GetRandomValue(3, 10); // Đợi ngẫu nhiên 3 đến 10 giây để sinh cổng mới
        }
    }
}