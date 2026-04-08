#pragma once
#include "constants.h"

/**
 * @enum ItemType
 * @brief Định nghĩa các loại vũ khí và sức mạnh xuất hiện trong trò chơi
 */
enum class ItemType {
    NORMAL,     ///< Đạn bình thường. Xe tăng sẽ dùng loại này khi hét đạn vũ khí xịn.
    GATLING,    ///< Đạn shotgun/gatling: Bắn 5 viên chùm cùng một lúc nhưng bay ngắn.
    FRAG,       ///< Mìn nổ chùm / Frag: Đặt mìn và bấm lần nữa để kích nổ thành 8 mảnh vỡ.
    MISSILE,    ///< Đạn tên lửa đuổi (Homing Missile): Tự động luồn lách để rượt máy bay địch.
    DEATH_RAY   ///< Đạn Laser (Death Ray): Bắn tia laser tốc độ cực cao, kèm theo tia ngắm lade cảnh báo đỏ.
};

/**
 * @class Item
 * @brief Lớp quản lý vật phẩm (hộp vũ khí) rớt xuống sàn đấu
 */
class Item {
public:
    b2Body* body;           ///< Hệ thống vật lý Box2D quản lý vị trí hộp vũ khí (kiểu Sensor rỗng)
    ItemType type;          ///< Chủng loại của vũ khí được chứa trong hộp này
    bool isDestroyed;       ///< Cờ đánh dấu nếu hộp bị xe tăng cán qua và cần hủy bỏ đi

    /**
     * @brief Khởi tạo một hộp vũ khí ngẫu nhiên
     */
    Item(b2World& world, b2Vec2 position, ItemType _type);

    /**
     * @brief Dựng hình hộp vũ khí kèm biểu tượng phân biệt tùy chủng loại trên mặt sàn
     */
    void Draw();
};
