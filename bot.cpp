#include "bot.h"
#include <algorithm>
#include <cmath>

// ============================================================================
//  UTILITY FUNCTIONS (pure math — thread-safe)
// ============================================================================
namespace {

// ---- Raycast Callbacks (chỉ dùng trên Main Thread) ----

class ClosestHitCB : public b2RayCastCallback {
public:
    bool hit = false, hitStatic = false;
    b2Body* body = nullptr;
    b2Vec2 point = b2Vec2(0,0), normal = b2Vec2(0,0);
    float ReportFixture(b2Fixture* f, const b2Vec2& p, const b2Vec2& n, float fr) override {
        if (f->IsSensor()) return -1.f;
        hit = true;
        body = f->GetBody();
        hitStatic = (body->GetType() == b2_staticBody);
        point = p; normal = n;
        return fr;
    }
};

// ---- Math Utilities ----

float NormAng(float a) { while (a > PI) a -= 2*PI; while (a < -PI) a += 2*PI; return a; }
float Ang2(b2Vec2 f, b2Vec2 t) { b2Vec2 d = t - f; return atan2f(-d.x, d.y); }
b2Vec2 SafeN(b2Vec2 v) { float l = v.Length(); return l < 1e-4f ? b2Vec2(0,0) : (1.f/l) * v; }
float Dot2(b2Vec2 a, b2Vec2 b) { return a.x*b.x + a.y*b.y; }
b2Vec2 Refl(b2Vec2 i, b2Vec2 n) { return SafeN(i - 2.f * Dot2(i,n) * n); }

float BulletSpd(ItemType w) {
    switch(w) {
        case ItemType::GATLING:   return 10.f;
        case ItemType::FRAG:      return 5.f;
        case ItemType::MISSILE:   return 4.5f;
        case ItemType::DEATH_RAY: return 8.f;
        default:                  return 6.f;
    }
}

/// Whisker raycast: đo khoảng cách đến tường gần nhất theo hướng `dir`
float CastW(b2World& w, b2Vec2 from, b2Vec2 dir, float ml) {
    struct WCB : public b2RayCastCallback {
        bool h = false; float f = 1.f;
        float ReportFixture(b2Fixture* fx, const b2Vec2&, const b2Vec2&, float fr) override {
            if (fx->GetBody()->GetType() == b2_staticBody) { h = true; if (fr < f) f = fr; }
            return 1.f;
        }
    } cb;
    w.RayCast(&cb, from, from + ml * dir);
    return cb.h ? cb.f * ml : ml;
}

/// Kiểm tra nếu frag bomb nên kích nổ (gần enemy)
bool ShouldFrag(const std::vector<Bullet*>& bl, int ow, b2Vec2 ep) {
    for (Bullet* b : bl) {
        if (!b || b->ownerPlayerIndex != ow) continue;
        if (!b->isFrag || b->explodeFrag || b->time <= 0) continue;
        if ((b->body->GetPosition() - ep).Length() < 2.6f) return true;
    }
    return false;
}

/// Chỉ kiểm tra tường (bỏ qua dynamic bodies)
class WallOnlyCB : public b2RayCastCallback {
public:
    bool hitWall = false;
    float ReportFixture(b2Fixture* f, const b2Vec2&, const b2Vec2&, float fr) override {
        if (f->IsSensor()) return -1.f;
        if (f->GetBody()->GetType() == b2_staticBody) { hitWall = true; return fr; }
        return -1.f;
    }
};

/// Kiểm tra line-of-sight giữa 2 điểm (không tường chắn)
bool CanSee(b2World& w, b2Vec2 from, b2Vec2 to) {
    WallOnlyCB cb; w.RayCast(&cb, from, to); return !cb.hitWall;
}

/// Kiểm tra mũi nòng không bị chắn bởi tường
bool MuzzleClear(b2World& w, b2Vec2 bodyPos, b2Vec2 fwd) {
    b2Vec2 sp = bodyPos + (30.0f / SCALE) * fwd;
    WallOnlyCB cb; w.RayCast(&cb, bodyPos, sp); return !cb.hitWall;
}

/// Tính thời gian intercept (dự đoán vị trí enemy)
float SolveIntercept(b2Vec2 s, b2Vec2 t, b2Vec2 tv, float bs, float mx = 5.f) {
    b2Vec2 d = t - s;
    float a = Dot2(tv,tv) - bs*bs, b = 2.f * Dot2(d,tv), c = Dot2(d,d);
    float fb = std::min(mx, sqrtf(c) / std::max(bs, .1f));
    if (fabsf(a) < 1e-4f) { if (fabsf(b) < 1e-4f) return fb; float t = -c/b; return t > 0 ? std::min(t,mx) : fb; }
    float disc = b*b - 4*a*c; if (disc < 0) return fb;
    float sq = sqrtf(disc), t1 = (-b-sq)/(2*a), t2 = (-b+sq)/(2*a), r = 9999.f;
    if (t1 > 0) r = t1; if (t2 > 0) r = std::min(r, t2);
    return r > 9998.f ? fb : std::min(r, mx);
}

/// Khoảng cách từ điểm P đến đoạn thẳng AB
float SegDist(b2Vec2 P, b2Vec2 A, b2Vec2 B) {
    b2Vec2 AB = B - A; b2Vec2 AP = P - A;
    float ab2 = Dot2(AB, AB);
    if (ab2 < 1e-6f) return (P - A).Length();
    float t = Dot2(AP, AB) / ab2;
    if (t < 0.f) t = 0.f; if (t > 1.f) t = 1.f;
    b2Vec2 closest(A.x + t*AB.x, A.y + t*AB.y);
    return (P - closest).Length();
}

/// Tìm đường bắn nảy tường full 360° (180 tia × 4 bounces, bước 2°)
/// Có kiểm tra self-hit: loại bỏ đường bounce quay lại trúng chính mình
bool FindBounce(Game* g, b2Vec2 mp, b2Body* eb, b2Vec2 ep, b2Vec2& out) {
    if (!g || !eb) return false;
    const float step = 0.035f;  // ~2° per ray
    const int numRays = (int)(2.f * PI / step);
    const float selfSafe = 0.5f;  // ~15px safe radius quanh bot
    float best = 1e9f; bool found = false;
    for (int i = 0; i < numRays; i++) {
        float a = i * step;
        b2Vec2 dir(-sinf(a), cosf(a));
        b2Vec2 pos = mp; b2Vec2 d = dir; float rem = 80.f;
        b2Vec2 firstWall(0,0); bool gotWall = false, hitE = false, selfHit = false;
        for (int bounce = 0; bounce < 4 && rem > 1.0f; bounce++) {
            ClosestHitCB cb;
            g->world.RayCast(&cb, pos, pos + rem * d);
            if (!cb.hit) break;

            // Self-hit check: segment SAU bounce đầu có đi qua gần bot không?
            if (bounce > 0) {
                float distToSelf = SegDist(mp, pos, cb.point);
                if (distToSelf < selfSafe) { selfHit = true; break; }
            }

            if (!gotWall && cb.hitStatic) { firstWall = cb.point; gotWall = true; }
            if (cb.body == eb) { hitE = true; break; }
            if (!cb.hitStatic) break;
            float dist = (cb.point - pos).Length(); rem -= dist;
            d = Refl(SafeN(cb.point - pos), cb.normal);
            if (d.LengthSquared() < 0.01f) break;
            pos = cb.point + 0.05f * d;
        }
        if (hitE && gotWall && !selfHit) {
            float sc = (firstWall - mp).Length();
            if (sc < best) { best = sc; out = firstWall; found = true; }
        }
    }
    return found;
}

// ============================================================================
//  Pure Pursuit: Tìm điểm lookahead trên đường path
//
//  1. Tìm điểm gần nhất trên path (chiếu vuông góc lên từng segment)
//  2. Từ điểm đó, đi dọc path thêm `lookaheadDist`
//  3. Trả về điểm lookahead → bot lái theo hướng này
// ============================================================================
b2Vec2 FindLookaheadPoint(const std::vector<b2Vec2>& path, b2Vec2 pos, float lookaheadDist) {
    if (path.size() < 2) return path.empty() ? pos : path.back();

    // Bước 1: Tìm điểm gần nhất trên path
    float minDistSq = 1e9f;
    int   closestSeg = 0;
    float closestT   = 0.f;

    for (int i = 0; i < (int)path.size() - 1; i++) {
        b2Vec2 a = path[i], b = path[i + 1];
        b2Vec2 ab = b - a;
        float abLenSq = Dot2(ab, ab);
        if (abLenSq < 1e-6f) continue;

        float t = Dot2(pos - a, ab) / abLenSq;
        t = std::max(0.f, std::min(1.f, t));
        b2Vec2 closest = a + t * ab;
        float dSq = (pos - closest).LengthSquared();
        if (dSq < minDistSq) {
            minDistSq  = dSq;
            closestSeg = i;
            closestT   = t;
        }
    }

    // Bước 2: Đi dọc path từ điểm gần nhất, tiến lookaheadDist
    b2Vec2 segDir = path[closestSeg + 1] - path[closestSeg];
    float  segLen = segDir.Length();
    float  remaining = lookaheadDist;

    // Phần còn lại trên segment hiện tại
    float segRemaining = (1.f - closestT) * segLen;
    if (segRemaining >= remaining && segLen > 1e-4f) {
        return path[closestSeg] + (closestT + remaining / segLen) * segDir;
    }
    remaining -= segRemaining;

    // Tiến qua các segment tiếp theo
    for (int i = closestSeg + 1; i < (int)path.size() - 1; i++) {
        b2Vec2 seg = path[i + 1] - path[i];
        float sl = seg.Length();
        if (sl >= remaining && sl > 1e-4f) {
            return path[i] + (remaining / sl) * seg;
        }
        remaining -= sl;
    }

    return path.back();
}

} // namespace

// ============================================================================
//  CONSTRUCTOR / DESTRUCTOR
// ============================================================================
Bot::Bot(int level, int playerIndex) : level(level), playerIndex(playerIndex) {
    moveThread  = std::thread(&Bot::MovementThreadFunc, this);
    shootThread = std::thread(&Bot::ShootingThreadFunc, this);
}

Bot::~Bot() {
    {
        std::lock_guard<std::mutex> lk(mtx);
        shutdownFlag = true;
    }
    cvStart.notify_all();
    cvDone.notify_all();
    if (moveThread.joinable()) moveThread.join();
    if (shootThread.joinable()) shootThread.join();
}

// ============================================================================
//  COLLECT SENSOR DATA — Main Thread
//
//  Thu thập TẤT CẢ dữ liệu cần Box2D (raycasts, physics state).
//  2 worker threads CHỈ đọc kết quả, KHÔNG gọi Box2D.
//
//  Raycasts: 5 whisker + A* (CheckClearance) + 72 laser × ≤3 bounces
// ============================================================================
void Bot::CollectSensorData() {
    Game* game  = currentGame;
    Tank* me    = currentMe;
    Tank* enemy = currentEnemy;
    SensorData s;

    if (requestPathClear) {
        cachedPath.clear();
        currentWaypointIdx = 0;
        requestPathClear = false;
    }

    // ====== TANK STATE ======
    s.myPos     = me->body->GetPosition();
    s.myVel     = me->body->GetLinearVelocity();
    s.myAngle   = me->body->GetAngle();
    s.fwd       = b2Vec2(-sinf(s.myAngle), cosf(s.myAngle));
    s.enemyPos  = enemy->body->GetPosition();
    s.enemyVel  = enemy->body->GetLinearVelocity();
    s.enemyDist = (s.enemyPos - s.myPos).Length();
    s.enemyBody = enemy->body;

    // ====== WHISKER RAYCASTS (5 tia) ======
    const float wLen = 1.5f, wA = 0.52f;

    // 3 tia chính: phía trước + 2 chéo (tránh va chạm)
    s.whiskerFront = CastW(game->world, s.myPos,
        b2Vec2(-sinf(s.myAngle), cosf(s.myAngle)), wLen);
    s.whiskerLeft  = CastW(game->world, s.myPos,
        b2Vec2(-sinf(s.myAngle + wA), cosf(s.myAngle + wA)), wLen);
    s.whiskerRight = CastW(game->world, s.myPos,
        b2Vec2(-sinf(s.myAngle - wA), cosf(s.myAngle - wA)), wLen);

    // 2 tia lateral (vuông góc heading) cho center-seeking
    float perpAng = s.myAngle + PI * 0.5f;
    b2Vec2 leftDir(-sinf(perpAng), cosf(perpAng));
    s.lateralLeft  = CastW(game->world, s.myPos, leftDir, 2.0f);
    s.lateralRight = CastW(game->world, s.myPos,
        b2Vec2(-leftDir.x, -leftDir.y), 2.0f);

    // ====== A* PATHFINDING ======
    bool needRecalc = cachedPath.empty()
                   || currentWaypointIdx >= (int)cachedPath.size();
    if (!needRecalc &&
        (cachedPath[currentWaypointIdx] - s.myPos).Length() < 0.8f) {
        currentWaypointIdx++;
        if (currentWaypointIdx >= (int)cachedPath.size()) needRecalc = true;
    }
    if (needRecalc ||
        (lastEnemyPos - s.enemyPos).LengthSquared() > 12.0f) {
        cachedPath = game->map.GetFullPath(
            game->world, s.myPos, s.enemyPos, blockedCells);
        currentWaypointIdx = 1;
        lastEnemyPos = s.enemyPos;
        s.pathRecalculated = true;
    }
    game->botPaths[playerIndex] = cachedPath;

    // ====== LOOK-AHEAD WAYPOINT (fallback cho Movement) ======
    s.moveTarget = s.enemyPos;
    if (currentWaypointIdx < (int)cachedPath.size()) {
        s.moveTarget = cachedPath[currentWaypointIdx];
        int maxLA = std::min(
            (int)cachedPath.size() - 1, currentWaypointIdx + 3);
        for (int i = maxLA; i > currentWaypointIdx; i--) {
            if (CheckClearance(game->world, s.myPos, cachedPath[i])) {
                s.moveTarget = cachedPath[i];
                break;
            }
        }
    }

    // ====== LASER FAN 360° (72 tia × tối đa 2 bounces) ======
    // Mỗi tia: bắn từ trung tâm bot, trace bounce khi chạm tường.
    // Nếu tia (sau bounce) trúng enemy → Shooting Thread sẽ xoay nòng bắn.
    const float rayStep   = (2.f * PI) / BOT_LASER_RAYS;
    const float maxRayLen = 30.f;  // Box2D units (~900 pixels)

    for (int i = 0; i < BOT_LASER_RAYS; i++) {
        LaserRayResult& ray = s.laserRays[i];
        ray.angle           = i * rayStep;
        ray.numSegments     = 0;
        ray.hitEnemy        = false;
        ray.enemySegmentIdx = -1;

        b2Vec2 pos = s.myPos;
        b2Vec2 dir(-sinf(ray.angle), cosf(ray.angle));
        float  remaining = maxRayLen;

        for (int bounce = 0; bounce <= BOT_MAX_BOUNCES && remaining > 0.1f; bounce++) {
            ClosestHitCB cb;
            game->world.RayCast(&cb, pos, pos + remaining * dir);

            RaySegment& seg = ray.segments[bounce];
            seg.start = pos;

            if (!cb.hit) {
                // Tia bay vào khoảng trống (không chạm gì)
                seg.end      = pos + remaining * dir;
                seg.hitStatic = false;
                seg.hitBody  = nullptr;
                seg.normal   = b2Vec2(0,0);
                seg.length   = remaining;
                ray.numSegments = bounce + 1;
                break;
            }

            seg.end      = cb.point;
            seg.hitStatic = cb.hitStatic;
            seg.hitBody  = cb.body;
            seg.normal   = cb.normal;
            seg.length   = (cb.point - pos).Length();
            ray.numSegments = bounce + 1;

            // Trúng enemy → đánh dấu và dừng trace tia này
            if (cb.body == enemy->body) {
                ray.hitEnemy        = true;
                ray.enemySegmentIdx = bounce;
                break;
            }

            // Chạm tường → phản xạ và tiếp tục
            if (cb.hitStatic) {
                remaining -= seg.length;
                dir = Refl(SafeN(cb.point - pos), cb.normal);
                if (dir.LengthSquared() < 0.01f) break;
                pos = cb.point + 0.05f * dir;  // offset nhẹ tránh self-hit
            } else {
                // Chạm dynamic body khác (không phải enemy) → dừng
                break;
            }
        }
    }
    s.numLaserRays = BOT_LASER_RAYS;

    // ====== COMBAT CHECKS ======
    // Direct shot: kiểm tra line-of-sight
    s.clearShot = CanSee(game->world, s.myPos, s.enemyPos);

    // Bounce shot: FindBounce (80 tia tập trung × 4 bounces)
    if (me->currentWeapon != ItemType::DEATH_RAY &&
        me->currentWeapon != ItemType::MISSILE) {
        b2Vec2 bp(0,0);
        if (FindBounce(game, s.myPos, enemy->body, s.enemyPos, bp)) {
            s.hasBounce   = true;
            s.bouncePoint = bp;
        }
    }

    // ====== WEAPON INFO ======
    s.currentWeapon = me->currentWeapon;
    s.shootCooldown = me->shootCooldownTimer;
    s.activeBullets = 0;
    for (Bullet* b : game->bullets) {
        if (b && b->ownerPlayerIndex == playerIndex && b->time > 0
            && !b->isMissile && !b->isFrag)
            s.activeBullets++;
    }
    s.fragReady = (me->currentWeapon == ItemType::FRAG) &&
                  ShouldFrag(game->bullets, playerIndex, s.enemyPos);

    sensor = s;
}

// ============================================================================
//  MOVEMENT THREAD — Pure Pursuit + Center-Seeking (ưu tiên THẤP)
//
//  Điều khiển: forward/backward + turn (khi Shooting rảnh)
//  KHÔNG BAO GIỜ bắn. Ưu tiên: đi giữa lối đi, tránh tường.
//
//  Steering = Pure Pursuit angle + center-seeking + whisker repulsion
// ============================================================================
void Bot::MovementThreadFunc() {
    int myGen = 0;
    while (true) {
        {
            std::unique_lock<std::mutex> lk(mtx);
            cvStart.wait(lk, [&] { return generation > myGen || shutdownFlag; });
            if (shutdownFlag) return;
            myGen = generation;
        }

        const SensorData& s = sensor;
        MoveDecision dec;

        // ---- Pure Pursuit: tìm lookahead point trên path ----
        float baseLookahead = 1.5f;  // Box2D units
        // Adaptive: giảm lookahead gần tường → rẽ chặt hơn
        float wallProximity = std::min({s.whiskerFront, s.whiskerLeft, s.whiskerRight});
        float lookahead = baseLookahead * std::max(0.3f, wallProximity / 1.5f);

        b2Vec2 target = s.moveTarget;  // fallback
        if ((int)cachedPath.size() >= 2) {
            target = FindLookaheadPoint(cachedPath, s.myPos, lookahead);
        }

        float moveAng = NormAng(Ang2(s.myPos, target) - s.myAngle);
        float absAng  = fabsf(moveAng);

        // ---- Center-Seeking: giữ xe giữa lối đi ----
        // Chỉ kích hoạt khi CÓ tường cả 2 bên (đang trong hành lang)
        // lateralLeft gần → bot lệch trái → cần xoay phải (correction âm)
        float centerCorrection = 0.f;
        if (s.lateralLeft < 2.0f && s.lateralRight < 2.0f) {
            centerCorrection = (s.lateralLeft - s.lateralRight) * 0.3f;
        }

        // ---- Whisker Repulsion (tránh tường khẩn cấp) ----
        float rep = 0.f;
        if (s.whiskerLeft  < 0.8f) rep -= (0.8f - s.whiskerLeft)  * 2.5f;
        if (s.whiskerRight < 0.8f) rep += (0.8f - s.whiskerRight) * 2.5f;

        // ---- Kết hợp steering ----
        float steer = moveAng + centerCorrection + rep;
        dec.turnLeft  = (steer >  0.05f);
        dec.turnRight = (steer < -0.05f);

        // ---- Tiến / Lùi ----
        if (s.whiskerFront < 0.25f) {
            dec.backward = true;
        } else if (absAng > 1.8f) {
            // Hướng sai quá → xoay tại chỗ, không tiến
        } else {
            dec.forward = true;
        }

        moveOut = dec;
        {
            std::lock_guard<std::mutex> lk(mtx);
            moveGen = myGen;
        }
        cvDone.notify_one();
    }
}

// ============================================================================
//  SHOOTING THREAD — Hybrid: Laser Fan (direct) + FindBounce (bounce)
//
//  CHỈ xoay xe (overrideTurn), KHÔNG điều khiển forward/backward.
//
//  Phase 1: Laser fan → direct shot (ray trúng enemy segment 0)
//  Phase 2: CanSee + Intercept → fallback direct (nếu laser fan miss)
//  Phase 3: FindBounce → bounce shot (aim tại bouncePoint trên tường)
//  Ưu tiên: Direct > Bounce
// ============================================================================
void Bot::ShootingThreadFunc() {
    int myGen = 0;
    // ---- Lock-on: ngăn oscillation giữa 2 bounce target ----
    float lockedBounceAngle = 0.f;
    int   bounceLockFrames  = 0;

    while (true) {
        {
            std::unique_lock<std::mutex> lk(mtx);
            cvStart.wait(lk, [&] { return generation > myGen || shutdownFlag; });
            if (shutdownFlag) return;
            myGen = generation;
        }

        const SensorData& s = sensor;
        ShootDecision dec;

        // ---- Kiểm tra slot đạn ----
        bool hasBulletSlot;
        if (s.currentWeapon == ItemType::NORMAL)
            hasBulletSlot = (s.activeBullets < 2);
        else
            hasBulletSlot = (s.activeBullets == 0);
        bool canFire = hasBulletSlot && (s.shootCooldown <= 0.0f);

        // ============ PHASE 1: DIRECT SHOT — Laser Fan ============
        float bestDirectAngle = 0.f;
        float bestDirectDist  = 1e9f;
        bool  foundDirect     = false;

        for (int i = 0; i < s.numLaserRays; i++) {
            const LaserRayResult& ray = s.laserRays[i];
            if (!ray.hitEnemy || ray.enemySegmentIdx != 0) continue;
            float dist = ray.segments[0].length;
            if (dist < bestDirectDist) {
                bestDirectDist  = dist;
                bestDirectAngle = ray.angle;
                foundDirect     = true;
            }
        }

        // Fallback direct: CanSee + Intercept (nếu laser fan miss)
        if (!foundDirect && s.clearShot) {
            float bSpd = BulletSpd(s.currentWeapon);
            float t = SolveIntercept(s.myPos, s.enemyPos, s.enemyVel, bSpd);
            b2Vec2 predicted = s.enemyPos + t * s.enemyVel;
            bestDirectAngle = Ang2(s.myPos, predicted);
            bestDirectDist  = s.enemyDist;
            foundDirect     = true;
        }

        // ============ PHASE 2: BOUNCE SHOT — FindBounce + LOCK-ON ============
        float bounceAngle = 0.f;
        bool  foundBounce = false;

        if (s.hasBounce) {
            float bounceDist = (s.bouncePoint - s.myPos).Length();
            if (bounceDist >= 30.0f / SCALE) {
                bounceAngle = Ang2(s.myPos, s.bouncePoint);
                foundBounce = true;
            }
        }

        // Lock-on: commit vào 1 bounce target, không cho nhảy
        if (foundBounce) {
            if (bounceLockFrames > 0) {
                // Đang lock → giữ angle cũ, countdown
                bounceAngle = lockedBounceAngle;
                bounceLockFrames--;
            } else {
                // Hết lock / chưa lock → lock angle mới
                lockedBounceAngle = bounceAngle;
                bounceLockFrames = 60;  // ~1 giây
            }
        } else {
            bounceLockFrames = 0;  // Mất bounce → reset lock
        }

        // Direct shot phá lock (direct luôn ưu tiên hơn)
        if (foundDirect) bounceLockFrames = 0;

        // ============ CHỌN MỤC TIÊU ============
        // Direct LUÔN ưu tiên (nhanh, chính xác)
        // Bounce CHỈ dùng khi KHÔNG có direct (tường chắn)
        float targetAngle = 0.f;
        bool  hasTarget   = false;

        if (foundDirect) {
            targetAngle = bestDirectAngle;
            hasTarget   = true;
        } else if (foundBounce) {
            targetAngle = bounceAngle;
            hasTarget   = true;
        }

        // ============ QUYẾT ĐỊNH XOAY + BẮN ============
        if (hasTarget) {
            dec.hasTarget    = true;
            dec.overrideTurn = true;

            float aimErr = NormAng(targetAngle - s.myAngle);
            dec.turnLeft  = (aimErr >  0.02f);
            dec.turnRight = (aimErr < -0.02f);

            // Tolerance bắn tùy vũ khí + khoảng cách + loại shot
            float fireTol = 0.10f;
            if (s.currentWeapon == ItemType::GATLING) fireTol = 0.20f;
            if (s.enemyDist < 5.f) fireTol = 0.25f;
            if (foundBounce && !foundDirect) fireTol = 0.15f;

            if (canFire && fabsf(aimErr) <= fireTol) {
                dec.shoot = true;
                bounceLockFrames = 0;  // Bắn xong → reset lock, tìm target mới
            }
        }

        // ---- Frag detonation (luôn kích khi có thể) ----
        if (s.fragReady) dec.shoot = true;

        shootOut = dec;
        {
            std::lock_guard<std::mutex> lk(mtx);
            shootGen = myGen;
        }
        cvDone.notify_one();
    }
}

// ============================================================================
//  GetAction — ĐIỀU PHỐI + TRỌNG TÀI
//
//  Quy tắc ưu tiên:
//    forward/backward : LUÔN từ Movement
//    turn             : Shooting override khi có target, else Movement
//    shoot            : LUÔN từ Shooting
//    Emergency        : sắp đâm tường → Movement cưỡng chế phanh lùi
// ============================================================================
TankActions Bot::GetAction(Game* game) {
    TankActions act;
    if (!game) return act;

    Tank* me = nullptr; Tank* enemy = nullptr;
    for (Tank* t : game->tanks) {
        if (t->playerIndex == playerIndex) me = t;
        else if (!t->isDestroyed && !enemy) enemy = t;
    }
    if (!me || me->isDestroyed || !enemy) return act;

    // --- Lùi thoát kẹt ---
    if (backupTimer > 0) {
        backupTimer--;
        act.backward = true;
        if (backupTurnDir > 0) act.turnLeft = true;
        else                   act.turnRight = true;
        return act;
    }

    // 1. Thu thập sensor (main thread: ALL raycasts + A* + laser fan)
    currentGame  = game;
    currentMe    = me;
    currentEnemy = enemy;
    CollectSensorData();

    // 2. Kích hoạt 2 worker threads
    {
        std::lock_guard<std::mutex> lk(mtx);
        generation++;
    }
    cvStart.notify_all();

    // 3. Chờ cả 2 threads xong
    {
        std::unique_lock<std::mutex> lk(mtx);
        cvDone.wait(lk, [&] {
            return (moveGen == generation && shootGen == generation)
                || shutdownFlag;
        });
    }
    if (shutdownFlag) return act;

    // 4. Trọng tài — kết hợp Movement + Shooting
    const MoveDecision&  move  = moveOut;
    const ShootDecision& shoot = shootOut;

    // ---- Rotation-stuck detector ----
    // So sánh heading hiện tại vs 16 frames trước (không dùng frame-to-frame
    // vì Box2D vibration gây false reset)
    static float headingBuf[16] = {};
    static int   headingIdx = 0;
    bool rotationStuck = false;

    headingBuf[headingIdx & 15] = sensor.myAngle;
    headingIdx++;

    if (shoot.hasTarget && (shoot.turnLeft || shoot.turnRight) && headingIdx > 16) {
        float oldHeading = headingBuf[headingIdx & 15];  // 16 frames trước
        float headingDelta = fabsf(sensor.myAngle - oldHeading);
        if (headingDelta > PI) headingDelta = 2*PI - headingDelta;
        // Ở 3 rad/s, 16 frames = 0.8 rad kỳ vọng. Nếu < 0.15 → stuck
        if (headingDelta < 0.15f) rotationStuck = true;
    }

    // Forward/backward:
    //   Rotation stuck → DÙNG MOVEMENT di chuyển tìm chỗ thoáng
    //   Có target + quay được → ĐỨNG YÊN
    //   Không có target → di chuyển bình thường
    if (shoot.hasTarget) {
        if (rotationStuck) {
            // Kẹt cứng → movement lái xe đến giữa lối đi
            act.forward  = move.forward;
            act.backward = move.backward;
            // Vẫn để shooting xoay, nhưng movement cũng hỗ trợ di chuyển
        } else {
            act.forward  = false;
            act.backward = false;
        }
    } else {
        act.forward  = move.forward;
        act.backward = move.backward;
    }

    // Turn: Shooting override khi có target (ưu tiên CAO)
    //       Movement chỉ lái khi Shooting rảnh (ưu tiên THẤP)
    if (shoot.overrideTurn && !rotationStuck) {
        act.turnLeft  = shoot.turnLeft;
        act.turnRight = shoot.turnRight;
    } else {
        act.turnLeft  = move.turnLeft;
        act.turnRight = move.turnRight;
    }

    // Shoot: từ Shooting Thread
    act.shoot = shoot.shoot;

    // DEBUG: in trạng thái bắn mỗi 30 frame
    {
        static int dbgFrame = 0;
        dbgFrame++;
        if (dbgFrame % 30 == 0) {
            // Tính target angle (giống Shooting Thread)
            float dbgAimErr = 99.f;
            if (sensor.clearShot) {
                // Direct: aim tại enemy
                float ta = atan2f(-(sensor.enemyPos.x - sensor.myPos.x),
                                   sensor.enemyPos.y - sensor.myPos.y);
                dbgAimErr = ta - sensor.myAngle;
            } else if (sensor.hasBounce) {
                // Bounce: aim tại wall point
                float ta = atan2f(-(sensor.bouncePoint.x - sensor.myPos.x),
                                   sensor.bouncePoint.y - sensor.myPos.y);
                dbgAimErr = ta - sensor.myAngle;
            }
            while (dbgAimErr > PI) dbgAimErr -= 2*PI;
            while (dbgAimErr < -PI) dbgAimErr += 2*PI;

            printf("[Bot%d] f=%d clr=%d bnc=%d hasT=%d sht=%d dist=%.1f aim=%.3f cool=%.2f bul=%d tL=%d tR=%d\n",
                playerIndex, dbgFrame,
                sensor.clearShot ? 1 : 0,
                sensor.hasBounce ? 1 : 0,
                shoot.hasTarget ? 1 : 0,
                shoot.shoot ? 1 : 0,
                sensor.enemyDist,
                dbgAimErr,
                sensor.shootCooldown,
                sensor.activeBullets,
                act.turnLeft ? 1 : 0,
                act.turnRight ? 1 : 0);
        }
    }

    // 5. Stuck detection
    if (sensor.pathRecalculated) stuckCounter = 0;

    bool touching = false;
    for (b2ContactEdge* e = me->body->GetContactList(); e; e = e->next) {
        if (e->contact->IsTouching() &&
            e->other->GetType() == b2_staticBody) {
            touching = true; break;
        }
    }
    if (touching && sensor.myVel.Length() < 0.1f) {
        stuckCounter++;
        if (stuckCounter > 15) {
            backupTimer      = 25;
            backupTurnDir    = (rand() % 2) ? 1 : -1;
            stuckCounter     = 0;
            requestPathClear = true;
            act = TankActions();
            act.backward = true;
        }
    } else {
        stuckCounter = 0;
    }

    // Idle detection: tăng threshold vì bot cố ý đứng yên khi bắn
    if (sensor.myVel.Length() < 0.05f) {
        idleCounter++;
        if (idleCounter > 600) {  // ~10 giây thay vì 2 giây
            backupTimer      = 30;
            backupTurnDir    = (rand() % 2) ? 1 : -1;
            idleCounter      = 0;
            requestPathClear = true;
            act = TankActions();
            act.backward = true;
        }
    } else {
        idleCounter = 0;
    }

    return act;
}
