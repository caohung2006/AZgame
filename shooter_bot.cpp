#include "shooter_bot.h"
#include <queue>
#include <vector>
#include <string>

struct GridCell {
    int col;
    int row;
};

// Thuật toán BFS tìm ô tiếp theo tối ưu để di chuyển đến mục tiêu
static GridCell FindNextCell(GridCell start, GridCell target) {
    if (start.col == target.col && start.row == target.row) {
        return start;
    }

    static const std::vector<std::string> maze = {
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

    std::queue<GridCell> q;
    bool visited[8][6] = {false};
    GridCell parent[8][6];
    for (int c = 0; c < 8; ++c) {
        for (int r = 0; r < 6; ++r) {
            parent[c][r] = {-1, -1};
        }
    }

    q.push(start);
    visited[start.col][start.row] = true;
    bool found = false;

    while (!q.empty()) {
        GridCell curr = q.front();
        q.pop();

        if (curr.col == target.col && curr.row == target.row) {
            found = true;
            break;
        }

        std::vector<GridCell> neighbors;
        
        // Sang phải (cột + 1)
        if (curr.col < 7 && maze[curr.row * 2 + 1][curr.col * 4 + 4] != '|') {
            neighbors.push_back({curr.col + 1, curr.row});
        }
        // Sang trái (cột - 1)
        if (curr.col > 0 && maze[curr.row * 2 + 1][curr.col * 4] != '|') {
            neighbors.push_back({curr.col - 1, curr.row});
        }
        // Xuống dưới (dòng + 1)
        if (curr.row < 5 && maze[curr.row * 2 + 2][curr.col * 4 + 2] != '-') {
            neighbors.push_back({curr.col, curr.row + 1});
        }
        // Lên trên (dòng - 1)
        if (curr.row > 0 && maze[curr.row * 2][curr.col * 4 + 2] != '-') {
            neighbors.push_back({curr.col, curr.row - 1});
        }

        for (const GridCell& nextCell : neighbors) {
            if (!visited[nextCell.col][nextCell.row]) {
                visited[nextCell.col][nextCell.row] = true;
                parent[nextCell.col][nextCell.row] = curr;
                q.push(nextCell);
            }
        }
    }

    if (!found) {
        return start;
    }

    GridCell step = target;
    while (true) {
        GridCell p = parent[step.col][step.row];
        if (p.col == start.col && p.row == start.row) {
            return step;
        }
        step = p;
    }
}

// ============================================================================
// CONSTRUCTOR — Tạo body Box2D giống Tank, chọn vị trí tuần tra ban đầu
// ============================================================================
ShooterBot::ShooterBot(b2World& world, int _playerIndex) {
    playerIndex = _playerIndex;
    shootCooldownTimer = 0.0f;
    patrolTimer = 0.0f;
    patrolTarget.Set(0.0f, 0.0f);

    b2BodyDef tankDef;
    tankDef.type = b2_dynamicBody;
    tankDef.position.Set((SCREEN_WIDTH / 2.0f) / SCALE, (SCREEN_HEIGHT / 2.0f) / SCALE);
    tankDef.fixedRotation = false;
    body = world.CreateBody(&tankDef);

    // Thân xe (hull)
    b2PolygonShape hullShape;
    hullShape.SetAsBox(14.0f / SCALE, 14.0f / SCALE, b2Vec2(0.0f, -7.0f / SCALE), 0.0f);
    b2FixtureDef hullFix;
    hullFix.shape = &hullShape;
    hullFix.density = 1.0f;
    hullFix.friction = 0.0f;
    hullFix.restitution = 0.0f;
    body->CreateFixture(&hullFix);

    // Nòng súng (barrel)
    b2PolygonShape barrelShape;
    barrelShape.SetAsBox(3.0f / SCALE, 7.0f / SCALE, b2Vec2(0.0f, 14.0f / SCALE), 0.0f);
    b2FixtureDef barrelFix;
    barrelFix.shape = &barrelShape;
    barrelFix.density = 0.2f;
    barrelFix.friction = 0.0f;
    barrelFix.restitution = 0.0f;
    body->CreateFixture(&barrelFix);

    PickNewPatrolTarget();
}

// ============================================================================
// UPDATE — Ngắm bắn mục tiêu + tìm đường truy đuổi + bất tử
// ============================================================================
void ShooterBot::Update(b2World& world, std::vector<Bullet*>& bullets, b2Body* target) {
    float dt = GetFrameTime();
    b2Vec2 myPos = body->GetPosition();
    float currentAngle = body->GetAngle();
    b2Vec2 forwardDir(-sinf(currentAngle), cosf(currentAngle));

    float cellW = 90.0f, cellH = 90.0f;
    float offsetX = (SCREEN_WIDTH - (8 * cellW)) / 2.0f;
    float offsetY = (SCREEN_HEIGHT - (6 * cellH)) / 2.0f - 50.0f;

    auto getCell = [&](b2Vec2 pos) -> GridCell {
        float x = pos.x * SCALE;
        float y = SCREEN_HEIGHT - pos.y * SCALE;
        int col = (int)((x - offsetX) / cellW);
        int row = (int)((y - offsetY) / cellH);
        if (col < 0) col = 0; if (col > 7) col = 7;
        if (row < 0) row = 0; if (row > 5) row = 5;
        return {col, row};
    };

    // ─────────────────────────────────────────
    // TÌM ĐƯỜNG TRUY ĐUỔI KẺ THÙ (BFS PATHFINDING)
    // ─────────────────────────────────────────
    b2Vec2 moveTargetPos = myPos;
    bool hasMoveTarget = false;

    if (target) {
        b2Vec2 targetPos = target->GetPosition();
        GridCell startCell = getCell(myPos);
        GridCell targetCell = getCell(targetPos);

        if (startCell.col == targetCell.col && startCell.row == targetCell.row) {
            // Cùng ô: tiến trực tiếp tới vị trí kẻ thù
            moveTargetPos = targetPos;
        } else {
            // Khác ô: tìm ô tiếp theo bằng BFS
            GridCell nextCell = FindNextCell(startCell, targetCell);
            float tx = offsetX + nextCell.col * cellW + cellW / 2.0f;
            float ty = offsetY + nextCell.row * cellH + cellH / 2.0f;
            moveTargetPos.Set(tx / SCALE, (SCREEN_HEIGHT - ty) / SCALE);
        }
        hasMoveTarget = true;
    }

    // ─────────────────────────────────────────
    // XOAY (ĐƯỜNG PHI HOLONOMIC): Hướng về đích di chuyển hoặc ngắm bắn kẻ thù
    // ─────────────────────────────────────────
    float desiredAngle = currentAngle;
    bool shouldAimAtTarget = false;

    if (target) {
        GridCell startCell = getCell(myPos);
        GridCell targetCell = getCell(target->GetPosition());
        if (startCell.col == targetCell.col && startCell.row == targetCell.row) {
            shouldAimAtTarget = true;
        }
    }

    if (shouldAimAtTarget) {
        // Nhắm bắn trực tiếp vào mục tiêu có dự đoán đón đầu
        b2Vec2 targetPos = target->GetPosition();
        b2Vec2 targetVel = target->GetLinearVelocity();
        b2Vec2 toTarget = targetPos - myPos;
        float dist = toTarget.Length();
        if (dist > 0.1f) {
            float flightTime = dist / BULLET_SPEED;
            b2Vec2 predicted = targetPos + flightTime * targetVel;
            b2Vec2 toPredicted = predicted - myPos;
            desiredAngle = atan2f(-toPredicted.x, toPredicted.y);
        }
    } else if (hasMoveTarget) {
        // Nhắm hướng đi đến ô tiếp theo
        b2Vec2 toMoveTarget = moveTargetPos - myPos;
        if (toMoveTarget.Length() > 0.15f) {
            desiredAngle = atan2f(-toMoveTarget.x, toMoveTarget.y);
        }
    }

    float angleDiff = NormalizeAngle(desiredAngle - currentAngle);
    float angularVel = 0.0f;
    if (fabsf(angleDiff) > 0.05f) {
        angularVel = (angleDiff > 0) ? TURN_SPEED : -TURN_SPEED;
    }
    body->SetAngularVelocity(angularVel);

    // ─────────────────────────────────────────
    // DI CHUYỂN PHI HOLONOMIC (Chỉ tiến dọc theo trục thân xe/nòng súng)
    // ─────────────────────────────────────────
    b2Vec2 velocity(0.0f, 0.0f);
    if (hasMoveTarget) {
        b2Vec2 toMoveTarget = moveTargetPos - myPos;
        float distToMoveTarget = toMoveTarget.Length();
        if (distToMoveTarget > 0.15f) {
            // Chỉ di chuyển khi hướng xe đã tương đối thẳng với hướng cần đi
            float targetAngleDiff = NormalizeAngle(desiredAngle - currentAngle);
            if (fabsf(targetAngleDiff) < 1.0f) {
                velocity = MOVE_SPEED * forwardDir;
            }
        }
    }
    body->SetLinearVelocity(velocity);

    // ─────────────────────────────────────────
    // BẮN ĐẠN (Chỉ khi nòng súng hướng trúng mục tiêu)
    // ─────────────────────────────────────────
    if (target) {
        b2Vec2 targetPos = target->GetPosition();
        b2Vec2 toTarget = targetPos - myPos;
        float angleToTarget = atan2f(-toTarget.x, toTarget.y);
        float angleDiffToTarget = NormalizeAngle(angleToTarget - currentAngle);

        if (shootCooldownTimer > 0.0f) shootCooldownTimer -= dt;
        if (fabsf(angleDiffToTarget) < AIM_TOLERANCE && shootCooldownTimer <= 0.0f) {
            b2Vec2 spawnPos = myPos + (24.5f / SCALE) * forwardDir;

            // Raycast kiểm tra nòng súng không chui vào tường
            class WallRayCastCallback : public b2RayCastCallback {
            public:
                bool hitWall = false;
                float ReportFixture(b2Fixture* fixture, const b2Vec2& point,
                                    const b2Vec2& normal, float fraction) override {
                    if (fixture->GetBody()->GetType() == b2_staticBody) {
                        hitWall = true; return fraction;
                    }
                    return -1.0f;
                }
            };
            WallRayCastCallback callback;
            world.RayCast(&callback, myPos, spawnPos);

            if (!callback.hitWall) {
                bullets.push_back(new Bullet(world, spawnPos, BULLET_SPEED * forwardDir));
                shootCooldownTimer = SHOOT_COOLDOWN;
            }
        }
    }

    // ─────────────────────────────────────────
    // BẤT TỬ: hấp thụ đạn trúng nhưng KHÔNG chết
    // ─────────────────────────────────────────
    for (b2ContactEdge* edge = body->GetContactList(); edge; edge = edge->next) {
        if (edge->contact->IsTouching()) {
            b2Body* otherBody = edge->other;
            for (Bullet* bullet : bullets) {
                if (otherBody == bullet->body) {
                    bullet->time = 0.0f;  // Huỷ viên đạn
                    // KHÔNG set isDestroyed — shooter bất tử
                }
            }
        }
    }
}

// ============================================================================
// PATROL — Chọn ô mê cung ngẫu nhiên làm đích di chuyển
// ============================================================================
void ShooterBot::PickNewPatrolTarget() {
    float cellW = 90.0f, cellH = 90.0f;
    float offsetX = (SCREEN_WIDTH - (8 * cellW)) / 2.0f;
    float offsetY = (SCREEN_HEIGHT - (6 * cellH)) / 2.0f - 50.0f;
    int col = GetRandomValue(0, 7);
    int row = GetRandomValue(0, 5);
    float px = offsetX + col * cellW + cellW / 2.0f;
    float py = offsetY + row * cellH + cellH / 2.0f;
    patrolTarget.Set(px / SCALE, (SCREEN_HEIGHT - py) / SCALE);
    patrolTimer = (float)GetRandomValue(30, 60) / 10.0f;  // 3–6 giây
}

// ============================================================================
// ANGLE NORMALIZATION
// ============================================================================
float ShooterBot::NormalizeAngle(float angle) {
    while (angle > b2_pi) angle -= 2.0f * b2_pi;
    while (angle < -b2_pi) angle += 2.0f * b2_pi;
    return angle;
}

// ============================================================================
// DRAW — Vẽ shooter với màu TÍM + nhãn "SHOOTER" + dấu bất tử ∞
// ============================================================================
void ShooterBot::Draw() {
    b2Vec2 pos = body->GetPosition();
    float rot = -body->GetAngle() * RAD2DEG;
    float x = pos.x * SCALE, y = SCREEN_HEIGHT - pos.y * SCALE;

    Color hullColor = PURPLE;
    // Nòng súng
    DrawRectanglePro({ x, y, 6.0f, 14.0f }, { 3.0f, 21.0f }, rot, DARKGRAY);
    // Thân xe
    DrawRectanglePro({ x, y, 28.0f, 28.0f }, { 14.0f, 7.0f }, rot, hullColor);
    // Nhãn
    DrawText("SHOOTER", (int)x - 22, (int)y - 30, 10, PURPLE);
}
