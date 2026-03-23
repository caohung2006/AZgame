#include "lib.h"

b2World world(b2Vec2(0.0f, 0.0f));
vector<Tank*> tanks; // Quản lý danh sách người chơi
int playerScores[4] = {0, 0, 0, 0}; // Khởi tạo điểm số ban đầu là 0
int numPlayersGlobal = 1;

void RunGame() {
    InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "AZ Game");
    SetTargetFPS(60);

    // Lưu trữ cấu hình phím để dùng lại khi khởi tạo ván mới
    struct PlayerConfig { int fw, bw, tl, tr, sh; };
    vector<PlayerConfig> configs(4); // Hỗ trợ tối đa 4 người chơi
    
    // Thiết lập cấu hình phím mặc định cho 2 người chơi
    configs[0] = {KEY_W, KEY_S, KEY_A, KEY_D, KEY_Q};
    configs[1] = {KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT, KEY_SLASH}; // Phím slash (/)
    configs[2] = {KEY_I, KEY_K, KEY_J, KEY_L, KEY_U};
    configs[3] = {KEY_KP_8, KEY_KP_5, KEY_KP_4, KEY_KP_6, KEY_KP_7};

    int numPlayers = 2; // Mặc định là 2 người chơi
    numPlayersGlobal = numPlayers; // Lưu vào biến toàn cục để dùng khi vẽ đồ hoạ

    bool needsRestart = true; // Cờ báo hiệu cần tạo map mới

    while (!WindowShouldClose()) {
        // Xử lý khi nhấn vào nút Settings ở góc phải màn hình
        if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
            Vector2 mousePos = GetMousePosition();
            if (mousePos.x >= SCREEN_WIDTH - 120 && mousePos.x <= SCREEN_WIDTH - 10 &&
                mousePos.y >= 10 && mousePos.y <= 40) {
                
                numPlayers = ShowPlayerCountScreen();
                numPlayersGlobal = numPlayers;
                for (int i = 0; i < numPlayers; i++) {
                    ShowKeyBindingScreen(configs[i].fw, configs[i].bw, configs[i].tl, configs[i].tr, configs[i].sh, i + 1);
                }
                
                // Reset điểm số khi bắt đầu cài đặt lại
                for (int i = 0; i < 4; i++) playerScores[i] = 0;
                needsRestart = true;
            }
        }

        if (needsRestart) {
            // Dọn dẹp ván cũ (Xoá tường, xe tăng, đạn cả trong Box2D lẫn bộ nhớ C++)
            for (b2Body* wall : walls) world.DestroyBody(wall);
            walls.clear();

            for (Tank* t : tanks) { world.DestroyBody(t->body); delete t; }
            tanks.clear();

            for (Bullet* b : bullets) { world.DestroyBody(b->body); delete b; }
            bullets.clear();

            // Tạo map mới và sinh lại xe tăng
            BuildMap();
            for (int i = 0; i < numPlayers; i++) {
                Tank* t = new Tank();
                t->Init(i, configs[i].fw, configs[i].bw, configs[i].tl, configs[i].tr, configs[i].sh);
                
                b2Vec2 pos = t->body->GetPosition();
                t->body->SetTransform(b2Vec2(pos.x - 2.0f + (i * 2.0f), pos.y), 0.0f);
                tanks.push_back(t);
            }
            teleportPortal.isActive = false;
            teleportPortal.cooldownTimer = 5.0f; // Reset lại cổng mỗi khi tạo ván mới
            needsRestart = false;
        }

        float dt = GetFrameTime();

        // Cập nhật di chuyển và kiểm tra trạng thái sống/chết của từng xe tăng
        for (auto it = tanks.begin(); it != tanks.end(); ) {
            Tank* t = *it;
            t->Move(); 

            if (t->isDestroyed) {
                world.DestroyBody(t->body); // Xoá vật lý của xe tăng khỏi Box2D
                delete t;                   // Giải phóng bộ nhớ RAM
                it = tanks.erase(it);       // Xoá khỏi danh sách và trỏ đến xe tiếp theo
            } else {
                ++it; // Nếu xe chưa bị tiêu diệt thì đi tới kiểm tra xe tiếp theo
            }
        }

        // Kiểm tra điều kiện kết thúc ván
        // Nếu chơi 1 người (numPlayers == 1), ván kết thúc khi xe bị tiêu diệt (tanks.size() == 0)
        // Nếu chơi nhiều người, ván kết thúc khi chỉ còn lại 1 xe sống sót (tanks.size() <= 1)
        if (!needsRestart) {
            if ((numPlayers > 1 && tanks.size() <= 1) || (numPlayers == 1 && tanks.size() == 0)) {
                if (numPlayers > 1 && tanks.size() == 1) {
                    playerScores[tanks[0]->playerIndex]++; // Cộng điểm cho người sống sót cuối cùng
                }
                needsRestart = true;
            }
        }

        // --- XỬ LÝ CỔNG DỊCH CHUYỂN (PORTAL) ---
        if (!needsRestart) {
            UpdatePortal(dt);
        }

        // --- TÍNH CHẤT VẬT LÝ KHÓ: HỐ ĐEN HÚT ĐẠN ---
        // Làm quỹ đạo đạn bị bẻ cong, vô hiệu hoá các thuật toán ngắm bắn đường thẳng
        // b2Vec2 blackHolePos((SCREEN_WIDTH / 2.0f) / SCALE, (SCREEN_HEIGHT / 2.0f) / SCALE);
        // for (Bullet* b : bullets) {
        //     b2Vec2 bulletPos = b->body->GetPosition();
        //     b2Vec2 direction = blackHolePos - bulletPos;
        //     float distance = direction.Length();
        // if (distance > 0.5f && distance < 2.0f) { // Thu hẹp phạm vi ảnh hưởng xuống bán kính rất nhỏ (khoảng 60 pixel)
        //         direction.Normalize();
        //     float forceMagnitude = 1.0f / distance; // Lực hút cực nhỏ, bẻ lệch hướng nhẹ nhàng hơn nữa
        //         b2Vec2 gravityForce = forceMagnitude * direction;
        //         b->body->ApplyForceToCenter(gravityForce, true);
        //     }
        // }

        // Sử dụng GetFrameTime() để đồng bộ thời gian vật lý với FPS thực tế
        world.Step(dt, 6, 2);
        
        // Cập nhật thời gian sống của đạn và dọn dẹp nếu đạn hết hạn
        Lifetime(dt);

        // Vẽ các thành phần ra màn hình
        DrawGame();
      
    }
    CloseWindow();
}