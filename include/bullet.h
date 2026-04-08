#pragma once
#include "constants.h"

// Forward declaration để tránh lỗi circular dependency (do tank.h cũng include bullet.h)
class Tank;

/**
 * @class Bullet
 * @brief Lớp đại diện cho viên đạn trong trò chơi. Xử lý logic bay, đạn đuổi (missile) và hiển thị.
 */
class Bullet {
public:
    b2Body* body;           ///< Thân vật lý quản lý tọa độ và vận tốc của đạn
    float time;             ///< Thời gian tồn tại còn lại của viên đạn

    bool isLaser;           ///< Đánh dấu đạn laser (xuyên thấu, bay cực nhanh)
    bool isFrag;            ///< Đánh dấu đạn nổ chùm (vỡ thành nhiều mảnh nhỏ)
    bool isMissile;         ///< Đánh dấu tên lửa (tự tìm mục tiêu)
    bool explodeFrag;       ///< Cờ báo hiệu đạn frag cần phát nổ thành mảnh vỡ ở frame này
    int ownerPlayerIndex;   ///< Số thứ tự của người chơi đã bắn viên đạn

    /**
     * @brief Khởi tạo viên đạn tại một vị trí cụ thể với lực/vận tốc ban đầu
     */
    Bullet(b2World& world, b2Vec2 position, b2Vec2 velocity, bool _isLaser = false, bool _isFrag = false, bool _isMissile = false, int _owner = -1);
    
    /**
     * @brief Cập nhật trạng thái của đạn. Dành cho việc đếm ngược thời gian và điều hướng cho đạn đuổi (missile)
     */
    void Update(float dt, const std::vector<Tank*>& tanks);

    /**
     * @brief Kiểm tra xem viên đạn đã hết hạn hoặc phát nổ hay chưa
     */
    bool IsDead() const;

    /**
     * @brief Dựng hình đồ họa đồ họa của viên đạn lên màn hình
     */
    void Draw();
};