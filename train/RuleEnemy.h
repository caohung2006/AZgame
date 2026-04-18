#include "AZRandom.h"
#include "Observation.h"
#include "game.h"
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846f
#endif

/**
 * @brief Rule-based enemy variant used in Phase 3+ training.
 *        Produces a TankActions for the enemy player based on simple
 * heuristics.
 *
 * VARIANT 1 (Phase 3): Turn toward agent + advance + shoot when aligned.
 * VARIANT 2 (Phase 4): Adds bullet-dodging and item-seeking behaviour.
 */

// ── Helper: normalise angle to [-π, π] ───────────────────────────────────────
inline float NormaliseAngle(float a) {
  while (a > (float)M_PI)
    a -= 2.0f * (float)M_PI;
  while (a < -(float)M_PI)
    a += 2.0f * (float)M_PI;
  return a;
}

// ── Variant 1: basic "seek and destroy" ──────────────────────────────────────
inline TankActions GetRuleEnemyV1(const Game &game, int enemyIdx) {
  TankActions act{};
  const Tank *me = nullptr;
  const Tank *foe = nullptr;
  for (auto *t : game.tanks) {
    if (t->playerIndex == enemyIdx)
      me = t;
    else
      foe = t;
  }
  if (!me || !foe)
    return act;

  b2Vec2 myPos = me->body->GetPosition();
  b2Vec2 foePos = foe->body->GetPosition();
  float myAngle = me->body->GetAngle();

  // 1. Logic Tuần tra ngẫu nhiên (Wandering):
  //    Dùng hash vị trí hiện tại để chọn mục tiêu. Khi bot đến gần mục tiêu (<1.5m),
  //    hash sẽ thay đổi → tự động chọn mục tiêu mới.
  int cellR, cellC;
  auto rc = game.map.WorldToCell(myPos);
  cellR = rc.first;
  cellC = rc.second;
  unsigned int pSeed = (unsigned int)(cellR * 31 + cellC * 17 + enemyIdx * 7);

  // Chọn một ô lưới ngẫu nhiên (tránh sát biên)
  int targetRow = (int)(pSeed % (GameMap::ROWS - 2)) + 1;
  int targetCol = (int)((pSeed / 3) % (GameMap::COLS - 2)) + 1;
  
  // Tránh chọn chính ô hiện tại
  if (targetRow == cellR && targetCol == cellC) {
    targetRow = (targetRow + 2) % GameMap::ROWS;
    if (targetRow < 1) targetRow = 1;
  }
  b2Vec2 patrolTarget = game.map.CellToWorld(targetRow, targetCol);

  b2Vec2 wp = game.map.GetNextWaypoint(myPos, patrolTarget);
  b2Vec2 toWp = wp - myPos;

  // 2. Lái xe theo Waypoint (Steering)
  float targetAngle = std::atan2f(-toWp.x, toWp.y);
  float angleDiff = NormaliseAngle(targetAngle - myAngle);

  if (angleDiff > 0.15f)
    act.turnLeft = true;
  else if (angleDiff < -0.15f)
    act.turnRight = true;

  // Tiến lên khi còn cách mục tiêu và góc lệch chấp nhận được
  if (toWp.Length() > 0.5f && std::abs(angleDiff) < 1.2f) {
    act.forward = true;
  }

  // 3. TUYỆT ĐỐI KHÔNG BẮN (Để AI Phase 2 tập trung học dẫn đường)
  act.shoot = false;
  return act;
}

// ── Variant 2: Tay súng nghiệp dư (Dành cho Phase 3) ─────────────────────────
// ĐI TÌM Agent bằng A* (khác V1 đi tuần tra) + bắn khi thấy nhưng ngắm lệch
inline TankActions GetRuleEnemyV2(const Game &game, int enemyIdx) {
  TankActions act{};

  const Tank *me = nullptr;
  const Tank *foe = nullptr;
  for (auto *t : game.tanks) {
    if (t->playerIndex == enemyIdx) me = t;
    else foe = t;
  }
  if (!me || !foe) return act;

  b2Vec2 myPos = me->body->GetPosition();
  b2Vec2 foePos = foe->body->GetPosition();
  float myAngle = me->body->GetAngle();
  b2Vec2 toFoe = foePos - myPos;
  float dist = toFoe.Length();

  // 1. Di chuyển: Dùng A* đuổi theo Agent (khác V1 đi lang thang)
  b2Vec2 wp = game.map.GetNextWaypoint(myPos, foePos);
  b2Vec2 toWp = wp - myPos;
  float targetAngle = std::atan2f(-toWp.x, toWp.y);
  float angleDiff = NormaliseAngle(targetAngle - myAngle);

  if (angleDiff > 0.15f)
    act.turnLeft = true;
  else if (angleDiff < -0.15f)
    act.turnRight = true;

  if (toWp.Length() > 0.5f && std::abs(angleDiff) < 1.2f)
    act.forward = true;

  // 2. Bắn súng nghiệp dư: Ngắm lệch hơn V3 và chỉ bắn khi gần
  bool canSee = CheckLineOfSight(game, myPos, foePos);
  if (canSee && dist < 7.0f) {
    float aimAngle = std::atan2f(-toFoe.x, toFoe.y);
    float aimDiff = NormaliseAngle(aimAngle - myAngle);

    // Ngắm lệch hơn V3 (0.30 rad vs 0.10 rad)
    if (std::abs(aimDiff) < 0.30f) {
       act.shoot = true;
    }

    // Giới hạn 2 viên đạn
    int activeBullets = 0;
    for (auto *b : game.bullets) {
      if (b->ownerPlayerIndex == me->playerIndex) activeBullets++;
    }
    if (activeBullets >= 2) act.shoot = false;
  }

  return act;
}

// ── Variant 3: Waypoint seeker + Sniper (Stop and Shoot) + Shield ────────────
inline TankActions GetRuleEnemyV3(const Game &game, int enemyIdx) {
  TankActions act{};

  const Tank *me = nullptr;
  const Tank *foe = nullptr;
  for (auto *t : game.tanks) {
    if (t->playerIndex == enemyIdx)
      me = t;
    else
      foe = t;
  }
  if (!me || !foe)
    return act;

  b2Vec2 myPos = me->body->GetPosition();
  b2Vec2 foePos = foe->body->GetPosition();
  float myAngle = me->body->GetAngle();
  b2Vec2 toFoe = foePos - myPos;
  float dist = toFoe.Length();

  // 1. Dùng A* để tìm đường tới mục tiêu
  b2Vec2 wp = game.map.GetNextWaypoint(myPos, foePos);
  b2Vec2 toWp = wp - myPos;
  float targetAngle = std::atan2f(-toWp.x, toWp.y);
  float angleDiff = NormaliseAngle(targetAngle - myAngle);

    // 2. Lái xe theo Waypoint (Steering) với logic thoát kẹt 3 tia Radar
    float angles[3] = {0.0f, -0.5f, 0.5f}; // Thẳng, Trái 30, Phải 30
    float minFrac = 1.0f;
    for (float a : angles) {
      float rayAng = myAngle + a;
      b2Vec2 dir(-std::sin(rayAng), std::cos(rayAng));
      RadarCallback rc(me->body);
      const_cast<b2World&>(game.world).RayCast(&rc, myPos, myPos + 1.5f * dir);
      if (rc.closestFraction < minFrac)
        minFrac = rc.closestFraction;
    }

    bool tooClose = (minFrac < 0.6f); // Quá sát tường (dưới 0.9m)

    // Luôn ưu tiên xoay nòng/thân về hướng Waypoint
    if (angleDiff > 0.15f)
      act.turnLeft = true;
    else if (angleDiff < -0.15f)
      act.turnRight = true;

    // Điều khiển tiến/lùi
    if (tooClose) {
      act.forward = false; // Nhả ga để xoay tại chỗ
      if (std::abs(angleDiff) > 1.2f) {
        act.backward = true; // Chỉ lùi nếu Waypoint nằm hẳn phía sau
      }
    } else {
      if (toWp.Length() > 0.5f && std::abs(angleDiff) < 1.0f) {
        act.forward = true; // Chỉ tiến khi góc lệch không quá lớn để tránh quệt sườn
      }
    }

  // 3. Logic Ngắm bắn: Nếu thấy địch trong tầm 8m thì Dừng xe và Bắn
  bool canSee = CheckLineOfSight(game, myPos, foePos);
  if (canSee && dist < 8.0f) {
    act.forward = false; // Phanh lại để ngắm cho chuẩn

    // Random góc lệch nhẹ (-0.2 đến +0.2 rad)
    float shootError = ((float)(AZ::Rand() % 400) - 200.0f) / 1000.0f;
    float aimAngle = std::atan2f(-toFoe.x, toFoe.y) + shootError;
    float aimDiff = NormaliseAngle(aimAngle - myAngle);

    // [SỬA LỖI]: Bắt buộc quay xe hướng vào mặt địch, bỏ qua Waypoint
    act.turnLeft = false;
    act.turnRight = false;
    // [Nerf]: Giảm tốc độ quay nòng súng (chỉ quay ở 30% số frame) để Agent có
    // thời gian phản ứng
    if (AZ::Rand() % 100 < 50) {
      if (aimDiff > 0.05f)
        act.turnLeft = true;
      else if (aimDiff < -0.05f)
        act.turnRight = true;
    }

    // Bắt buộc phải ngắm thật chuẩn (< 0.1 rad) mới được bóp cò
    if (std::abs(aimDiff) < 0.1f)
      act.shoot = true;

    // Giới hạn RuleV3 chỉ bắn tối đa 2 viên đạn bất kể loại nào để Agent đỡ "ngợp"
    int myID = me->playerIndex;
    int activeBullets = 0;
    for (auto *b : game.bullets) {
      if (b->ownerPlayerIndex == myID)
        activeBullets++;
    }
    if (activeBullets >= 2)
      act.shoot = false;
  }

  // 4. Logic Khiên: Phản xạ khi có đạn bay tới (DÀNH CHO PHASE 4+)
  if (me->shieldCooldownTimer <= 0.0f) {
    for (auto *b : game.bullets) {
      if (b->ownerPlayerIndex == me->playerIndex)
        continue;
      b2Vec2 bulletPos = b->body->GetPosition();
      b2Vec2 toBullet = bulletPos - myPos;
      if (toBullet.Length() < 2.5f) {
        b2Vec2 relVel = b->body->GetLinearVelocity();
        if (b2Dot(relVel, toBullet) < 0) { // Đạn đang bay về phía mình
          act.shield = true;
          break;
        }
      }
    }
  }

  return act;
}

// ── Dispatcher
// ────────────────────────────────────────────────────────────────
enum class RuleVariant { V1, V2, V3 };

inline TankActions GetRuleEnemyAction(const Game &game, int enemyIdx,
                                      RuleVariant variant = RuleVariant::V1) {
  if (variant == RuleVariant::V3)
    return GetRuleEnemyV3(game, enemyIdx);
  return (variant == RuleVariant::V2) ? GetRuleEnemyV2(game, enemyIdx)
                                      : GetRuleEnemyV1(game, enemyIdx);
}
