#include "bot.h"
#include <algorithm>
#include <cmath>

namespace {

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

struct BulletThreat {
    bool active = false;
    float ttc = 999.0f;
    b2Vec2 bulletPos = b2Vec2(0.0f, 0.0f);
    b2Vec2 bulletVel = b2Vec2(0.0f, 0.0f);
};

static int s_dodgeFrames[4] = {0, 0, 0, 0};
static b2Vec2 s_dodgeDir[4] = {
    b2Vec2(1.0f, 0.0f),
    b2Vec2(1.0f, 0.0f),
    b2Vec2(1.0f, 0.0f),
    b2Vec2(1.0f, 0.0f)
};
static float s_lastThreatTtc[4] = {999.0f, 999.0f, 999.0f, 999.0f};
static int s_weaveFrames[4] = {0, 0, 0, 0};
static int s_weaveDir[4] = {1, -1, 1, -1};

int ClampPlayerIndex(int index) {
    return std::max(0, std::min(3, index));
}

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

float SolveInterceptTime(const b2Vec2& shooterPos, const b2Vec2& targetPos, const b2Vec2& targetVel, float bulletSpeed, float maxTime) {
    b2Vec2 toTarget = targetPos - shooterPos;
    float speed = std::max(bulletSpeed, 0.1f);
    float a = Dot(targetVel, targetVel) - speed * speed;
    float b = 2.0f * Dot(toTarget, targetVel);
    float c = Dot(toTarget, toTarget);

    if (fabsf(a) < 0.0001f) {
        if (fabsf(b) < 0.0001f) return std::min(maxTime, c / speed);
        float t = -c / b;
        if (t > 0.0f) return std::min(t, maxTime);
        return std::min(maxTime, c / speed);
    }

    float disc = b * b - 4.0f * a * c;
    if (disc < 0.0f) return std::min(maxTime, c / speed);

    float sqrtDisc = sqrtf(disc);
    float t1 = (-b - sqrtDisc) / (2.0f * a);
    float t2 = (-b + sqrtDisc) / (2.0f * a);
    float t = 9999.0f;

    if (t1 > 0.0f) t = t1;
    if (t2 > 0.0f) t = std::min(t, t2);
    if (t == 9999.0f) return std::min(maxTime, c / speed);

    return std::min(t, maxTime);
}

int CountOwnedActiveBullets(const std::vector<Bullet*>& bullets, int ownerIndex) {
    int count = 0;
    for (Bullet* bullet : bullets) {
        if (bullet && bullet->ownerPlayerIndex == ownerIndex && bullet->time > 0.0f) {
            count++;
        }
    }
    return count;
}

bool ShouldDetonateFrag(const std::vector<Bullet*>& bullets, int ownerIndex, const b2Vec2& enemyPos) {
    for (Bullet* bullet : bullets) {
        if (!bullet) continue;
        if (bullet->ownerPlayerIndex != ownerIndex) continue;
        if (!bullet->isFrag || bullet->explodeFrag || bullet->time <= 0.0f) continue;
        float dist = (bullet->body->GetPosition() - enemyPos).Length();
        if (dist < 2.6f) return true;
    }
    return false;
}

bool HasMuzzleClearance(b2World& world, const b2Vec2& origin, const b2Vec2& direction, float length) {
    RayHitInfo info;
    RayHitCallback callback(&info);
    world.RayCast(&callback, origin, origin + length * direction);
    return !(info.hit && info.hitStatic);
}

BulletThreat FindBulletThreat(const std::vector<Bullet*>& bullets, int myPlayerIndex, const b2Vec2& myPos, const b2Vec2& myVel) {
    BulletThreat best;

    for (Bullet* bullet : bullets) {
        if (!bullet || bullet->time <= 0.0f || bullet->ownerPlayerIndex == myPlayerIndex) continue;

        b2Vec2 bulletPos = bullet->body->GetPosition();
        b2Vec2 bulletVel = bullet->body->GetLinearVelocity();
        b2Vec2 relPos = myPos - bulletPos;
        b2Vec2 relVel = bulletVel - myVel;
        float speedSqr = relVel.LengthSquared();

        if (speedSqr < 0.05f) continue;

        float t = Dot(relPos, relVel) / speedSqr;
        if (t <= 0.0f || t > 2.5f) continue;

        b2Vec2 bulletFuture = bulletPos + t * bulletVel;
        b2Vec2 myFuture = myPos + t * myVel;
        float missDistance = (bulletFuture - myFuture).Length();

        if (missDistance < 1.9f && t < best.ttc) {
            best.active = true;
            best.ttc = t;
            best.bulletPos = bulletPos;
            best.bulletVel = bulletVel;
        }
    }

    return best;
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
        if (incoming.LengthSquared() < 0.01f) continue;

        float projection = Dot(incoming, firstHit.normal);
        b2Vec2 reflected = SafeNormalize(incoming - 2.0f * projection * firstHit.normal);
        if (reflected.LengthSquared() < 0.01f) continue;

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

    if (found) {
        *outWallPoint = bestPoint;
    }

    return found;
}

} // namespace

Bot::Bot(int level, int playerIndex) : level(level), playerIndex(playerIndex) {}

TankActions Bot::GetAction(Game* game) {
    TankActions actions;
    if (!game) return actions;

    Tank* myTank = nullptr;
    Tank* enemyTank = nullptr;

    for (Tank* tank : game->tanks) {
        if (tank->playerIndex == playerIndex) {
            myTank = tank;
        } else if (!tank->isDestroyed) {
            enemyTank = tank;
        }
    }

    if (!myTank || myTank->isDestroyed) return actions;

    const b2Vec2 myPos = myTank->body->GetPosition();
    const b2Vec2 myVel = myTank->body->GetLinearVelocity();
    const float myAngle = myTank->body->GetAngle();
    const b2Vec2 forwardDir(-sinf(myAngle), cosf(myAngle));

    if (level == 1) {
        if (rand() % 100 < 5) actions.forward = true;
        if (rand() % 100 < 5) actions.turnLeft = true;
        return actions;
    }

    // 1. Evaluate Threats & Emergency Dodge
    BulletThreat threat = FindBulletThreat(game->bullets, playerIndex, myPos, myVel);
    if (threat.active && threat.ttc < 1.8f) {
        // Use Shield if extremely close
        if (threat.ttc < 0.6f && myTank->shieldCooldownTimer <= 0.0f && level >= 4) {
            actions.shield = true;
        }

        // Dodge
        b2Vec2 bulletDir = SafeNormalize(threat.bulletVel);
        b2Vec2 toThreat = threat.bulletPos - myPos;
        float cross = toThreat.x * bulletDir.y - toThreat.y * bulletDir.x;
        b2Vec2 escapeDir = (cross > 0.0f) ? b2Vec2(-bulletDir.y, bulletDir.x) : b2Vec2(bulletDir.y, -bulletDir.x);
        
        float escapeAngle = NormalizeAngle(AngleTo(myPos, myPos + escapeDir) - myAngle);
        if (escapeAngle > 0.1f) actions.turnLeft = true;
        else if (escapeAngle < -0.1f) actions.turnRight = true;
        
        if (std::cos(escapeAngle) > -0.2f) actions.forward = true;
        else actions.backward = true;
        
        return actions; // Dodge overrides all other logic to ensure survival
    }

    // 2. Target Selection & Aiming
    bool enemyInSight = false;
    bool hasTarget = false;
    b2Vec2 targetAim(0.0f, 0.0f);
    float enemyDist = 999.0f;

    if (enemyTank) {
        b2Vec2 enemyPos = enemyTank->body->GetPosition();
        b2Vec2 enemyVel = enemyTank->body->GetLinearVelocity();
        enemyDist = (enemyPos - myPos).Length();

        float bulletSpeed = 6.0f;
        if (myTank->currentWeapon == ItemType::GATLING) bulletSpeed = 10.0f;
        else if (myTank->currentWeapon == ItemType::MISSILE) bulletSpeed = 4.5f;
        else if (myTank->currentWeapon == ItemType::DEATH_RAY) bulletSpeed = 16.0f;

        float hitTime = SolveInterceptTime(myPos, enemyPos, enemyVel, bulletSpeed, 2.5f);
        b2Vec2 predictedPos = enemyPos + hitTime * enemyVel;

        if (CheckClearance(game->world, myPos, predictedPos)) {
            targetAim = predictedPos;
            enemyInSight = true;
            hasTarget = true;
        } else if (CheckClearance(game->world, myPos, enemyPos)) {
            targetAim = enemyPos;
            enemyInSight = true;
            hasTarget = true;
        } else if (level >= 5 && myTank->currentWeapon != ItemType::DEATH_RAY && myTank->currentWeapon != ItemType::MISSILE) {
            b2Vec2 wallPt;
            if (FindBounceShot(game, myPos, enemyTank->body, enemyPos, &wallPt)) {
                targetAim = wallPt;
                hasTarget = true;
            }
        }
    }

    // 3. Movement & Shooting
    if (hasTarget && enemyTank) {
        float aimError = NormalizeAngle(AngleTo(myPos, targetAim) - myAngle);
        
        // Aim at target
        if (aimError > 0.04f) actions.turnLeft = true;
        else if (aimError < -0.04f) actions.turnRight = true;
        
        // Optimal distance maintenance (Kiting / Chasing)
        if (enemyInSight) {
            if (enemyDist < 8.0f) {
                actions.backward = true;
            } else if (enemyDist > 16.0f) {
                if (std::abs(aimError) < 0.5f) actions.forward = true;
            } else {
                // Mid-range: just hold position or move slightly to keep momentum
                if (myVel.Length() < 0.5f && std::abs(aimError) < 0.5f) actions.forward = true;
            }
        }

        // Fire control
        bool canShoot = myTank->shootCooldownTimer <= 0.0f;
        if (canShoot) {
            float tolerance = 0.08f;
            if (enemyDist > 15.0f) tolerance = 0.04f; // tighter aim at long range
            if (enemyDist < 5.0f) tolerance = 0.2f;   // looser aim at close range
            
            if (std::abs(aimError) <= tolerance) {
                if (HasMuzzleClearance(game->world, myPos, forwardDir, 1.25f)) {
                    actions.shoot = true;
                }
            }
        }

        // Handle specific weapons like Frag
        if (myTank->currentWeapon == ItemType::FRAG && ShouldDetonateFrag(game->bullets, playerIndex, enemyTank->body->GetPosition())) {
            actions.shoot = true; 
        }

    } else if (enemyTank && game->mapEnabled) {
        // 4. Pathfinding if no target or bounce shot found
        b2Vec2 enemyPos = enemyTank->body->GetPosition();
        if (backupTimer > 0) {
            backupTimer--;
            actions.backward = true;
            return actions;
        }

        bool needRecalc = cachedPath.empty() || currentWaypointIdx >= (int)cachedPath.size();
        if (!needRecalc) {
            if ((cachedPath[currentWaypointIdx] - myPos).Length() < 1.4f) {
                currentWaypointIdx++;
                if (currentWaypointIdx >= (int)cachedPath.size()) needRecalc = true;
            }
            if (myVel.Length() < 0.25f) {
                stuckCounter++;
                if (stuckCounter > 24) {
                    backupTimer = 16;
                    cachedPath.clear();
                    stuckCounter = 0;
                    return actions;
                }
            } else {
                stuckCounter = 0;
            }
        }

        if (needRecalc || (lastEnemyPos - enemyPos).LengthSquared() > 9.0f) {
            cachedPath = game->map.GetFullPath(game->world, myPos, enemyPos, blockedCells);
            currentWaypointIdx = 1;
            stuckCounter = 0;
            lastEnemyPos = enemyPos;
        }

        game->botPaths[playerIndex] = cachedPath;

        if (currentWaypointIdx < (int)cachedPath.size()) {
            float moveAngle = NormalizeAngle(AngleTo(myPos, cachedPath[currentWaypointIdx]) - myAngle);
            if (moveAngle > 0.15f) actions.turnLeft = true;
            else if (moveAngle < -0.15f) actions.turnRight = true;
            if (std::cos(moveAngle) > 0.15f) actions.forward = true;
            else if (std::cos(moveAngle) < -0.55f) actions.backward = true;
        }
    }

    // 5. Unstuck from walls
    if (actions.turnLeft || actions.turnRight) {
        for (b2ContactEdge* edge = myTank->body->GetContactList(); edge; edge = edge->next) {
            if (edge->contact->IsTouching() && edge->other->GetType() == b2_staticBody) {
                actions.forward = false;
                break;
            }
        }
    }

    return actions;
}
