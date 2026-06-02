#pragma once
#include "constants.h"

/**
 * @class GameMap
 * @brief Sinh và quản lý hệ thống Mê cung. Logic thuần, không đồ họa.
 * Rendering do Renderer đảm nhận thông qua GetWalls().
 */
class GameMap {
private:
    std::vector<b2Body*> walls; ///< Danh sách các khối tường tĩnh Box2D
    
public:
    /// Dữ liệu tường dạng hình chữ nhật (tọa độ pixel, tâm, chiều rộng/cao)
    struct WallRect {
        float x;
        float y;
        float width;
        float height;
    };

    /// Sinh bản đồ ngẫu nhiên bằng Recursive Backtracker + đục tường tạo shortcut
    void Build(b2World& world);

    /// Dựng bản đồ từ danh sách hình chữ nhật tường (tọa độ pixel)
    void BuildFromRects(b2World& world, const std::vector<WallRect>& rects);
    
    /// Phá hủy toàn bộ tường hiện tại
    void Clear(b2World& world);
    
    /// Sinh tọa độ ô ngẫu nhiên để spawn
    b2Vec2 GetRandomCellCenter() const;
    
    /// Trả về danh sách tường cho Renderer vẽ
    const std::vector<b2Body*>& GetWalls() const { return walls; }
};