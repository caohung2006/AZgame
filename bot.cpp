#include "bot.h"
#include <algorithm>
#include <cmath>

namespace {

// --- Raycast helpers ---
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
        info.point = p;
        info.normal = n;
        return fr;
    }
};

// Raycast chỉ tìm tường (bỏ qua mọi thứ khác)
class WallOnlyCB : public b2RayCastCallback {
public:
    bool hitWall = false;
    float ReportFixture(b2Fixture* f, const b2Vec2&, const b2Vec2&, float fr) override {
        if (f->IsSensor()) return -1.f;
        if (f->GetBody()->GetType() == b2_staticBody) { hitWall = true; return fr; }
        return -1.f;
    }
};

// --- Toán ---
float NormAng(float a) { while (a > PI) a -= 2*PI; while (a < -PI) a += 2*PI; return a; }
float Ang2(b2Vec2 f, b2Vec2 t) { b2Vec2 d = t - f; return atan2f(-d.x, d.y); }
b2Vec2 SafeN(b2Vec2 v) { float l = v.Length(); return l < 1e-4f ? b2Vec2(0,0) : (1.f/l) * v; }
float Dot2(b2Vec2 a, b2Vec2 b) { return a.x*b.x + a.y*b.y; }
b2Vec2 Refl(b2Vec2 i, b2Vec2 n) { return SafeN(i - 2.f * Dot2(i,n) * n); }

// --- Intercept ---
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

// --- Kiểm tra tầm nhìn: 1 tia duy nhất (nhanh, dùng cho visibility) ---
bool CanSee(b2World& w, b2Vec2 from, b2Vec2 to) {
    WallOnlyCB cb;
    w.RayCast(&cb, from, to);
    return !cb.hitWall;
}

// --- Kiểm tra nòng súng có bị chặn không ---
// Giống hệt tank.cpp: raycast từ body center đến spawn pos (1.0 unit)
bool MuzzleClear(b2World& w, b2Vec2 bodyPos, b2Vec2 fwd) {
    b2Vec2 spawnPos = bodyPos + (30.0f / SCALE) * fwd;
    WallOnlyCB cb;
    w.RayCast(&cb, bodyPos, spawnPos);
    return !cb.hitWall;
}

// --- Bắn nảy tường (2 bounce max) ---
bool FindBounce(Game* g, b2Vec2 mp, b2Body* mb, b2Body* eb, b2Vec2 ep, b2Vec2& out) {
    if (!g || !eb) return false;
    float ba = Ang2(mp, ep);
    float best = 1e9f;
    bool found = false;

    for (int i = -40; i <= 40; i++) {
        float a = ba + i * 0.065f;
        b2Vec2 d(-sinf(a), cosf(a));
        ClosestHitCB c1;
        g->world.RayCast(&c1, mp, mp + 80.f * d);
        if (!c1.info.hit || !c1.info.hitStatic) continue;

        b2Vec2 r1 = Refl(SafeN(c1.info.point - mp), c1.info.normal);
        if (r1.LengthSquared() < .01f) continue;
        b2Vec2 s1 = c1.info.point + .05f * r1;
        float rm = 80.f - (c1.info.point - mp).Length();
        if (rm < 1.f) continue;

        ClosestHitCB c2;
        g->world.RayCast(&c2, s1, s1 + rm * r1);
        if (c2.info.hit && c2.info.body == eb) {
            float sc = (c1.info.point - mp).Length();
            if (sc < best) { best = sc; out = c1.info.point; found = true; }
            continue;
        }

        if (c2.info.hit && c2.info.hitStatic) {
            b2Vec2 r2 = Refl(SafeN(c2.info.point - s1), c2.info.normal);
            if (r2.LengthSquared() < .01f) continue;
            b2Vec2 s2 = c2.info.point + .05f * r2;
            float rm2 = rm - (c2.info.point - s1).Length();
            if (rm2 < 1.f) continue;
            ClosestHitCB c3;
            g->world.RayCast(&c3, s2, s2 + rm2 * r2);
            if (c3.info.hit && c3.info.body == eb) {
                float sc = (c1.info.point - mp).Length() * 1.4f;
                if (sc < best) { best = sc; out = c1.info.point; found = true; }
            }
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

// --- Whisker cast ---
float CastW(b2World& w, b2Vec2 from, b2Vec2 dir, float maxLen) {
    struct WCB : public b2RayCastCallback {
        bool h = false; float f = 1.f;
        float ReportFixture(b2Fixture* fx, const b2Vec2&, const b2Vec2&, float fr) override {
            if (fx->GetBody()->GetType() == b2_staticBody) { h = true; if (fr < f) f = fr; }
            return 1.f;
        }
    } cb;
    w.RayCast(&cb, from, from + maxLen * dir);
    return cb.h ? cb.f * maxLen : maxLen;
}

} // namespace

// ============================================================================
Bot::Bot(int level, int playerIndex) : level(level), playerIndex(playerIndex) {}

TankActions Bot::GetAction(Game* game) {
    TankActions act;
    if (!game) return act;

    Tank* me = nullptr;
    Tank* enemy = nullptr;
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
    //  STEP 2: KIỂM TRA THẤY ĐỊCH
    // ==================================================================
    // Dùng 1 tia mỏng (không fat ray) → chính xác hơn trong mê cung
    bool canSeeEnemy = CanSee(game->world, myPos, ePos);

    // ==================================================================
    //  STEP 3: DI CHUYỂN + BẮN
    // ==================================================================
    if (canSeeEnemy) {
        // ============================
        //  THẤY ĐỊCH → chiến đấu
        // ============================

        // Tính điểm ngắm đón đầu
        float bSpd = BulletSpd(me->currentWeapon);
        float t = SolveIntercept(myPos, ePos, eVel, bSpd);
        b2Vec2 predicted = ePos + t * eVel;
        b2Vec2 aimPt = CanSee(game->world, myPos, predicted) ? predicted : ePos;
        float aimErr = NormAng(Ang2(myPos, aimPt) - myAng);

        // Quay ngắm vào địch
        if (aimErr >  0.02f) act.turnLeft  = true;
        if (aimErr < -0.02f) act.turnRight = true;

        // Di chuyển: LUÔN di chuyển, không bao giờ đứng yên
        if (eDist < 4.0f) {
            act.backward = true;                    // Quá gần → lùi
        } else {
            act.forward = true;                      // Tiến đến gần
        }

        // BẮN — chỉ cần nòng thoáng (giống tank.cpp), KHÔNG check bounce
        if (me->shootCooldownTimer <= 0.0f) {
            float tol;
            if      (eDist > 18.f) tol = 0.05f;
            else if (eDist > 10.f) tol = 0.10f;
            else if (eDist >  5.f) tol = 0.15f;
            else                    tol = 0.30f;
            if (me->currentWeapon == ItemType::GATLING) tol *= 2.0f;

            if (fabsf(aimErr) <= tol) {
                // Chỉ check nòng thoáng — giống hệt tank.cpp line 104-107
                if (MuzzleClear(game->world, myPos, fwd))
                    act.shoot = true;
            }
        }

        // Frag: kích nổ khi gần địch
        if (me->currentWeapon == ItemType::FRAG && ShouldFrag(game->bullets, playerIndex, ePos))
            act.shoot = true;
    }
    else {
        // ============================
        //  KHÔNG THẤY ĐỊCH → navigate
        // ============================

        // Chọn waypoint
        b2Vec2 moveTarget = ePos;
        if (currentWaypointIdx < (int)cachedPath.size()) {
            moveTarget = cachedPath[currentWaypointIdx];
            int maxLA = std::min((int)cachedPath.size() - 1, currentWaypointIdx + 2);
            for (int i = maxLA; i > currentWaypointIdx; i--) {
                if (CheckClearance(game->world, myPos, cachedPath[i])) {
                    moveTarget = cachedPath[i];
                    break;
                }
            }
        }

        float moveAng = NormAng(Ang2(myPos, moveTarget) - myAng);
        float absAng  = fabsf(moveAng);

        // Whisker steering — giữ giữa đường
        const float wLen = 1.5f, wA = 0.52f;
        float distF = CastW(game->world, myPos, b2Vec2(-sinf(myAng), cosf(myAng)), wLen);
        float distL = CastW(game->world, myPos, b2Vec2(-sinf(myAng+wA), cosf(myAng+wA)), wLen);
        float distR = CastW(game->world, myPos, b2Vec2(-sinf(myAng-wA), cosf(myAng-wA)), wLen);

        float rep = 0.f;
        if (distL < 1.0f) rep -= (1.0f - distL) * 2.5f;
        if (distR < 1.0f) rep += (1.0f - distR) * 2.5f;

        float steer = moveAng + rep;
        if (steer >  0.06f) act.turnLeft  = true;
        if (steer < -0.06f) act.turnRight = true;

        // Tiến/lùi
        if (distF < 0.4f) {
            act.backward = true;
        } else if (absAng > 1.2f) {
            // Rẽ góc lớn → xoay tại chỗ
        } else {
            act.forward = true;
        }

        // Thử bắn nảy tường
        if (me->shootCooldownTimer <= 0.0f &&
            me->currentWeapon != ItemType::DEATH_RAY &&
            me->currentWeapon != ItemType::MISSILE) {
            b2Vec2 wp;
            if (FindBounce(game, myPos, me->body, enemy->body, ePos, wp)) {
                float bErr = NormAng(Ang2(myPos, wp) - myAng);
                if (fabsf(bErr) < 0.10f && MuzzleClear(game->world, myPos, fwd))
                    act.shoot = true;
            }
        }

        // Frag
        if (me->currentWeapon == ItemType::FRAG && ShouldFrag(game->bullets, playerIndex, ePos))
            act.shoot = true;
    }

    // ==================================================================
    //  STEP 4: PHÁT HIỆN KẸT
    // ==================================================================
    bool touching = false;
    for (b2ContactEdge* e = me->body->GetContactList(); e; e = e->next)
        if (e->contact->IsTouching() && e->other->GetType() == b2_staticBody) {
            touching = true; break;
        }

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
    } else {
        stuckCounter = 0;
    }

    return act;
}
