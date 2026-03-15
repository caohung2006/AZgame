#include "lib.h"

b2World world(b2Vec2(0.0f, 0.0f));

void WorldAZ() {
    InitWindow(1024, 768, "Top Down WASD Movement");
    SetTargetFPS(90);

    map();         // Tạo tường vật lý
    tank.Init();   // Khởi tạo xe tăng vào thế giới

    while (!WindowShouldClose()) {

        tank.Move(); // Lắng nghe phím và di chuyển xe

        // world.Step(1.0f / 60.0f, 6, 2);
        // Sử dụng GetFrameTime() để đồng bộ thời gian vật lý với FPS thực tế
        world.Step(GetFrameTime(), 6, 2);

        // ===== RENDER =====
        Render();
      
    }
    CloseWindow();
}