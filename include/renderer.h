#pragma once
#include "game.h"

/**
 * @class Renderer
 * @brief Lớp tĩnh chịu trách nhiệm vẽ toàn bộ đồ họa game bằng Raylib.
 * Tách biệt hoàn toàn khỏi logic game → có thể tắt khi train RL.
 */
class Renderer {
public:
    /// Vẽ toàn bộ thế giới game (map, tanks, bullets, items, portals)
    static void DrawWorld(const Game& game);

    static void DrawTank(const Tank& tank);
    static void DrawBullet(const Bullet& bullet);
    static void DrawMap(const GameMap& map);
    static void DrawPortal(const Portal& portal);
    static void DrawItem(const Item& item);
};
