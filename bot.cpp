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

Bot::Bot(int level, int playerIndex) : level(level), playerIndex(playerIndex) {}

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
    // Cấp độ 1: Dummy (Bia tập bắn)
    // ==========================================
    if (level == 1) {
        if (rand() % 100 < 5) actions.forward = true;
        if (rand() % 100 < 5) actions.turnLeft = true;
        return actions;
    }

    // ==========================================
    // Cấp độ 2+: Tự động tìm đường và ngắm bắn
    // ==========================================
    float enemyDist = 999.0f;
    bool enemyInSight = false;

    if (enemyTank) {
        b2Vec2 toEnemy = enemyTank->body->GetPosition() - myPos;
        enemyDist = toEnemy.Length();

        // Sử dụng CheckClearance (Fat Raycast) thay vì tia đơn lẻ để tránh lỗi tia lọt qua kẽ tường gây nhiễu A*
        enemyInSight = CheckClearance(game->world, myPos, enemyTank->body->GetPosition());
    }

    // 1. Xác định VIRTUAL TARGET (Mục tiêu ảo)
    b2Vec2 virtualTarget = myPos;
    if (enemyTank) {
        virtualTarget = enemyTank->body->GetPosition();
        
        if (enemyInSight) {
            // Khi thấy địch, mục tiêu ảo là vị trí hiện tại hoặc vị trí đón đầu của địch
            if (level >= 5) {
                float dist = (virtualTarget - myPos).Length();
                float timeToHit = dist / 15.0f;
                virtualTarget = virtualTarget + timeToHit * enemyTank->body->GetLinearVelocity();
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
                    cachedPath = game->map.GetFullPath(game->world, myPos, enemyTank->body->GetPosition(), blockedCells);
                    currentWaypointIdx = 1;
                    lastEnemyPos = enemyTank->body->GetPosition();
                    recalcCooldown = 30; // Không tính lại trong 30 frame
                    game->botPaths[playerIndex] = cachedPath;
                    if (currentWaypointIdx < (int)cachedPath.size()) {
                        virtualTarget = cachedPath[currentWaypointIdx];
                    }
                }
                return actions;
            }
            
            // === Giảm Cooldown mỗi frame ===
            // [Path Commitment] Đã phóng lao thì phải theo lao
            // Xóa bỏ hoàn toàn recalcCooldown (tính theo frame).
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
                        float offsetX = (SCREEN_WIDTH - (8 * cellW)) / 2.0f;
                        float offsetY = (SCREEN_HEIGHT - (6 * cellH)) / 2.0f - 50.0f;
                        int col = (int)floorf((myPos.x * SCALE - offsetX) / cellW);
                        int row = (int)floorf((SCREEN_HEIGHT - myPos.y * SCALE - offsetY) / cellH);
                        if (col >= 0 && col < 8 && row >= 0 && row < 6) {
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
                    float offsetX = (SCREEN_WIDTH - (8 * cellW)) / 2.0f;
                    float offsetY = (SCREEN_HEIGHT - (6 * cellH)) / 2.0f - 50.0f;
                    
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
                cachedPath = game->map.GetFullPath(game->world, myPos, enemyTank->body->GetPosition(), blockedCells);
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
    if (enemyTank) {
        // [Bước 1: Dự báo điểm mù] Predictive Aiming
        // Tính toán vị trí địch sẽ đứng khi đạn bay tới nơi
        b2Vec2 aimTarget = enemyTank->body->GetPosition();
        if (level >= 5) {
            float dist = (aimTarget - myPos).Length();
            float bulletSpeed = 6.0f; // Tốc độ đạn mặc định
            if (myTank->currentWeapon == ItemType::GATLING) bulletSpeed = 10.0f;
            else if (myTank->currentWeapon == ItemType::DEATH_RAY) bulletSpeed = 8.0f;
            
            // Tương lai = Hiện tại + (Quãng đường / Tốc độ đạn) * Vận tốc địch
            aimTarget = aimTarget + (dist / bulletSpeed) * enemyTank->body->GetLinearVelocity();
        }

        // [Bước 2: Xoay xe/nòng súng]
        // Ưu tiên: Nếu thấy địch thì xoay về phía địch để bắn, nếu không thấy thì xoay về Waypoint để đi
        b2Vec2 lookTarget = (enemyInSight && level >= 3) ? aimTarget : virtualTarget;
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
                actions.forward = true;
            }
        }

        // [Bước 4: Kỷ luật bóp cò] Trigger Discipline
        if (enemyInSight && myTank->shootCooldownTimer <= 0) {
            b2Vec2 toAim = aimTarget - myPos;
            toAim.Normalize();
            float dot = forwardDir.x * toAim.x + forwardDir.y * toAim.y;
            
            // Ngưỡng góc ngắm: Càng xa ngắm càng phải kỹ (accuracyThreshold cao)
            float accuracyThreshold = 0.99f; // Ngắm cực chuẩn (~8 độ)
            float dist = (aimTarget - myPos).Length();
            if (dist < 4.0f) accuracyThreshold = 0.95f; 
            if (dist < 2.0f) accuracyThreshold = 0.85f; // Gần quá thì bắn bừa
            
            if (dot > accuracyThreshold) {
                actions.shoot = true;
            }
        }
    }

    // ==========================================
    // Cấp độ 3+: Né Đạn (Veteran)
    // ==========================================
    if (level >= 3) {
        b2Vec2 myVel = myTank->body->GetLinearVelocity();
        float minTTC = 999.0f;
        b2Vec2 dangerBulletVel(0,0);
        b2Vec2 dangerBulletPos(0,0);
        
        for (auto b : game->bullets) {
            if (b->ownerPlayerIndex != playerIndex && b->time > 0.0f) {
                b2Vec2 relPos = myPos - b->body->GetPosition();
                b2Vec2 bVel = b->body->GetLinearVelocity();
                b2Vec2 relVel = bVel - myVel;
                float speedSqr = relVel.LengthSquared();
                
                if (speedSqr > 0.1f) {
                    float t = (relPos.x * relVel.x + relPos.y * relVel.y) / speedSqr;
                    if (t > 0 && t < 1.2f) { // Đạn sẽ chạm trong vòng 1.2 giây
                        b2Vec2 closestPoint = b->body->GetPosition() + t * bVel;
                        b2Vec2 myExpectedPos = myPos + t * myVel;
                        float missDist = (closestPoint - myExpectedPos).Length();
                        
                        if (missDist < 1.2f) { // Bán kính nguy hiểm (khoảng 36 pixels)
                            if (t < minTTC) {
                                minTTC = t;
                                dangerBulletVel = bVel;
                                dangerBulletPos = b->body->GetPosition();
                            }
                        }
                    }
                }
            }
        }

        if (minTTC < 1.0f) { // Nguy hiểm cận kề
            actions.forward = false; // Ngừng tiến
            actions.backward = false;
            actions.turnLeft = false;
            actions.turnRight = false;
            
            // Tìm hướng vuông góc với đạn để lách
            b2Vec2 toMe = myPos - dangerBulletPos;
            float cross = toMe.x * dangerBulletVel.y - toMe.y * dangerBulletVel.x;
            float dot = forwardDir.x * dangerBulletVel.x + forwardDir.y * dangerBulletVel.y;
            
            // Bẻ lái cố định về 1 hướng để thoát khỏi đường bay của đạn
            if (cross > 0) {
                actions.turnRight = true;
            } else {
                actions.turnLeft = true;
            }

            // Tiến hoặc lùi tùy thuộc vào việc đạn đang đến từ phía trước hay sau lưng
            if (dot < 0) {
                actions.backward = true; // Đạn tới từ phía trước -> lùi lại
            } else {
                actions.forward = true; // Đạn rượt từ sau lưng -> vọt tới
            }
            
            // Né là ưu tiên số 1, ghi đè các hành động di chuyển khác nhưng vẫn có thể bắn
            if (enemyInSight && myTank->shootCooldownTimer <= 0) actions.shoot = true;
            return actions;
        }
    }

    // ==========================================
    // Cấp độ 4+: Kiting (Boss) - Thả diều
    // ==========================================
    if (level >= 4) {
        if (enemyDist < 8.0f && enemyInSight) { // Kẻ địch ở quá gần
            actions.forward = false;
            actions.backward = true; // Vừa lùi vừa bắn
        }
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
        
        // Nếu chạm tường trong lúc đang cố xoay xe
        if (touchingWall) {
            // Chỉ cần hủy lệnh tiến lên, vật lý Box2D sẽ tự đẩy xe ra khi xe cố xoay tại chỗ
            // Không dùng lệnh backward nữa vì nó gây ra vòng lặp tiến-lùi (jittering)
            actions.forward = false;
        }
    }

    return actions;
}
