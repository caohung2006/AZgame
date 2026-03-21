#include "lib.h"

std::vector<b2Body*> walls;

// Hàm khởi tạo và xây dựng bản đồ (mê cung)
void BuildMap() {
    // Hàm lambda tiện ích để tạo nhanh một bức tường dựa trên toạ độ Pixel màn hình
    auto addWall = [](float x, float y, float width, float height) {
        b2BodyDef def;
        def.type = b2_staticBody; // Tường là vật thể tĩnh, không di chuyển và không bị ảnh hưởng bởi lực
        
        // Đảo trục Y (Vì Raylib dùng gốc toạ độ ở góc trên trái, còn Box2D dùng chuẩn toán học ở dưới trái)
        def.position.Set(x / SCALE, (SCREEN_HEIGHT - y) / SCALE); 

        b2PolygonShape shape;
        // Box2D nhận kích thước là một nửa chiều rộng (half-width) và một nửa chiều cao (half-height)
        shape.SetAsBox((width / 2.0f) / SCALE, (height / 2.0f) / SCALE); 

        b2FixtureDef fixture;
        fixture.shape = &shape;
        fixture.density = 0.0f; // Tường không cần khối lượng

        b2Body* body = world.CreateBody(&def);
        body->CreateFixture(&fixture);
        walls.push_back(body); // Lưu vào danh sách để sử dụng ở khâu Render
    };

    // --- THIẾT KẾ BẢN ĐỒ MÊ CUNG KIỂU AZ TANK ---
    // Dùng mảng chuỗi để vẽ sơ đồ mê cung. Khu vực chính giữa được để trống để làm vùng an toàn spawn xe tăng.
    // '+' là cột góc, '-' là tường ngang, '|' là tường dọc. Khoảng trắng là đường đi.
    std::vector<std::string> maze = {
        "+---+---+---+---+---+---+---+---+",
        "|               |               |",
        "+   +---+---+   +   +---+---+   +",
        "|   |       |       |       |   |",
        "+   +   +   +---+---+   +   +   +",
        "|       |               |       |",
        "+---+   +               +   +---+",
        "|       |               |       |",
        "+   +   +   +---+---+   +   +   +",
        "|   |       |       |       |   |",
        "+   +---+---+   +   +---+---+   +",
        "|               |               |",
        "+---+---+---+---+---+---+---+---+"
    };

    float cellW = 90.0f; // Thu nhỏ chiều rộng ô đường đi để tạo không gian hẹp như AZ Tank
    float cellH = 90.0f; // Thu nhỏ chiều cao ô đường đi
    float wallThickness = 6.0f; // Độ dày của bức tường

    // Tính toán độ lệch (offset) để canh giữa bản đồ trên màn hình
    int numCols = 8; // Sơ đồ chuỗi ở trên có 8 ô ngang
    int numRows = 6; // Sơ đồ chuỗi ở trên có 6 ô dọc
    float offsetX = (SCREEN_WIDTH - (numCols * cellW)) / 2.0f;
    float offsetY = (SCREEN_HEIGHT - (numRows * cellH)) / 2.0f - 50.0f; // Trừ đi 50px để dịch toàn bộ map lên trên

    // Quét qua từng ký tự trong sơ đồ để sinh ra bức tường tương ứng
    for (int row = 0; row < maze.size(); row++) {
        for (int col = 0; col < maze[row].length(); col++) {
            char c = maze[row][col];
            
            // Ký tự '-' là tường nằm ngang (Lấy % 4 == 2 để chỉ vẽ 1 đoạn duy nhất ở chính giữa ô)
            if (c == '-' && col % 4 == 2) {
                float cx = offsetX + (col / 4) * cellW + (cellW / 2.0f);
                float cy = offsetY + (row / 2) * cellH;
                addWall(cx, cy, cellW + wallThickness, wallThickness);
            }
            // Ký tự '|' là tường nằm dọc
            else if (c == '|') {
                float cx = offsetX + (col / 4) * cellW;
                float cy = offsetY + (row / 2) * cellH + (cellH / 2.0f);
                addWall(cx, cy, wallThickness, cellH + wallThickness);
            }
            // Cột trụ tại các góc giao nhau để nối khít các cạnh tường
            else if (c == '+') {
                float cx = offsetX + (col / 4) * cellW;
                float cy = offsetY + (row / 2) * cellH;
                addWall(cx, cy, wallThickness, wallThickness);
            }
        }
    }
}