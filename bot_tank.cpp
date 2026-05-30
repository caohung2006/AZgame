#include "bot_tank.h"
#include <float.h>

// ============================================================================
// DỰ BÁO VỊ TRÍ ĐẠN VỚI NHIỀU LẦN NẢY TƯỜNG (lên tới 3 lần)
// ============================================================================
static b2Vec2 GetPredictedBulletPos(b2World& world, b2Vec2 bulletPos, b2Vec2 bulletVel, float t) {
    float speed = bulletVel.Length();
    if (speed < 0.1f) return bulletPos;

    class BounceRayCastCallback : public b2RayCastCallback {
    public:
        bool hit = false;
        b2Vec2 point;
        b2Vec2 normal;
        float ReportFixture(b2Fixture* fixture, const b2Vec2& pt,
                            const b2Vec2& norm, float fraction) override {
            if (fixture->GetBody()->GetType() == b2_staticBody) {
                hit = true;
                point = pt;
                normal = norm;
                return fraction;
            }
            return -1.0f;
        }
    };

    // Mô phỏng tối đa 3 lần nảy tường
    b2Vec2 currentPos = bulletPos;
    b2Vec2 currentVel = bulletVel;
    float remainingTime = t;

    for (int bounce = 0; bounce < 3 && remainingTime > 0.001f; ++bounce) {
        BounceRayCastCallback cb;
        b2Vec2 end = currentPos + (remainingTime * 1.1f + 0.5f) * currentVel;
        world.RayCast(&cb, currentPos, end);

        if (cb.hit) {
            float distToWall = (cb.point - currentPos).Length();
            float currentSpeed = currentVel.Length();
            if (currentSpeed < 0.01f) break;
            float timeToWall = distToWall / currentSpeed;

            if (remainingTime <= timeToWall) {
                // Đạn đến đích trước khi chạm tường
                return currentPos + remainingTime * currentVel;
            } else {
                // Đạn chạm tường → phản xạ
                remainingTime -= timeToWall;
                currentPos = cb.point;
                currentVel = currentVel - 2.0f * b2Dot(currentVel, cb.normal) * cb.normal;
                // Đẩy ra xa tường một chút để tránh raycast bị kẹt
                currentPos += 0.01f * cb.normal;
            }
        } else {
            // Không chạm tường → bay thẳng
            return currentPos + remainingTime * currentVel;
        }
    }

    // Sau 3 lần nảy, bay thẳng với thời gian còn lại
    return currentPos + remainingTime * currentVel;
}


// ============================================================================
// CONSTRUCTOR — Tạo body Box2D giống hệt Tank (cùng hull + barrel shape)
// ============================================================================
BotTank::BotTank(b2World& world, int _playerIndex) {
    playerIndex = _playerIndex;
    shootCooldownTimer = 0.0f;
    isDestroyed = false;

    b2BodyDef tankDef;
    tankDef.type = b2_dynamicBody;
    tankDef.position.Set((SCREEN_WIDTH / 2.0f) / SCALE, (SCREEN_HEIGHT / 2.0f) / SCALE);
    tankDef.fixedRotation = false;
    body = world.CreateBody(&tankDef);

    // Thân xe (hull) — hình chữ nhật 28x28 pixel
    b2PolygonShape hullShape;
    hullShape.SetAsBox(14.0f / SCALE, 14.0f / SCALE, b2Vec2(0.0f, -7.0f / SCALE), 0.0f);
    b2FixtureDef hullFix;
    hullFix.shape = &hullShape;
    hullFix.density = 1.0f;
    hullFix.friction = 0.0f;
    hullFix.restitution = 0.0f;
    body->CreateFixture(&hullFix);

    // Nòng súng (barrel) — hình chữ nhật 6x14 pixel
    b2PolygonShape barrelShape;
    barrelShape.SetAsBox(3.0f / SCALE, 7.0f / SCALE, b2Vec2(0.0f, 14.0f / SCALE), 0.0f);
    b2FixtureDef barrelFix;
    barrelFix.shape = &barrelShape;
    barrelFix.density = 0.2f;
    barrelFix.friction = 0.0f;
    barrelFix.restitution = 0.0f;
    body->CreateFixture(&barrelFix);
}

// ============================================================================
// MAIN AI UPDATE — Gọi mỗi khung hình, quyết định hành vi của bot
// ============================================================================
void BotTank::Update(b2World& world, std::vector<Bullet*>& bullets, std::vector<Tank*>& enemies) {
    float dt = GetFrameTime();
    b2Vec2 botPos = body->GetPosition();
    float currentAngle = body->GetAngle();
    b2Vec2 forwardDir(-sinf(currentAngle), cosf(currentAngle));

    // ─────────────────────────────────────────
    // PHASE 1: ĐÁNH GIÁ MỐI ĐE DOẠ (Né đạn)
    // ─────────────────────────────────────────
    b2Vec2 dodgeDir = EvaluateThreats(world, bullets);
    bool needsDodge = (dodgeDir.LengthSquared() > 0.01f);

    // ─────────────────────────────────────────
    // PHASE 2: CHỌN MỤC TIÊU & DỰ ĐOÁN VỊ TRÍ
    // ─────────────────────────────────────────
    Tank* target = SelectTarget(enemies);
    float desiredAngle = currentAngle;

    if (target) {
        b2Vec2 targetPos = target->body->GetPosition();
        b2Vec2 targetVel = target->body->GetLinearVelocity();
        b2Vec2 toTarget = targetPos - botPos;
        float dist = toTarget.Length();

        // Dự đoán vị trí: ngắm vào nơi địch SẼ ở khi đạn bay đến
        if (dist > 0.1f) {
            float flightTime = dist / BULLET_SPEED;
            b2Vec2 predictedPos = targetPos + flightTime * targetVel;
            b2Vec2 toPredicted = predictedPos - botPos;
            desiredAngle = atan2f(-toPredicted.x, toPredicted.y);
        }
    }

    // ─────────────────────────────────────────
    // PHASE 3: XOAY (luôn cố gắng hướng về mục tiêu khi không né đạn)
    // ─────────────────────────────────────────
    float angleDiff = NormalizeAngle(desiredAngle - currentAngle);

    if (!needsDodge) {
        if (fabsf(angleDiff) > 0.03f) {
            body->SetAngularVelocity(angleDiff > 0 ? TURN_SPEED : -TURN_SPEED);
        } else {
            body->SetAngularVelocity(0.0f);
        }
    }

    // ─────────────────────────────────────────
    // PHASE 4: DI CHUYỂN (Né đạn bằng mô phỏng phi holonomic > Chạy bình thường)
    // ─────────────────────────────────────────
    b2Vec2 velocity(0.0f, 0.0f);

    if (needsDodge) {
        // TÌM HÀNH ĐỘNG NÉ TỐI ƯU — 25 TỔ HỢP ỨNG CỬ VIÊN
        b2Vec2 enemyPos(SCREEN_WIDTH / 2.0f / SCALE, SCREEN_HEIGHT / 2.0f / SCALE);
        Tank* targetEnemy = SelectTarget(enemies);
        if (targetEnemy) {
            enemyPos = targetEnemy->body->GetPosition();
        } else {
            // Thử tìm bất kỳ xe tăng/đối thủ động nào trong thế giới
            for (b2Body* b = world.GetBodyList(); b; b = b->GetNext()) {
                if (b->GetType() == b2_dynamicBody && b != body) {
                    enemyPos = b->GetPosition();
                    break;
                }
            }
        }

        float bestOmega = 0.0f;
        float bestLinear = 0.0f;
        float minCost = FLT_MAX;
        
        // 5 mức xoay × 5 mức tốc độ = 25 ứng cử viên (thay vì 9)
        float omegas[] = {-TURN_SPEED, -TURN_SPEED * 0.5f, 0.0f, TURN_SPEED * 0.5f, TURN_SPEED};
        float linears[] = {DODGE_SPEED, DODGE_SPEED * 0.5f, 0.0f, -0.5f * DODGE_SPEED, -DODGE_SPEED};

        for (float o : omegas) {
            for (float l : linears) {
                float cost = EvaluateDodgeCandidate(world, botPos, currentAngle, o, l, bullets, enemyPos);
                if (cost < minCost) {
                    minCost = cost;
                    bestOmega = o;
                    bestLinear = l;
                }
            }
        }

        body->SetAngularVelocity(bestOmega);
        velocity = bestLinear * forwardDir;
    } else if (target) {
        b2Vec2 toTarget = target->body->GetPosition() - botPos;
        float dist = toTarget.Length();

        if (dist > PURSUIT_DIST) {
            velocity = MOVE_SPEED * forwardDir;
        } else if (dist < RETREAT_DIST) {
            velocity = -0.7f * MOVE_SPEED * forwardDir;
        }
    }

    // Kiểm tra tường khi di chuyển bình thường (di chuyển né đã tự phạt né tường)
    if (!needsDodge && velocity.LengthSquared() > 0.01f) {
        b2Vec2 moveDir = velocity;
        moveDir.Normalize();
        if (IsWallInDirection(world, botPos, moveDir, 0.8f)) {
            velocity.Set(0.0f, 0.0f);
        }
    }

    body->SetLinearVelocity(velocity);

    // ─────────────────────────────────────────
    // PHASE 5: BẮN ĐẠN
    // ─────────────────────────────────────────
    if (shootCooldownTimer > 0.0f) shootCooldownTimer -= dt;

    if (target && shootCooldownTimer <= 0.0f) {
        if (fabsf(angleDiff) < AIM_TOLERANCE) {
            b2Vec2 spawnPos = botPos + (24.5f / SCALE) * forwardDir;

            // Raycast kiểm tra nòng súng không chui vào tường
            class WallRayCastCallback : public b2RayCastCallback {
            public:
                bool hitWall = false;
                float ReportFixture(b2Fixture* fixture, const b2Vec2& point,
                                    const b2Vec2& normal, float fraction) override {
                    if (fixture->GetBody()->GetType() == b2_staticBody) {
                        hitWall = true;
                        return fraction;
                    }
                    return -1.0f;
                }
            };
            WallRayCastCallback callback;
            world.RayCast(&callback, botPos, spawnPos);

            if (!callback.hitWall) {
                bullets.push_back(new Bullet(world, spawnPos, BULLET_SPEED * forwardDir));
                shootCooldownTimer = SHOOT_COOLDOWN;
            }
        }
    }

    // ─────────────────────────────────────────
    // PHASE 6: PHÁT HIỆN VA CHẠM (bị đạn trúng → chết)
    // ─────────────────────────────────────────
    for (b2ContactEdge* edge = body->GetContactList(); edge; edge = edge->next) {
        if (edge->contact->IsTouching()) {
            b2Body* otherBody = edge->other;
            for (Bullet* bullet : bullets) {
                if (otherBody == bullet->body) {
                    bullet->time = 0.0f;
                    isDestroyed = true;
                }
            }
        }
    }
}

// ============================================================================
// THREAT EVALUATION — Quét đạn, tính hướng né tổng hợp (vuông góc + ra xa)
// ============================================================================
b2Vec2 BotTank::EvaluateThreats(b2World& world, std::vector<Bullet*>& bullets) {
    b2Vec2 botPos = body->GetPosition();
    b2Vec2 combinedDodge(0.0f, 0.0f);

    // Quét thời gian mịn hơn, phản ứng sớm hơn (từ 0.08s)
    float timeSteps[] = {0.08f, 0.16f, 0.25f, 0.35f, 0.5f, 0.7f, 0.9f, 1.2f, 1.5f};
    int numSteps = sizeof(timeSteps) / sizeof(timeSteps[0]);

    for (Bullet* b : bullets) {
        b2Vec2 bulletPos = b->body->GetPosition();
        b2Vec2 bulletVel = b->body->GetLinearVelocity();
        float speed = bulletVel.Length();
        if (speed < 0.1f) continue;

        // === LỌC ĐẠN CỦA CHÍNH MÌNH ===
        // Đạn mới bắn ra (time > 9.5 tức < 0.5s tuổi) và đang bay xa khỏi bot → bỏ qua
        b2Vec2 bulletToBot = botPos - bulletPos;
        float dotProduct = b2Dot(bulletVel, bulletToBot);
        if (b->time > 9.5f && dotProduct < 0.0f) {
            // Đạn đang bay RA XA bot và rất mới → chắc chắn do mình bắn
            continue;
        }

        for (int i = 0; i < numSteps; ++i) {
            float t = timeSteps[i];
            b2Vec2 projectedBulletPos = GetPredictedBulletPos(world, bulletPos, bulletVel, t);
            float dist = (botPos - projectedBulletPos).Length();

            if (dist < DANGER_RADIUS) {
                float urgency = (DANGER_RADIUS - dist) / (t + 0.05f);

                // === NÉ VUÔNG GÓC + RA XA ===
                b2Vec2 awayDir = botPos - projectedBulletPos;
                if (awayDir.LengthSquared() > 0.001f) {
                    awayDir.Normalize();
                } else {
                    awayDir.Set(-bulletVel.y / speed, bulletVel.x / speed);
                }

                // Thành phần vuông góc với quỹ đạo đạn — hiệu quả hơn khi né
                b2Vec2 perpDir(-bulletVel.y / speed, bulletVel.x / speed);
                float side = b2Dot(botPos - projectedBulletPos, perpDir);
                if (side < 0) perpDir = -1.0f * perpDir; // Chọn bên gần hơn

                // Tổng hợp: 40% ra xa + 60% vuông góc
                combinedDodge += urgency * (0.4f * awayDir + 0.6f * perpDir);
            }
        }
    }

    if (combinedDodge.LengthSquared() > 0.01f) {
        combinedDodge.Normalize();
    }

    return combinedDodge;
}

// ============================================================================
// TARGET SELECTION — Tìm xe tăng địch gần nhất còn sống
// ============================================================================
Tank* BotTank::SelectTarget(std::vector<Tank*>& enemies) {
    Tank* nearest = nullptr;
    float minDist2 = FLT_MAX;
    b2Vec2 botPos = body->GetPosition();

    for (Tank* t : enemies) {
        if (t->isDestroyed) continue;
        float d2 = (t->body->GetPosition() - botPos).LengthSquared();
        if (d2 < minDist2) {
            minDist2 = d2;
            nearest = t;
        }
    }
    return nearest;
}

// ============================================================================
// WALL DETECTION — Raycast kiểm tra tường tĩnh trong một hướng
// ============================================================================
bool BotTank::IsWallInDirection(b2World& world, b2Vec2 from, b2Vec2 dir, float dist) {
    class WallCallback : public b2RayCastCallback {
    public:
        bool hit = false;
        float ReportFixture(b2Fixture* fixture, const b2Vec2& point,
                            const b2Vec2& normal, float fraction) override {
            if (fixture->GetBody()->GetType() == b2_staticBody) {
                hit = true;
                return fraction;
            }
            return -1.0f;
        }
    };

    WallCallback cb;
    b2Vec2 end = from + dist * dir;
    world.RayCast(&cb, from, end);
    return cb.hit;
}

// ============================================================================
// ANGLE NORMALIZATION — Đưa góc về khoảng [-π, π]
// ============================================================================
float BotTank::NormalizeAngle(float angle) {
    while (angle > b2_pi) angle -= 2.0f * b2_pi;
    while (angle < -b2_pi) angle += 2.0f * b2_pi;
    return angle;
}

// ============================================================================
// DRAW — Vẽ bot với màu ĐỎ đặc trưng và nhãn "BOT"
// ============================================================================
void BotTank::Draw() {
    b2Vec2 pos = body->GetPosition();
    float rot = -body->GetAngle() * RAD2DEG;
    float x = pos.x * SCALE, y = SCREEN_HEIGHT - pos.y * SCALE;

    Color hullColor = RED;
    // Nòng súng (barrel)
    DrawRectanglePro({ x, y, 6.0f, 14.0f }, { 3.0f, 21.0f }, rot, DARKGRAY);
    // Thân xe (hull)
    DrawRectanglePro({ x, y, 28.0f, 28.0f }, { 14.0f, 7.0f }, rot, hullColor);
    // Nhãn "BOT" phía trên xe
    DrawText("BOT", (int)x - 10, (int)y - 30, 10, RED);
}

// ============================================================================
// EVALUATE DODGE CANDIDATE — Mô phỏng quỹ đạo né đạn (phi holonomic)
// Quét 8 hướng tường, penalty góc chết mạnh, bước mô phỏng mịn hơn
// ============================================================================
float BotTank::EvaluateDodgeCandidate(b2World& world, b2Vec2 botPos, float currentAngle, float omega, float vLinear, const std::vector<Bullet*>& bullets, b2Vec2 enemyPos) {
    float cost = 0.0f;

    float simAngle = currentAngle;
    b2Vec2 simPos = botPos;

    for (int step = 1; step <= DODGE_SIM_STEPS; ++step) {
        float t = (float)step * DODGE_SIM_DT;
        
        // Giả lập di chuyển phi holonomic theo thời gian thực
        simAngle += DODGE_SIM_DT * omega;
        b2Vec2 fDir(-sinf(simAngle), cosf(simAngle));
        simPos += DODGE_SIM_DT * vLinear * fDir;

        // 1. Phạt cực nặng nếu quỹ đạo đâm vào tường — quét 8 hướng
        b2Vec2 wallCheckDirs[] = {
            fDir,                                   // Phía trước
            -1.0f * fDir,                           // Phía sau
            b2Vec2(-fDir.y, fDir.x),                // Bên trái
            b2Vec2(fDir.y, -fDir.x),                // Bên phải
            b2Vec2(fDir.x - fDir.y, fDir.y + fDir.x),  // Trước-trái (chéo)
            b2Vec2(fDir.x + fDir.y, fDir.y - fDir.x),  // Trước-phải (chéo)
            b2Vec2(-fDir.x - fDir.y, -fDir.y + fDir.x), // Sau-trái (chéo)
            b2Vec2(-fDir.x + fDir.y, -fDir.y - fDir.x)  // Sau-phải (chéo)
        };
        // Chuẩn hóa các vector chéo
        for (int d = 4; d < 8; ++d) {
            float len = wallCheckDirs[d].Length();
            if (len > 0.01f) {
                wallCheckDirs[d].x /= len;
                wallCheckDirs[d].y /= len;
            }
        }

        for (int d = 0; d < 8; ++d) {
            float checkDist = (d < 4) ? 0.7f : 0.5f; // Khoảng cách kiểm tra chéo ngắn hơn
            if (IsWallInDirection(world, simPos, wallCheckDirs[d], checkDist)) {
                cost += 800.0f / (float)step; // Đâm tường càng sớm phạt càng nặng
            }
        }

        // 2. Dự báo khoảng cách với đạn ở thời điểm t (bao gồm nhiều lần nảy tường)
        for (Bullet* b : bullets) {
            b2Vec2 bulletPos = b->body->GetPosition();
            b2Vec2 bulletVel = b->body->GetLinearVelocity();

            // Lọc đạn mình vừa bắn
            b2Vec2 bToBot = botPos - bulletPos;
            if (b->time > 9.5f && b2Dot(bulletVel, bToBot) < 0.0f) continue;

            b2Vec2 projectedBulletPos = GetPredictedBulletPos(world, bulletPos, bulletVel, t);

            float dist = (simPos - projectedBulletPos).Length();
            float dangerRadius = 2.5f;
            if (dist < dangerRadius) {
                // Penalty phi tuyến: càng gần càng nguy hiểm, thời gian sớm phạt nặng hơn
                float penetration = dangerRadius - dist;
                cost += penetration * penetration * 400.0f / (t + 0.05f);
            }
        }
    }

    // 3. Phạt MẠNH nếu điểm kết thúc gần tường (tránh bị dồn góc)
    b2Vec2 checkDirs[] = {
        b2Vec2(1.0f, 0.0f), b2Vec2(-1.0f, 0.0f),
        b2Vec2(0.0f, 1.0f), b2Vec2(0.0f, -1.0f),
        b2Vec2(0.707f, 0.707f), b2Vec2(-0.707f, 0.707f),
        b2Vec2(0.707f, -0.707f), b2Vec2(-0.707f, -0.707f)
    };
    int wallCount = 0;
    for (const auto& d : checkDirs) {
        if (IsWallInDirection(world, simPos, d, 1.0f)) {
            wallCount++;
            cost += 60.0f; // Mỗi hướng bị chặn phạt 60
        }
    }
    // Phạt cộng dồn nếu bị bao vây nhiều hướng (dồn góc)
    if (wallCount >= 3) {
        cost += (float)(wallCount - 2) * 150.0f;
    }

    // 4. Ưu tiên duy trì cự ly thoải mái với kẻ địch
    float distToEnemy = (simPos - enemyPos).Length();
    if (distToEnemy < 3.0f) {
        cost += (3.0f - distToEnemy) * 35.0f;
    } else if (distToEnemy > 8.0f) {
        cost += (distToEnemy - 8.0f) * 10.0f;
    }

    return cost;
}
