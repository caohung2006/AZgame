#include "bot.h"
#include <cmath>
#include <algorithm>

class EnemyRayCastCallback : public b2RayCastCallback {
public:
    b2Body* enemyBody;
    bool hitEnemy = false;

    EnemyRayCastCallback(b2Body* enemy) : enemyBody(enemy) {}

    float ReportFixture(b2Fixture* fixture, const b2Vec2& point, const b2Vec2& normal, float fraction) override {
        b2Body* hitBody = fixture->GetBody();
        if (hitBody->GetType() == b2_staticBody) {
            hitEnemy = false;
            return fraction;
        }
        if (hitBody == enemyBody) {
            hitEnemy = true;
            return fraction;
        }
        return -1.0f;
    }
};

class WallRayCastCallback : public b2RayCastCallback {
public:
    float hitDist = -1.0f;
    float ReportFixture(b2Fixture* fixture, const b2Vec2& point, const b2Vec2& normal, float fraction) override {
        if (fixture->GetBody()->GetType() == b2_staticBody) {
            hitDist = fraction;
            return fraction;
        }
        return -1.0f;
    }
};

namespace {

float NormalizeAngle(float angle) {
    while (angle > PI) angle -= 2.0f * PI;
    while (angle < -PI) angle += 2.0f * PI;
    return angle;
}

float AngleTo(const b2Vec2& from, const b2Vec2& to) {
    b2Vec2 delta = to - from;
    return atan2f(-delta.x, delta.y);
}

b2Vec2 SafeNormalize(const b2Vec2& value) {
    float length = value.Length();
    if (length < 0.0001f) return b2Vec2(0.0f, 0.0f);
    return (1.0f / length) * value;
}

float Dot(const b2Vec2& a, const b2Vec2& b) {
    return a.x * b.x + a.y * b.y;
}

int RandRange(int minValue, int maxValue) {
    if (maxValue <= minValue) return minValue;
    return minValue + (rand() % (maxValue - minValue + 1));
}

float SolveInterceptTime(const b2Vec2& shooterPos, const b2Vec2& targetPos, const b2Vec2& targetVel, float bulletSpeed) {
    b2Vec2 toTarget = targetPos - shooterPos;
    
    // Phương trình bậc 2: (V_t^2 - S_b^2)t^2 + 2(D.V_t)t + D^2 = 0
    float a = targetVel.LengthSquared() - bulletSpeed * bulletSpeed;
    float b = 2.0f * Dot(toTarget, targetVel);
    float c = toTarget.LengthSquared();

    if (fabsf(a) < 0.001f) {
        if (fabsf(b) < 0.001f) return 0.0f;
        float t = -c / b;
        return (t > 0) ? t : 0.0f;
    }

    float disc = b * b - 4.0f * a * c;
    if (disc < 0) return 0.0f;

    float t1 = (-b - sqrtf(disc)) / (2.0f * a);
    float t2 = (-b + sqrtf(disc)) / (2.0f * a);

    if (t1 > 0 && t2 > 0) return std::min(t1, t2);
    if (t1 > 0) return t1;
    if (t2 > 0) return t2;
    return 0.0f;
}

bool ShouldDetonateFrag(const std::vector<Bullet*>& bullets, int ownerIndex, const b2Vec2& enemyPos) {
    for (Bullet* b : bullets) {
        if (b && b->ownerPlayerIndex == ownerIndex && b->isFrag && !b->explodeFrag && b->time > 0.0f) {
            float dist = (b->body->GetPosition() - enemyPos).Length();
            if (dist < 2.5f) return true; // Khoảng cách nổ tối ưu
        }
    }
    return false;
}

struct RayHitInfo {
    bool hit = false;
    bool hitStatic = false;
    b2Body* body = nullptr;
    b2Vec2 point = b2Vec2(0.0f, 0.0f);
    b2Vec2 normal = b2Vec2(0.0f, 0.0f);
};

class RayHitCallback : public b2RayCastCallback {
public:
    explicit RayHitCallback(RayHitInfo* outInfo) : info(outInfo) {}
    float ReportFixture(b2Fixture* fixture, const b2Vec2& point, const b2Vec2& normal, float fraction) override {
        if (fixture->IsSensor()) return -1.0f;
        info->hit = true;
        info->body = fixture->GetBody();
        info->hitStatic = info->body && info->body->GetType() == b2_staticBody;
        info->point = point;
        info->normal = normal;
        return fraction;
    }
private:
    RayHitInfo* info;
};


bool SafeToShoot(b2World& world, const b2Vec2& origin, const b2Vec2& dir, b2Body* myBody) {
    // 1. Muzzle clearance
    RayHitInfo info1;
    RayHitCallback cb1(&info1);
    world.RayCast(&cb1, origin, origin + 1.25f * dir);
    if (info1.hit && info1.hitStatic) return false;

    // 2. Self-bounce check
    RayHitInfo info2;
    RayHitCallback cb2(&info2);
    world.RayCast(&cb2, origin, origin + 30.0f * dir);
    if (info2.hit && info2.hitStatic) {
        float proj = Dot(dir, info2.normal);
        b2Vec2 reflected = SafeNormalize(dir - 2.0f * proj * info2.normal);
        RayHitInfo info3;
        RayHitCallback cb3(&info3);
        b2Vec2 bounceStart = info2.point + 0.05f * reflected;
        world.RayCast(&cb3, bounceStart, bounceStart + 30.0f * reflected);
        if (info3.hit && info3.body == myBody) return false;
    }
    return true;
}

bool FindBounceShot(Game* game, const b2Vec2& myPos, b2Body* enemyBody, const b2Vec2& enemyPos, b2Vec2* outWallPoint) {
    if (!game || !enemyBody || !outWallPoint) return false;
    const float maxRayLength = 80.0f;
    const float baseAngle = AngleTo(myPos, enemyPos);
    float bestScore = 9999.0f;
    bool found = false;
    b2Vec2 bestPoint = b2Vec2(0.0f, 0.0f);
    for (int i = -34; i <= 34; ++i) {
        float angle = baseAngle + i * 0.07f;
        b2Vec2 dir(-sinf(angle), cosf(angle));
        RayHitInfo firstHit;
        RayHitCallback firstCallback(&firstHit);
        game->world.RayCast(&firstCallback, myPos, myPos + maxRayLength * dir);
        if (!firstHit.hit || !firstHit.hitStatic) continue;
        b2Vec2 incoming = SafeNormalize(firstHit.point - myPos);
        float projection = Dot(incoming, firstHit.normal);
        b2Vec2 reflected = SafeNormalize(incoming - 2.0f * projection * firstHit.normal);
        b2Vec2 bounceStart = firstHit.point + 0.05f * reflected;
        RayHitInfo secondHit;
        RayHitCallback secondCallback(&secondHit);
        game->world.RayCast(&secondCallback, bounceStart, bounceStart + maxRayLength * reflected);
        if (secondHit.hit && secondHit.body == enemyBody) {
            float score = std::fabs(NormalizeAngle(AngleTo(myPos, firstHit.point) - baseAngle));
            if (score < bestScore) {
                bestScore = score;
                bestPoint = firstHit.point;
                found = true;
            }
        }
    }
    if (found) *outWallPoint = bestPoint;
    return found;
}

// Tính toán rủi ro của một hướng di chuyển dựa trên TTC của các viên đạn
float EvaluateDirectionRisk(Game* game, int myPlayerIndex, const b2Vec2& myPos, const b2Vec2& myVel, const b2Vec2& testVel) {
    float totalRisk = 0.0f;
    for (auto b : game->bullets) {
        if (b->ownerPlayerIndex == myPlayerIndex || b->time <= 0.0f) continue;
        
        b2Vec2 bPos = b->body->GetPosition();
        b2Vec2 bVel = b->body->GetLinearVelocity();
        
        // Vị trí tương đối và vận tốc tương đối
        b2Vec2 relPos = myPos - bPos;
        b2Vec2 relVel = bVel - testVel; // Đạn so với xe đang di chuyển theo testVel
        
        float dist = relPos.Length();
        float closingSpeed = Dot(relPos, bVel) / dist; // Tốc độ đạn bay về phía xe
        
        if (closingSpeed > 0.5f) {
            float ttc = dist / closingSpeed;
            if (ttc < 1.5f) {
                // Rủi ro tỷ lệ nghịch với TTC
                totalRisk += (1.5f - ttc);
                
                // Bonus rủi ro nếu đạn bay trúng hitbox (khoảng cách gần nhất nhỏ)
                float speedSqr = relVel.LengthSquared();
                if (speedSqr > 0.01f) {
                    float tMin = Dot(relPos, relVel) / speedSqr;
                    if (tMin > 0) {
                        b2Vec2 closestPoint = relPos - tMin * relVel;
                        if (closestPoint.Length() < 1.2f) { // Bán kính xe tăng + padding
                            totalRisk += 1.0f;
                        }
                    }
                }
            }
        }
    }
    return totalRisk;
}

} // namespace

Bot::Bot(int level, int playerIndex) : level(level), playerIndex(playerIndex), evasionTimer(0) {}

TankActions Bot::GetAction(Game* game) {
    TankActions actions;
    if (!game) return actions;

    Tank* myTank = nullptr;
    Tank* enemyTank = nullptr;

    for (auto t : game->tanks) {
        if (t->playerIndex == playerIndex) {
            myTank = t;
        } else if (!t->isDestroyed) {
            enemyTank = t; // Chỉ nhắm vào 1 kẻ địch còn sống
        }
    }

    if (!myTank || myTank->isDestroyed) return actions;

    b2Vec2 myPos = myTank->body->GetPosition();
    float myAngle = myTank->body->GetAngle();
    b2Vec2 forwardDir(-sinf(myAngle), cosf(myAngle));

    // ==========================================
    // Cấp độ 1: "Bao Cát Tĩnh" (Học nền tảng)
    // ==========================================
    if (level == 1) {
        return actions; // Đứng im, không bắn, không né
    }

    // ==========================================
    // Cấp độ 7: "Kẻ Đào Tẩu" (Fleeing Bot - Dùng cho Phase 5)
    // ==========================================
    if (level == 7) {
        // Level 7 chỉ chạy trốn, không bắn
        // Logic chọn góc trốn nằm ở phần Virtual Target bên dưới (line ~334)
    }

    // ==========================================
    // Cấp độ 2+: Tự động tìm đường và ngắm bắn
    // ==========================================
    float enemyDist = 999.0f;
    bool enemyInSight = false;
    bool pathClear = false;
    bool hasBounceShot = false;
    if (enemyTank) {
        b2Vec2 toEnemy = enemyTank->body->GetPosition() - myPos;
        enemyDist = toEnemy.Length();

        // 1. Tầm nhìn hẹp (Thin Raycast) để bắn tỉa từ xa
        EnemyRayCastCallback sniperCB(enemyTank->body);
        game->world.RayCast(&sniperCB, myPos, enemyTank->body->GetPosition());
        enemyInSight = sniperCB.hitEnemy;

        // 2. Tầm nhìn rộng (Fat Raycast) để dẫn đường A* tránh kẹt góc
        pathClear = CheckClearance(game->world, myPos, enemyTank->body->GetPosition());
        
        // Cập nhật enemyInSight cho logic dẫn đường (dùng Fat Raycast để tránh bot lao vào góc tường)
        // Nhưng logic bắn sẽ dùng sniperCB.hitEnemy
    }

    // 1. Xác định VIRTUAL TARGET (Mục tiêu ảo)
    b2Vec2 virtualTarget = myPos;
    if (enemyTank) {
        virtualTarget = enemyTank->body->GetPosition();
        
        // [CẤP ĐỘ 7] Mục tiêu là góc xa nhất để trốn chạy
        if (level == 7) {
            b2Vec2 enemyPos = enemyTank->body->GetPosition();
            b2Vec2 corners[4] = {
                {2.0f, 2.0f},
                {SCREEN_WIDTH/SCALE - 2.0f, 2.0f},
                {2.0f, SCREEN_HEIGHT/SCALE - 2.0f},
                {SCREEN_WIDTH/SCALE - 2.0f, SCREEN_HEIGHT/SCALE - 2.0f}
            };
            float maxDist = -1.0f;
            for(int i=0; i<4; i++) {
                float d = (corners[i] - enemyPos).Length();
                if (d > maxDist) {
                    maxDist = d;
                    virtualTarget = corners[i];
                }
            }
        }

        if (pathClear) {
            // Khi thấy địch, mục tiêu ảo là vị trí hiện tại hoặc vị trí đón đầu của địch
            if (level >= 4 && level != 7) {
                float bulletSpeed = 6.0f;
                if (myTank->currentWeapon == ItemType::GATLING) bulletSpeed = 10.0f;
                else if (myTank->currentWeapon == ItemType::DEATH_RAY) bulletSpeed = 8.0f;
                
                float t = SolveInterceptTime(myPos, enemyTank->body->GetPosition(), enemyTank->body->GetLinearVelocity(), bulletSpeed);
                if (t > 0) virtualTarget = enemyTank->body->GetPosition() + t * enemyTank->body->GetLinearVelocity();
                else virtualTarget = enemyTank->body->GetPosition();
            }
            // Hủy cache path khi thấy địch trực tiếp
            cachedPath.clear();
            currentWaypointIdx = 0;
            
            game->botPaths[playerIndex].clear();
            game->botPaths[playerIndex].push_back(myPos);
            game->botPaths[playerIndex].push_back(virtualTarget);
        } else if (game->mapEnabled) {
            // === CHẾ ĐỘ LÙI XE khi đang bị kẹt ===
            if (backupTimer > 0) {
                backupTimer--;
                actions.backward = true;
                if (backupTimer == 0) {
                    b2Vec2 pathTarget = (level == 7) ? virtualTarget : enemyTank->body->GetPosition();
                    cachedPath = game->map.GetFullPath(game->world, myPos, pathTarget, blockedCells);
                    currentWaypointIdx = 1;
                    lastEnemyPos = enemyTank->body->GetPosition();
                    game->botPaths[playerIndex] = cachedPath;
                    if (currentWaypointIdx < (int)cachedPath.size()) {
                        virtualTarget = cachedPath[currentWaypointIdx];
                    }
                }
                return actions;
            }
            

            // === THEO DÕI CHUỖI WAYPOINT ===
            bool needRecalc = false;
            
            if (cachedPath.empty() || currentWaypointIdx >= (int)cachedPath.size()) {
                needRecalc = true;
            } else {
                // [Cách 2] Waypoint Tolerance: bán kính chấp nhận = 1.2 đơn vị (~36 pixels)
                // Không ép bot phải đi tới chính xác tọa độ waypoint → tránh xoay vòng vòng
                b2Vec2 currentWP = cachedPath[currentWaypointIdx];
                float distToWP = (currentWP - myPos).Length();
                if (distToWP < 1.2f) {
                    currentWaypointIdx++;
                    if (currentWaypointIdx >= (int)cachedPath.size()) {
                        needRecalc = true;
                    }
                }
                
                // Phát hiện bị kẹt
                float speed = myTank->body->GetLinearVelocity().Length();
                if (speed < 0.3f) {
                    stuckCounter++;
                    if (stuckCounter > 30) {
                        float cellW = 90.0f, cellH = 90.0f;
                        float offsetX = (SCREEN_WIDTH - (GameMap::COLS * cellW)) / 2.0f;
                        float offsetY = (SCREEN_HEIGHT - (GameMap::ROWS * cellH)) / 2.0f - 50.0f;
                        int col = (int)floorf((myPos.x * SCALE - offsetX) / cellW);
                        int row = (int)floorf((SCREEN_HEIGHT - myPos.y * SCALE - offsetY) / cellH);
                        if (col >= 0 && col < GameMap::COLS && row >= 0 && row < GameMap::ROWS) {
                            blockedCells.push_back({row, col});
                        }
                        backupTimer = 20;
                        cachedPath.clear();
                        stuckCounter = 0;
                        return actions;
                    }
                } else {
                    stuckCounter = 0;
                    if (!blockedCells.empty()) {
                        static int clearTimer = 0;
                        clearTimer++;
                        if (clearTimer > 180) {
                            blockedCells.erase(blockedCells.begin());
                            clearTimer = 0;
                        }
                    }
                }
                
                // [Path Commitment] Chỉ tính lại đường khi Mục tiêu đã BƯỚC SANG Ô CARO KHÁC
                if (!needRecalc) {
                    float cellW = 90.0f, cellH = 90.0f;
                    float offsetX = (SCREEN_WIDTH - (GameMap::COLS * cellW)) / 2.0f;
                    float offsetY = (SCREEN_HEIGHT - (GameMap::ROWS * cellH)) / 2.0f - 50.0f;
                    
                    b2Vec2 enemyPos = enemyTank->body->GetPosition();
                    int currentEnemyCol = (int)floorf((enemyPos.x * SCALE - offsetX) / cellW);
                    int currentEnemyRow = (int)floorf((SCREEN_HEIGHT - enemyPos.y * SCALE - offsetY) / cellH);
                    
                    int lastEnemyCol = (int)floorf((lastEnemyPos.x * SCALE - offsetX) / cellW);
                    int lastEnemyRow = (int)floorf((SCREEN_HEIGHT - lastEnemyPos.y * SCALE - offsetY) / cellH);
                    
                    // Nếu mục tiêu di chuyển sang ô lưới khác -> Tính lại A*
                    if (currentEnemyRow != lastEnemyRow || currentEnemyCol != lastEnemyCol) {
                        needRecalc = true;
                    }
                }
            }
            
            // Khi tính lại đường mới
            if (needRecalc) {
                b2Vec2 pathTarget = (level == 7) ? virtualTarget : enemyTank->body->GetPosition();
                cachedPath = game->map.GetFullPath(game->world, myPos, pathTarget, blockedCells);
                currentWaypointIdx = 1;
                stuckCounter = 0;
                lastEnemyPos = enemyTank->body->GetPosition();
            }
            
            game->botPaths[playerIndex] = cachedPath;
            
            if (currentWaypointIdx < (int)cachedPath.size()) {
                virtualTarget = cachedPath[currentWaypointIdx];
            }
        }
    }

    // 2. Di chuyển và ngắm bắn
    b2Vec2 aimTarget = myPos;
    if (enemyTank) {
        // [Bước 1: Dự báo điểm mù] Predictive Aiming (Dùng Quadratic Intercept)
        aimTarget = enemyTank->body->GetPosition();

        if (level >= 4) {
            float bulletSpeed = 6.0f;
            if (myTank->currentWeapon == ItemType::GATLING) bulletSpeed = 10.0f;
            else if (myTank->currentWeapon == ItemType::DEATH_RAY) bulletSpeed = 8.0f;
            
            float t = SolveInterceptTime(myPos, aimTarget, enemyTank->body->GetLinearVelocity(), bulletSpeed);
            if (t > 0 && t < 3.0f) {
                aimTarget = aimTarget + t * enemyTank->body->GetLinearVelocity();
                
                // [Gatling Spray] Bắn quét hình quạt để tăng tỉ lệ trúng
                if (myTank->currentWeapon == ItemType::GATLING) {
                    b2Vec2 toTarget = aimTarget - myPos;
                    b2Vec2 perp(-toTarget.y, toTarget.x);
                    perp.Normalize();
                    float sprayScale = (rand() % 100 - 50) / 50.0f * 0.4f; // Sai số nhẹ
                    aimTarget = aimTarget + sprayScale * perp;
                }
            }
        }

        // [Bước 1.5: Bắn nảy tường cho Level 6]
        if (!enemyInSight && level >= 6 && myTank->currentWeapon != ItemType::DEATH_RAY && myTank->currentWeapon != ItemType::MISSILE) {
            b2Vec2 wallPt;
            if (FindBounceShot(game, myPos, enemyTank->body, enemyTank->body->GetPosition(), &wallPt)) {
                aimTarget = wallPt;
                hasBounceShot = true;
            }
        }

        // [Bước 2: Xoay xe/nòng súng]
        // Ưu tiên: Nếu thấy địch (trực tiếp hoặc nảy) thì xoay về phía đó, nếu không thì xoay về Waypoint
        b2Vec2 lookTarget = virtualTarget;
        if (level != 7 && (enemyInSight || hasBounceShot)) {
            lookTarget = aimTarget;
        }
        b2Vec2 toLook = lookTarget - myPos;
        
        if (toLook.LengthSquared() > 0.01f) {
            float absAngle = atan2f(-toLook.x, toLook.y);
            float relAngle = absAngle - myAngle;

            // Chuẩn hóa góc về [-PI, PI]
            while (relAngle > PI) relAngle -= 2 * PI;
            while (relAngle < -PI) relAngle += 2 * PI;
            
            // Xoay hướng về mục tiêu
            if (relAngle > 0.05f) actions.turnLeft = true;
            else if (relAngle < -0.05f) actions.turnRight = true;

            // [Bước 3: Di chuyển thông minh]
            // Chỉ tiến lên nếu đang ngắm về phía Waypoint (để tránh đi lệch đường)
            // HOẶC nếu đang bắn nhau thì có thể tiến/lùi tùy logic Kiting bên dưới
            b2Vec2 toWaypoint = virtualTarget - myPos;
            float wpAngle = atan2f(-toWaypoint.x, toWaypoint.y);
            float wpRelAngle = wpAngle - myAngle;
            while (wpRelAngle > PI) wpRelAngle -= 2 * PI;
            while (wpRelAngle < -PI) wpRelAngle += 2 * PI;

            if (cosf(wpRelAngle) > 0.8f) {
                // [Phanh gấp để ngắm]
                // Nếu đang nhắm bắn địch ở xa (> 5.0 units ~ 150px), ưu tiên đứng im để xoay thân xe chính xác
                bool isSniperAiming = (level >= 4 && enemyInSight && (aimTarget - myPos).Length() > 5.0f);
                if (!isSniperAiming) {
                    actions.forward = true;
                }
            }
        }

        // [Bước 4: Kỷ luật bóp cò] Trigger Discipline
        if ((enemyInSight || hasBounceShot) && myTank->shootCooldownTimer <= 0) {
            float dist = (aimTarget - myPos).Length();
            
            // Level 2+ biết bắn, nhưng Level 2 bắn rất chậm. Level 7 không bắn.
            bool canShoot = (level >= 2 && level != 7);
            if (level == 2 && rand() % 60 != 0) canShoot = false; // Level 2: Chỉ bắn ~1 lần mỗi giây
            
            if (canShoot) {
                b2Vec2 toAim = aimTarget - myPos;
                toAim.Normalize();
                float dot = forwardDir.x * toAim.x + forwardDir.y * toAim.y;
                
                // [Kỷ luật bóp cò] Dynamic Tolerance
                float accuracyThreshold = 0.985f; // Mặc định ~10 độ
                
                if (hasBounceShot) {
                    accuracyThreshold = 0.999f; // Bắn nảy cần cực chuẩn
                } else if (level >= 4) {
                    if (dist > 10.0f) accuracyThreshold = 0.999f;      // Xa (> 300px): < 2 độ
                    else if (dist > 5.0f) accuracyThreshold = 0.996f; // Trung (150-300px): < 5 độ
                } else {
                    // Logic cũ cho Level thấp
                    if (dist < 4.0f) accuracyThreshold = 0.95f; 
                    if (dist < 2.0f) accuracyThreshold = 0.85f;
                }
                
                if (dot > accuracyThreshold) {
                    if (SafeToShoot(game->world, myPos, forwardDir, myTank->body)) {
                        actions.shoot = true;
                    }
                }
            }
        }

        // [Kích nổ đạn Frag] Tự động nổ khi gần địch
        if (myTank->currentWeapon == ItemType::FRAG || myTank->currentWeapon == ItemType::NORMAL) {
             if (ShouldDetonateFrag(game->bullets, playerIndex, enemyTank->body->GetPosition())) {
                 actions.shoot = true; // Lệnh bắn ở đây sẽ kích hoạt nổ Frag trong Tank::FireWeapon
             }
        }
    }

    // ========================================================================
    // CẤP ĐỘ 4+: MÁY TRẠNG THÁI ƯU TIÊN (Priority State Machine)
    // ========================================================================
    if (level >= 4) {
        // 0. KHÓA TRẠNG THÁI (Commitment Lock) - Chỉ áp dụng cho Level 5+ (biết né)
        if (level >= 5 && evasionTimer > 0) {
            evasionTimer--;
            // Luôn cập nhật lệnh bắn từ logic ngắm ở trên (đã tính trước evasion)
            lockedActions.shoot = actions.shoot;
            return lockedActions;
        }

        // 🔴 ƯU TIÊN 1: TRẠNG THÁI SINH TỒN (Né Đạn Chủ Động)
        // Level 5+: Tổng hợp hướng né vuông góc từ TẤT CẢ đạn nguy hiểm (weighted by 1/TTC)
        if (level >= 5) {
            // === BƯỚC 1: Thu thập TẤT CẢ đạn nguy hiểm + tính vector né cho từng viên ===
            b2Vec2 totalEscapeVec(0, 0);  // Tổng hợp các hướng né (weighted)
            int threatCount = 0;

            for (auto b : game->bullets) {
                if (b->ownerPlayerIndex == playerIndex || b->time <= 0.0f) continue;

                b2Vec2 bPos = b->body->GetPosition();
                b2Vec2 bVel = b->body->GetLinearVelocity();
                b2Vec2 relPos = myPos - bPos;
                float dist = relPos.Length();
                if (dist < 0.1f) continue;

                // Đạn có bay về phía xe không?
                float closingSpeed = Dot(relPos, bVel) / dist;
                if (closingSpeed <= 0.5f) continue;

                float ttc = dist / closingSpeed;
                if (ttc > 2.0f) continue; // Phản ứng trong vòng 2 giây (đủ thời gian xoay xe 90°)

                // Đạn có trúng hitbox không? (closest approach < 1.5 unit)
                float speedSqr = bVel.LengthSquared();
                if (speedSqr < 0.01f) continue;

                float tMin = Dot(relPos, bVel) / speedSqr;
                if (tMin <= 0) continue;
                b2Vec2 closestDelta = relPos - tMin * bVel;
                float missDist = closestDelta.Length();
                if (missDist > 1.5f) continue;

                // Viên đạn này NGUY HIỂM → tính vector né vuông góc
                b2Vec2 bulletDir = SafeNormalize(bVel);
                // Vector vuông góc: chọn hướng xa đạn hơn (dựa trên vị trí tương đối)
                b2Vec2 perpA(-bulletDir.y, bulletDir.x);
                b2Vec2 perpB(bulletDir.y, -bulletDir.x);
                // Chọn hướng mà xe đang ở (để né theo hướng tự nhiên nhất)
                b2Vec2 chosenPerp = (Dot(relPos, perpA) > 0) ? perpA : perpB;

                // Trọng số: TTC càng nhỏ → càng khẩn cấp → weight càng lớn
                float urgency = 1.0f / std::max(0.05f, ttc);
                // Bonus weight nếu đạn gần trúng chính xác (missDist nhỏ)
                urgency *= (1.0f + (1.5f - missDist));

                totalEscapeVec = totalEscapeVec + urgency * chosenPerp;
                threatCount++;
            }

            // === BƯỚC 2: Nếu có đạn nguy hiểm, tính hướng né tổng hợp ===
            if (threatCount > 0 && totalEscapeVec.LengthSquared() > 0.01f) {
                b2Vec2 escapeDir = SafeNormalize(totalEscapeVec);

                // Kiểm tra tường: nếu hướng né bị chặn, thử hướng ngược
                WallRayCastCallback cbMain;
                game->world.RayCast(&cbMain, myPos, myPos + 2.0f * escapeDir);
                if (cbMain.hitDist >= 0) {
                    // Bị tường chặn → thử hướng ngược
                    b2Vec2 altDir = -1.0f * escapeDir;
                    WallRayCastCallback cbAlt;
                    game->world.RayCast(&cbAlt, myPos, myPos + 2.0f * altDir);
                    if (cbAlt.hitDist < 0) {
                        escapeDir = altDir; // Hướng ngược thoáng → dùng
                    }
                    // Nếu cả 2 đều bị tường → vẫn dùng escapeDir gốc (ít nhất cố gắng)
                }

                // === BƯỚC 3: Chuyển hướng né thành TankActions ===
                float escapeAngle = atan2f(-escapeDir.x, escapeDir.y);
                float relAngle = NormalizeAngle(escapeAngle - myAngle);
                float dotFwd = Dot(forwardDir, escapeDir);

                TankActions dodgeAct;

                if (dotFwd > 0.3f) {
                    dodgeAct.forward = true;
                    if (relAngle > 0.1f) dodgeAct.turnLeft = true;
                    else if (relAngle < -0.1f) dodgeAct.turnRight = true;
                } else if (dotFwd < -0.3f) {
                    dodgeAct.backward = true;
                    if (relAngle > 0.1f) dodgeAct.turnRight = true;
                    else if (relAngle < -0.1f) dodgeAct.turnLeft = true;
                } else {
                    dodgeAct.forward = true;
                    if (relAngle > 0) dodgeAct.turnLeft = true;
                    else dodgeAct.turnRight = true;
                }

                dodgeAct.shoot = actions.shoot; // Kế thừa lệnh bắn — né VÀ bắn cùng lúc
                lockedActions = dodgeAct;
                // Nhiều đạn → lock ngắn hơn để re-evaluate thường xuyên
                evasionTimer = (threatCount > 1) ? RandRange(5, 10) : RandRange(8, 15);
                return lockedActions;
            }
        }

        // 🟡 ƯU TIÊN 2: TRẠNG THÁI XẠ THỦ (Bắn Tỉa & Thả Diều)
        if (level != 7 && (enemyInSight || hasBounceShot)) {
            // Stationary Pivot: Dừng xe để xoay thân chính xác
            bool isSniperAiming = (enemyDist > 5.0f || hasBounceShot);
            if (isSniperAiming) {
                actions.forward = false;
                actions.backward = false;
            }
            
            // Kỹ năng Thả Diều (Kiting): Lùi lại nếu địch quá gần (Chỉ Level 4+)
            if (enemyInSight && enemyDist < 4.0f) {
                actions.forward = false;
                actions.backward = true;
                WallRayCastCallback backWall;
                game->world.RayCast(&backWall, myPos, myPos - 1.2f * forwardDir);
                if (backWall.hitDist >= 0) {
                    actions.backward = false;
                    actions.turnLeft = true;
                }
            }
            
            // 🛡️ CHIẾN THUẬT KHIÊN (Shield) - Chỉ Level 6
            if (level >= 6 && myTank->shieldCooldownTimer <= 0.0f && !myTank->hasShield) {
                // Tự động bật khiên khi đang ngắm bắn nhau
                actions.shield = true;
            }

            return actions;
        }

        // 🟢 ƯU TIÊN 3: TRẠNG THÁI TRUY LÙNG (Tìm Đường)
        // Hành động tiến/lùi/xoay đã được tính toán ở Bước 2 (dựa trên Waypoint)
        // Ở đây ta chỉ thêm logic tránh kẹt tường (nếu có)
        WallRayCastCallback frontWall;
        game->world.RayCast(&frontWall, myPos, myPos + 1.2f * forwardDir);
        if (frontWall.hitDist >= 0 && actions.forward) {
            // Đang định tiến mà phía trước có tường -> phanh lại để xoay
            actions.forward = false;
        }

        return actions;
    }


    // ==========================================
    // ANTI-STUCK: Xử lý kẹt góc tường
    // ==========================================
    if (actions.turnLeft || actions.turnRight) {
        bool touchingWall = false;
        for (b2ContactEdge* edge = myTank->body->GetContactList(); edge; edge = edge->next) {
            if (edge->contact->IsTouching() && edge->other->GetType() == b2_staticBody) {
                touchingWall = true;
                break;
            }
        }
        
        if (touchingWall) {
            actions.forward = false;
        }
    }

    return actions;
}
