#include "bot.h"
#include <algorithm>
#include <cmath>

namespace {

struct RayHitInfo {
    bool hit = false, hitStatic = false;
    b2Body* body = nullptr;
    b2Vec2 point = b2Vec2(0,0), normal = b2Vec2(0,0);
};

class ClosestHitCB : public b2RayCastCallback {
public:
    RayHitInfo info;
    float ReportFixture(b2Fixture* f, const b2Vec2& p, const b2Vec2& n, float fr) override {
        if (f->IsSensor()) return -1.f;
        info.hit = true;
        info.body = f->GetBody();
        info.hitStatic = (info.body->GetType() == b2_staticBody);
        info.point = p; info.normal = n;
        return fr;
    }
};

class WallOnlyCB : public b2RayCastCallback {
public:
    bool hitWall = false;
    float ReportFixture(b2Fixture* f, const b2Vec2&, const b2Vec2&, float fr) override {
        if (f->IsSensor()) return -1.f;
        if (f->GetBody()->GetType() == b2_staticBody) { hitWall = true; return fr; }
        return -1.f;
    }
};

float NormAng(float a) { while (a > PI) a -= 2*PI; while (a < -PI) a += 2*PI; return a; }
float Ang2(b2Vec2 f, b2Vec2 t) { b2Vec2 d = t - f; return atan2f(-d.x, d.y); }
b2Vec2 SafeN(b2Vec2 v) { float l = v.Length(); return l < 1e-4f ? b2Vec2(0,0) : (1.f/l) * v; }
float Dot2(b2Vec2 a, b2Vec2 b) { return a.x*b.x + a.y*b.y; }
b2Vec2 Refl(b2Vec2 i, b2Vec2 n) { return SafeN(i - 2.f * Dot2(i,n) * n); }

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

float BulletSpd(ItemType w) {
    switch(w) {
        case ItemType::GATLING:   return 10.f;
        case ItemType::FRAG:      return 5.f;
        case ItemType::MISSILE:   return 4.5f;
        case ItemType::DEATH_RAY: return 8.f;
        default:                  return 6.f;
    }
}

bool CanSee(b2World& w, b2Vec2 from, b2Vec2 to) {
    WallOnlyCB cb; w.RayCast(&cb, from, to); return !cb.hitWall;
}

bool MuzzleClear(b2World& w, b2Vec2 bodyPos, b2Vec2 fwd) {
    b2Vec2 sp = bodyPos + (30.0f / SCALE) * fwd;
    WallOnlyCB cb; w.RayCast(&cb, bodyPos, sp); return !cb.hitWall;
}

bool FindBounce(Game* g, b2Vec2 mp, b2Body* mb, b2Body* eb, b2Vec2 ep, b2Vec2& out) {
    if (!g || !eb) return false;
    float ba = Ang2(mp, ep); float best = 1e9f; bool found = false;
    for (int i = -40; i <= 40; i++) {
        float a = ba + i * 0.065f;
        b2Vec2 dir(-sinf(a), cosf(a));
        b2Vec2 pos = mp; b2Vec2 d = dir; float rem = 80.f;
        b2Vec2 firstWall(0,0); bool gotWall = false, hitE = false;
        for (int bounce = 0; bounce < 4 && rem > 1.0f; bounce++) {
            ClosestHitCB cb; g->world.RayCast(&cb, pos, pos + rem * d);
            if (!cb.info.hit) break;
            if (!gotWall && cb.info.hitStatic) { firstWall = cb.info.point; gotWall = true; }
            if (cb.info.body == eb) { hitE = true; break; }
            if (!cb.info.hitStatic) break;
            float dist = (cb.info.point - pos).Length(); rem -= dist;
            d = Refl(SafeN(cb.info.point - pos), cb.info.normal);
            if (d.LengthSquared() < 0.01f) break;
            pos = cb.info.point + 0.05f * d;
        }
        if (hitE && gotWall) {
            float sc = (firstWall - mp).Length();
            if (sc < best) { best = sc; out = firstWall; found = true; }
        }
    }
    return found;
}

bool ShouldFrag(const std::vector<Bullet*>& bl, int ow, b2Vec2 ep) {
    for (Bullet* b : bl) {
        if (!b || b->ownerPlayerIndex != ow) continue;
        if (!b->isFrag || b->explodeFrag || b->time <= 0) continue;
        if ((b->body->GetPosition() - ep).Length() < 2.6f) return true;
    }
    return false;
}

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

} // namespace

Bot::Bot(int level, int playerIndex) : level(level), playerIndex(playerIndex) {}

TankActions Bot::GetAction(Game* game) {
    TankActions act;
    if (!game) return act;

    Tank* me = nullptr; Tank* enemy = nullptr;
    for (Tank* t : game->tanks) {
        if (t->playerIndex == playerIndex) me = t;
        else if (!t->isDestroyed && !enemy) enemy = t;
    }
    if (!me || me->isDestroyed || !enemy) return act;

    const b2Vec2 myPos = me->body->GetPosition();
    const b2Vec2 myVel = me->body->GetLinearVelocity();
    const float  myAng = me->body->GetAngle();
    const b2Vec2 fwd(-sinf(myAng), cosf(myAng));
    b2Vec2 ePos = enemy->body->GetPosition();
    b2Vec2 eVel = enemy->body->GetLinearVelocity();
    float  eDist = (ePos - myPos).Length();

    if (level == 1) {
        if (rand()%100 < 5) act.forward = true;
        if (rand()%100 < 5) act.turnLeft = true;
        return act;
    }

    // --- Lùi thoát kẹt ---
    if (backupTimer > 0) {
        backupTimer--;
        act.backward = true;
        if (backupTurnDir > 0) act.turnLeft = true; else act.turnRight = true;
        return act;
    }

    // ==================================================================
    //  STEP 1: TÍNH ĐƯỜNG A*
    // ==================================================================
    bool needRecalc = cachedPath.empty() || currentWaypointIdx >= (int)cachedPath.size();
    if (!needRecalc && (cachedPath[currentWaypointIdx] - myPos).Length() < 0.8f) {
        currentWaypointIdx++;
        if (currentWaypointIdx >= (int)cachedPath.size()) needRecalc = true;
    }
    if (needRecalc || (lastEnemyPos - ePos).LengthSquared() > 12.0f) {
        cachedPath = game->map.GetFullPath(game->world, myPos, ePos, blockedCells);
        currentWaypointIdx = 1;
        stuckCounter = 0;
        lastEnemyPos = ePos;
    }
    game->botPaths[playerIndex] = cachedPath;

    // ==================================================================
    //  STEP 2: LUÔN DI CHUYỂN (không bao giờ dừng)
    // ==================================================================
    b2Vec2 moveTarget = ePos;
    if (currentWaypointIdx < (int)cachedPath.size()) {
        moveTarget = cachedPath[currentWaypointIdx];
        int maxLA = std::min((int)cachedPath.size()-1, currentWaypointIdx + 2);
        for (int i = maxLA; i > currentWaypointIdx; i--) {
            if (CheckClearance(game->world, myPos, cachedPath[i])) {
                moveTarget = cachedPath[i]; break;
            }
        }
    }

    float moveAng = NormAng(Ang2(myPos, moveTarget) - myAng);
    float absAng  = fabsf(moveAng);

    // Whisker steering
    const float wLen = 1.5f, wA = 0.52f;
    float distF = CastW(game->world, myPos, b2Vec2(-sinf(myAng), cosf(myAng)), wLen);
    float distL = CastW(game->world, myPos, b2Vec2(-sinf(myAng+wA), cosf(myAng+wA)), wLen);
    float distR = CastW(game->world, myPos, b2Vec2(-sinf(myAng-wA), cosf(myAng-wA)), wLen);

    float rep = 0.f;
    if (distL < 1.0f) rep -= (1.0f - distL) * 2.5f;
    if (distR < 1.0f) rep += (1.0f - distR) * 2.5f;

    float steer = moveAng + rep;
    act.turnLeft  = (steer >  0.06f);
    act.turnRight = (steer < -0.06f);

    if (distF < 0.4f) {
        act.backward = true;
    } else if (absAng > 1.2f) {
        // Xoay tại chỗ khi rẽ 90° — NHƯNG vẫn có turnLeft/Right ở trên
    } else {
        act.forward = true;
    }

    // ==================================================================
    //  STEP 3: BẮN (layer trên di chuyển — KHÔNG thay đổi forward/backward)
    // ==================================================================
    bool canSeeEnemy = CanSee(game->world, myPos, ePos);

    // Bắn 1 viên 1
    int myActiveBullets = 0;
    for (Bullet* b : game->bullets) {
        if (b && b->ownerPlayerIndex == playerIndex && b->time > 0) myActiveBullets++;
    }
    bool canFire = (myActiveBullets == 0) && (me->shootCooldownTimer <= 0.0f);

    // --- Quyết định mode bắn (1 lần mỗi viên đạn) ---
    if (shotModeTimer > 0) {
        shotModeTimer--;
    } else {
        // Tỉ lệ direct vs bounce theo khoảng cách
        //  < 5 units:  90% direct, 10% bounce
        //  5-10:       60% direct, 40% bounce
        // 10-18:       30% direct, 70% bounce
        //  > 18:       10% direct, 90% bounce
        int directPct;
        if      (eDist < 5.f)  directPct = 90;
        else if (eDist < 10.f) directPct = 60;
        else if (eDist < 18.f) directPct = 30;
        else                    directPct = 10;

        shotMode = ((rand() % 100) < directPct) ? 0 : 1;
        shotModeTimer = 60;  // Giữ mode ổn định 1 giây (60 frames)
    }

    // --- Tìm bounce point (dùng cho cả 2 mode nếu cần) ---
    b2Vec2 bounceWp(0,0);
    bool hasBounce = false;
    if (me->currentWeapon != ItemType::DEATH_RAY && me->currentWeapon != ItemType::MISSILE) {
        hasBounce = FindBounce(game, myPos, me->body, enemy->body, ePos, bounceWp);
    }

    if (shotMode == 0 && canSeeEnemy) {
        // ============================================
        //  MODE DIRECT: Bắn thẳng (override turn)
        // ============================================
        float bSpd = BulletSpd(me->currentWeapon);
        float t = SolveIntercept(myPos, ePos, eVel, bSpd);
        b2Vec2 predicted = ePos + t * eVel;
        b2Vec2 aimPt = CanSee(game->world, myPos, predicted) ? predicted : ePos;
        float aimErr = NormAng(Ang2(myPos, aimPt) - myAng);

        // Override turn để nhắm vào địch
        act.turnLeft  = (aimErr >  0.02f);
        act.turnRight = (aimErr < -0.02f);



        // Bắn 1 viên
        if (canFire) {
            float tol;
            if      (eDist > 15.f) tol = 0.04f;
            else if (eDist >  8.f) tol = 0.08f;
            else if (eDist >  4.f) tol = 0.15f;
            else                    tol = 0.30f;
            if (me->currentWeapon == ItemType::GATLING) tol *= 2.0f;

            if (fabsf(aimErr) <= tol && MuzzleClear(game->world, myPos, fwd))
                act.shoot = true;
        }
    }
    else if (shotMode == 0 && !canSeeEnemy && hasBounce) {
        // Direct mode nhưng không thấy địch → fallback sang bounce
        float bErr = NormAng(Ang2(myPos, bounceWp) - myAng);
        if (canFire && fabsf(bErr) < 0.08f && MuzzleClear(game->world, myPos, fwd))
            act.shoot = true;
    }
    else if (shotMode == 1 && hasBounce) {
        // ============================================
        //  MODE BOUNCE: Bắn nảy tường (giữ navigation turn)
        //  Không override turn — bắn khi nòng quét qua đúng hướng
        // ============================================
        float bErr = NormAng(Ang2(myPos, bounceWp) - myAng);
        if (canFire && fabsf(bErr) < 0.08f && MuzzleClear(game->world, myPos, fwd))
            act.shoot = true;
    }
    else if (shotMode == 1 && !hasBounce && canSeeEnemy) {
        // Bounce mode nhưng không tìm được bounce → fallback sang direct
        float bSpd = BulletSpd(me->currentWeapon);
        float t = SolveIntercept(myPos, ePos, eVel, bSpd);
        b2Vec2 predicted = ePos + t * eVel;
        b2Vec2 aimPt = CanSee(game->world, myPos, predicted) ? predicted : ePos;
        float aimErr = NormAng(Ang2(myPos, aimPt) - myAng);

        act.turnLeft  = (aimErr >  0.02f);
        act.turnRight = (aimErr < -0.02f);
        if (eDist < 4.0f) { act.forward = false; act.backward = true; }

        if (canFire) {
            float tol;
            if      (eDist > 15.f) tol = 0.04f;
            else if (eDist >  8.f) tol = 0.08f;
            else if (eDist >  4.f) tol = 0.15f;
            else                    tol = 0.30f;
            if (me->currentWeapon == ItemType::GATLING) tol *= 2.0f;
            if (fabsf(aimErr) <= tol && MuzzleClear(game->world, myPos, fwd))
                act.shoot = true;
        }
    }

    // Frag
    if (me->currentWeapon == ItemType::FRAG && ShouldFrag(game->bullets, playerIndex, ePos))
        act.shoot = true;

    // ==================================================================
    //  STEP 4: PHÁT HIỆN KẸT
    // ==================================================================
    bool touching = false;
    for (b2ContactEdge* e = me->body->GetContactList(); e; e = e->next)
        if (e->contact->IsTouching() && e->other->GetType() == b2_staticBody)
            { touching = true; break; }

    if (touching && myVel.Length() < 0.1f) {
        stuckCounter++;
        if (stuckCounter > 15) {
            backupTimer = 25;
            backupTurnDir = (rand() % 2) ? 1 : -1;
            stuckCounter = 0;
            cachedPath.clear();
            act = TankActions();
            act.backward = true;
        }
    } else stuckCounter = 0;

    return act;
}
