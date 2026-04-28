#pragma once
#include "game.h"
#include "constants.h"
#include <vector>

/**
 * @class Bot
 * @brief Hệ thống AI Bot tích hợp thẳng vào game C++.
 * Cho phép người chơi đấu với Bot trong main.exe hoặc dùng để train RL.
 */
class Bot {
public:
    int level;
    int playerIndex;

    // Chuỗi waypoint mà bot đang đi theo (đi hết rồi mới tính lại A*)
    std::vector<b2Vec2> cachedPath;    // Danh sách các waypoint đã smooth
    int currentWaypointIdx = 0;        // Index waypoint đang đi tới
    int stuckCounter = 0;              // Đếm frame bị kẹt để tự tính lại đường
    
    // Cooldown: chỉ tính lại A* khi mục tiêu di chuyển đủ xa HOẶC hết cooldown
    int recalcCooldown = 0;            // Đếm frame cooldown (> 0 = chưa được tính lại)
    b2Vec2 lastEnemyPos = b2Vec2(0, 0); // Vị trí địch lần cuối tính A*
    
    // Khi kẹt: đánh dấu ô bị kẹt + lùi xe trước khi tìm đường mới
    std::vector<std::pair<int,int>> blockedCells; // Các ô bị kẹt (row, col) - A* sẽ phạt nặng
    int backupTimer = 0;               // Đếm frame lùi xe (> 0 = đang lùi)

    Bot(int level, int playerIndex);

    /**
     * @brief Tính toán và trả về hành động của Bot trong frame hiện tại.
     * @param game Con trỏ đến trạng thái game hiện tại.
     * @return Hành động (phím bấm giả lập) của Bot.
     */
    TankActions GetAction(Game* game);
};
