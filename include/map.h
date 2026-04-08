#pragma once
#include "constants.h"

// Lớp phụ trách quản lý kết cấu màn chơi / mê cung
class GameMap {
private:
    std::vector<b2Body*> walls; // Danh sách lưu trữ các khối tường tĩnh trong vật lý Box2D
    
public:
    // Phân tích sơ đồ chuỗi (string) và khởi tạo các viên gạch/tường b2_staticBody
    void Build(b2World& world);
    
    // Võ các bức tường lên màn hình
    void Draw();
    
    // Sinh ngẫu nhiên một tọa độ ở trung tâm một lối đi (cell) trong mê cung
    b2Vec2 GetRandomCellCenter() const;
    
    // Dọn dẹp, phá hủy toàn bộ các khối tường hiện tại trong Box2D và dọn bộ nhớ
    void Clear(b2World& world);
};