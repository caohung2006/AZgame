#pragma once
#include "constants.h"
#include <vector>
#include <queue>
#include <utility>
#include <cmath>
#include <algorithm>

enum class MapMode {
    OPEN   = 0,  ///< No interior walls — flat arena for Phase 1
    SPARSE = 1,  ///< Few interior walls (many shortcuts) — Phase 2
    NORMAL = 2   ///< Full maze — Phases 3-5 and the actual game
};

/**
 * @class GameMap
 * @brief Sinh và quản lý hệ thống Mê cung + A* Pathfinding.
 */
class GameMap {
public:
    // ── Grid constants ──────────────────────────────────────────────────────
    static constexpr int   ROWS   = 6;
    static constexpr int   COLS   = 8;
    static constexpr float CELL_W = 90.0f;  // pixels per cell
    static constexpr float CELL_H = 90.0f;

    // ── Wall grid data (saved during Build() for A* pathfinding) ─────────
    bool hWalls[ROWS + 1][COLS] = {};   // Horizontal wall segments
    bool vWalls[ROWS][COLS + 1] = {};   // Vertical wall segments
    MapMode currentMode = MapMode::OPEN;

    // ── Core API ─────────────────────────────────────────────────────────
    void Build(b2World& world, MapMode mode = MapMode::NORMAL);
    void Clear(b2World& world);
    b2Vec2 GetRandomCellCenter() const;
    const std::vector<b2Body*>& GetWalls() const { return walls; }

    // ── A* Pathfinding ───────────────────────────────────────────────────
    /// Convert Box2D world position to grid cell (row, col)
    std::pair<int,int> WorldToCell(b2Vec2 worldPos) const;

    /// Convert grid cell to Box2D world position (cell center)
    b2Vec2 CellToWorld(int row, int col) const;

    /// Check if movement between adjacent cells is allowed (no wall)
    bool CanMove(int fromR, int fromC, int toR, int toC) const;

    /// Get Box2D position of the next A* waypoint from agent toward enemy.
    /// In OPEN mode, returns enemyPos directly (no maze to navigate).
    b2Vec2 GetNextWaypoint(b2Vec2 agentPos, b2Vec2 enemyPos) const;

private:
    std::vector<b2Body*> walls;

    float OffsetX() const { return (SCREEN_WIDTH  - COLS * CELL_W) / 2.0f; }
    float OffsetY() const { return (SCREEN_HEIGHT - ROWS * CELL_H) / 2.0f - 50.0f; }
};