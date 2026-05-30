#pragma once
#include "game.h"
#include "constants.h"
#include <vector>

class Bot {
public:
    int level;
    int playerIndex;

    // Pathfinding
    std::vector<b2Vec2> cachedPath;
    int currentWaypointIdx = 0;
    int stuckCounter = 0;
    b2Vec2 lastGoalPos = b2Vec2(0, 0);
    b2Vec2 lastEnemyPos = b2Vec2(0, 0);
    std::vector<std::pair<int,int>> blockedCells;
    int backupTimer = 0;
    int backupTurnDir = 1;
    int pathRecalcCD = 0;

    // Combat
    int idleTimer = 0;
    int strafeDir = 1;
    int strafeTimer = 0;
    int bounceSearchCD = 0;
    float cachedBounceAngle = 0;
    bool hasBounceShot = false;
    int shotMode = 0;         // 0=direct, 1=bounce (quyết định 1 lần/viên đạn)
    int shotModeTimer = 0;    // Đếm ngược giữ mode ổn định

    // Dodge
    int dodgeLockTimer = 0;
    int dodgeLockDir = 1;

    Bot(int level, int playerIndex);
    TankActions GetAction(Game* game);
};
