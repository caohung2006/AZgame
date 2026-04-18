#pragma once
#include "game.h"
#include <vector>
#include <cmath>
#include <algorithm>

/**
 * @brief Raycast callback to find distance to walls.
 */
class RadarCallback : public b2RayCastCallback {
public:
    b2Body* ignoreBody;
    float closestFraction = 1.0f;
    
    RadarCallback(b2Body* ignore) : ignoreBody(ignore) {}
    
    float ReportFixture(b2Fixture* fixture, const b2Vec2& point, const b2Vec2& normal, float fraction) override {
        if (fixture->IsSensor()) return -1.0f;
        // [VÁ LỖI TỰ KỶ]: Phải bỏ qua body của chính bản thân chiếc xe tăng
        // Nếu không, tia Radar số 0 (chính diện) sẽ luôn bắn trúng nòng súng của nó!
        if (fixture->GetBody() == ignoreBody) return -1.0f;
        
        closestFraction = fraction;
        return fraction;
    }
};

/**
 * @brief Raycast callback to check line of sight.
 */
class LOSCallback : public b2RayCastCallback {
public:
    bool hitWall = false;
    float ReportFixture(b2Fixture* fixture, const b2Vec2& point, const b2Vec2& normal, float fraction) override {
        if (fixture->GetBody()->GetType() == b2_staticBody) {
            hitWall = true;
            return fraction;
        }
        return -1.0f;
    }
};

inline bool CheckLineOfSight(const Game& game, b2Vec2 p1, b2Vec2 p2) {
    LOSCallback callback;
    const_cast<b2World&>(game.world).RayCast(&callback, p1, p2);
    return !callback.hitWall;
}

/**
 * @brief Populate 36-input observation vector.
 *
 * Layout:
 *   [0-3]   Agent State (pos, cos, sin)
 *   [4-6]   A* Navigation (local dir, distance)
 *   [7-12]  Enemy Info (pos, LOS, velocity)
 *   [13-20] Bullet Danger (top 2, unified, local frame)
 *   [21-23] Resources (ammo, shield, shield cooldown)
 *   [24-30] Radar (7 rays)
 *   [31]    Angular Velocity
 *   [32]    Enemy Ammo
 *   [33-34] Agent Linear Velocity (local frame)
 *   [35]    Shoot Cooldown
 */
inline void GetObservation(const Game &game, int agentIdx, b2Vec2 astarWaypoint,
                            std::vector<float> &obs) {
  obs.assign(36, 0.0f);
  const Tank *agent = nullptr;
  const Tank *enemy = nullptr;
  for (auto *t : game.tanks) {
    if (t->playerIndex == agentIdx) agent = t;
    else enemy = t;
  }
  if (!agent) return;

  b2Vec2 aPos = agent->body->GetPosition();
  float aAngle = agent->body->GetAngle();
  float aCos = std::cos(aAngle);
  float aSin = std::sin(aAngle);
  b2Vec2 aVel = agent->body->GetLinearVelocity();

  const float maxSpeed = 3.0f;
  const float worldW = (float)SCREEN_WIDTH / SCALE;
  const float worldH = (float)SCREEN_HEIGHT / SCALE;
  const float maxDist = std::sqrt(worldW * worldW + worldH * worldH);
  const float MAX_ACTIVE_BULLETS = 5.0f;

  // ── [0-3] Agent State ──
  obs[0] = aPos.x / worldW;
  obs[1] = aPos.y / worldH;
  obs[2] = aCos;
  obs[3] = aSin;

  // ── [4-6] A* Navigation (Local Frame) ──
  b2Vec2 toWp = astarWaypoint - aPos;
  float dWp = toWp.Length();
  if (dWp > 1e-4f) {
    obs[4] = (toWp.x * aCos + toWp.y * aSin) / dWp;
    obs[5] = (-toWp.x * aSin + toWp.y * aCos) / dWp;
  }
  obs[6] = std::clamp(dWp / 5.0f, 0.0f, 1.0f);

  // ── [7-12] Enemy Info ──
  if (enemy) {
    b2Vec2 ePos = enemy->body->GetPosition();
    b2Vec2 toE = ePos - aPos;
    float dE = toE.Length();
    if (dE > 1e-4f) {
      obs[7] = (toE.x * aCos + toE.y * aSin) / maxDist;
      obs[8] = (-toE.x * aSin + toE.y * aCos) / maxDist;
    }
    obs[9] = std::clamp(dE / maxDist, 0.0f, 1.0f);
    obs[10] = CheckLineOfSight(game, aPos, ePos) ? 1.0f : 0.0f;
    b2Vec2 eVel = enemy->body->GetLinearVelocity();
    obs[11] = std::clamp((eVel.x * aCos + eVel.y * aSin) / maxSpeed, -1.0f, 1.0f);
    obs[12] = std::clamp((-eVel.x * aSin + eVel.y * aCos) / maxSpeed, -1.0f, 1.0f);
  }

  // ── [13-20] Bullet Danger: Top 2, ưu tiên đạn đang bay đến ──
  {
    struct BD { float dist; b2Vec2 pos; b2Vec2 vel; bool approaching; };
    std::vector<BD> dangers;
    for (auto *b : game.bullets) {
      // KHÔNG lọc đạn của mình: Đạn bounce (restitution=1.0) có thể bật nảy
      // từ tường và giết chính chủ. Agent cần thấy TẤT CẢ đạn để né.
      b2Vec2 bPos = b->body->GetPosition();
      b2Vec2 toB = bPos - aPos;
      float d = toB.Length();
      if (d < 8.0f) {
        b2Vec2 relVel = b->body->GetLinearVelocity() - aVel;
        bool appr = (b2Dot(relVel, toB) < 0) || (d < 2.0f);
        dangers.push_back({d, bPos, b->body->GetLinearVelocity(), appr});
      }
    }
    std::sort(dangers.begin(), dangers.end(), [](const BD &a, const BD &b) {
      if (a.approaching != b.approaching) return a.approaching > b.approaching;
      return a.dist < b.dist;
    });
    for (int i = 0; i < 2; i++) {
      int base = 13 + i * 4;
      if (i < (int)dangers.size()) {
        b2Vec2 bd = dangers[i].pos - aPos;
        b2Vec2 bv = dangers[i].vel;
        obs[base + 0] = std::clamp((bd.x * aCos + bd.y * aSin) / 8.0f, -1.0f, 1.0f);
        obs[base + 1] = std::clamp((-bd.x * aSin + bd.y * aCos) / 8.0f, -1.0f, 1.0f);
        obs[base + 2] = std::clamp((bv.x * aCos + bv.y * aSin) / 15.0f, -1.0f, 1.0f);
        obs[base + 3] = std::clamp((-bv.x * aSin + bv.y * aCos) / 15.0f, -1.0f, 1.0f);
      }
    }
  }

  // ── [21-23] Resources ──
  {
    int activeBullets = 0;
    for (auto *b : game.bullets) {
      if (b->ownerPlayerIndex == agentIdx && !b->isMissile && !b->isFrag)
        activeBullets++;
    }
    obs[21] = std::clamp(1.0f - (activeBullets / MAX_ACTIVE_BULLETS), 0.0f, 1.0f);
  }
  obs[22] = agent->hasShield ? 1.0f : 0.0f;
  obs[23] = std::clamp(1.0f - (agent->shieldCooldownTimer / 15.0f), 0.0f, 1.0f);

  // ── [24-30] Radar (7 tia BỐ TRÍ PHI TUYẾN TÍNH) ──
  // 3 tia trước (lách mê cung) + 2 sườn (canh tường) + 2 sau (lùi xe)
  const float RAY_ANGLES[7] = {
      0.0f,                          // Tia 0: Chính diện mũi xe
      -30.0f * 3.14159f / 180.0f,    // Tia 1: Chéo trái trước
       30.0f * 3.14159f / 180.0f,    // Tia 2: Chéo phải trước
      -90.0f * 3.14159f / 180.0f,    // Tia 3: Sườn trái
       90.0f * 3.14159f / 180.0f,    // Tia 4: Sườn phải
      -150.0f * 3.14159f / 180.0f,   // Tia 5: Sau lưng trái
       150.0f * 3.14159f / 180.0f    // Tia 6: Sau lưng phải
  };
  for (int i = 0; i < 7; i++) {
    float rayAngle = aAngle + RAY_ANGLES[i];
    b2Vec2 dir(-std::sin(rayAngle), std::cos(rayAngle));
    b2Vec2 p2 = aPos + 10.0f * dir;
    RadarCallback rc(agent->body);
    const_cast<b2World&>(game.world).RayCast(&rc, aPos, p2);
    obs[24 + i] = rc.closestFraction;
  }

  // ── [31] Angular Velocity ──
  obs[31] = std::clamp(agent->body->GetAngularVelocity() / 5.0f, -1.0f, 1.0f);

  // ── [32] Enemy Ammo ──
  {
    int enemyBullets = 0;
    if (enemy) {
      for (auto *b : game.bullets) {
        if (b->ownerPlayerIndex == enemy->playerIndex && !b->isMissile && !b->isFrag)
          enemyBullets++;
      }
    }
    obs[32] = std::clamp(1.0f - (enemyBullets / MAX_ACTIVE_BULLETS), 0.0f, 1.0f);
  }

  // ── [33-34] Agent Linear Velocity (Local Frame) ──
  obs[33] = std::clamp((aVel.x * aCos + aVel.y * aSin) / maxSpeed, -1.0f, 1.0f);
  obs[34] = std::clamp((-aVel.x * aSin + aVel.y * aCos) / maxSpeed, -1.0f, 1.0f);

  // ── [35] Shoot Cooldown ──
  obs[35] = std::clamp(1.0f - (agent->shootCooldownTimer / 0.5f), 0.0f, 1.0f);
}