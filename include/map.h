#pragma once
#include "constants.h"

/**
 * @class GameMap
 * @brief Chịu trách nhiệm thiết kế, sinh tự động và hiển thị hệ thống Mê cung (Maze)
 */
class GameMap {
private:
    std::vector<b2Body*> walls; ///< Danh sách lưu trữ các khối tường tĩnh trong vật lý Box2D
    
public:
    /**
     * @brief Sinh ra một bộ bản đồ ngẫu nhiên mới bằng thuật toán Recursive Backtracker
     * Kết hợp thêm logic đập tường ngẫu nhiên để tạo các vòng lặp lối tắt
     */
    void Build(b2World& world);
    
    /**
     * @brief Vẽ từng bức tường tĩnh trong danh sách lên màn hình Raylib
     */
    void Draw();
    
    /**
     * @brief Sinh ngẫu nhiên một tọa độ ở trung tâm để spawn người chơi hoặc vật phẩm an toàn
     */
    b2Vec2 GetRandomCellCenter() const;
    
    /**
     * @brief Dọn dẹp, phá hủy toàn bộ các khối tường hiện tại trong Box2D và dọn bộ nhớ
     */
    void Clear(b2World& world);
};