#pragma once
#include "game.h"
#include "Observation.h" // Import để có CheckLineOfSight
#include "Curriculum.h"  // Import để nhận biết Phase
#include <algorithm>
#include <cmath>
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
//  Unified Step Reward (Dynamic Curriculum Learning)
// ─────────────────────────────────────────────────────────────────────────────
inline float ComputeStepReward(const Game &game, int agentIdx, bool canSeeEnemy,
                               b2Vec2 astarWaypoint, float prevDist,
                               float &outCurrDist, const TankActions &actions,
                               const std::vector<float> &agentObs, Phase phase) {
  const Tank *agent = nullptr;
  const Tank *enemy = nullptr;
  for (auto *t : game.tanks) {
    if (t->playerIndex == agentIdx)
      agent = t;
    else
      enemy = t;
  }

  if (!agent || agentObs.size() < 36) {
    outCurrDist = prevDist;
    return 0.0f;
  }

  float reward = 0.0f;
  const float DT_SCALE = 1.0f / 60.0f;

  float ammoLevel = agentObs[21];
  float radarFront = agentObs[24];
  float radarLeft = agentObs[25];
  float radarRight = agentObs[26];

  // ─── RADAR CẢNH BÁO ĐẠN SỚM (O(1) Filter + Hybrid Raycast) ───
  bool dangerAlert = false;
  b2Vec2 aPos = agent->body->GetPosition();
  float aAng = agent->body->GetAngle();
  float cosA = std::cos(aAng);
  float sinA = std::sin(aAng);

  auto CheckBulletDanger = [&](int base) -> bool {
    float dx_n = agentObs[base], dy_n = agentObs[base + 1];
    float vx_n = agentObs[base + 2], vy_n = agentObs[base + 3];
    float dSq = dx_n * dx_n + dy_n * dy_n;
    if (dSq > 0.0001f && dSq < 0.40f) {
      float dot = dx_n * vx_n + dy_n * vy_n;
      if (dot < -0.1f || dSq < 0.035f) {
        float rx = dx_n * 8.0f, ry = dy_n * 8.0f;
        b2Vec2 bPos(aPos.x + (rx * cosA - ry * sinA),
                    aPos.y + (rx * sinA + ry * cosA));
        if (CheckLineOfSight(game, aPos, bPos))
          return true;
      }
    }
    return false;
  };

  if (CheckBulletDanger(13) || CheckBulletDanger(17))
    dangerAlert = true;

  // ─── PHÂN TÍCH CHUYỂN ĐỘNG & GÓC NHÌN ───
  if (enemy) {
    b2Vec2 ePos = enemy->body->GetPosition();
    b2Vec2 toEnemy = ePos - aPos;
    outCurrDist = toEnemy.Length();

    float agentAngle = agent->body->GetAngle();
    b2Vec2 fwd(-std::sin(agentAngle), std::cos(agentAngle));
    b2Vec2 vel = agent->body->GetLinearVelocity();
    float angVel = std::abs(agent->body->GetAngularVelocity());

    b2Vec2 toEnNorm = toEnemy;
    if (outCurrDist > 1e-4f)
      toEnNorm.Normalize();

    b2Vec2 toTarget = astarWaypoint - aPos;
    b2Vec2 toTargetNorm = toTarget;
    if (toTarget.Length() > 1e-4f)
      toTargetNorm.Normalize();

    float maxSpeed = 3.0f;
    float speedNow = vel.Length();
    float fwdSpeed = vel.x * fwd.x + vel.y * fwd.y;
    float moveDot = vel.x * toTargetNorm.x + vel.y * toTargetNorm.y;
    float dotAim = fwd.x * toEnNorm.x + fwd.y * toEnNorm.y;
    float dotWp = fwd.x * toTargetNorm.x + fwd.y * toTargetNorm.y;

    bool noseInWall =
        (radarFront < 0.08f || radarLeft < 0.05f || radarRight < 0.05f);

    // =====================================================================
    // ⚔️ PHASE 4 & 5: SPARSE REWARD (SELF-PLAY MASTERY)
    // =====================================================================
    if (phase == Phase::PHASE4 || phase == Phase::PHASE5) {
      // Phạt đứng im khi chưa thấy địch → ép phải đi tìm
      if (speedNow < 0.2f && !canSeeEnemy)
        reward -= 2.0f * DT_SCALE;

      // Phạt xoay tại chỗ (Anti-beyblade) → ép phải di chuyển có mục đích
      if (angVel > 3.0f && speedNow < 0.5f)
        reward -= 3.0f * DT_SCALE;

      // Thưởng nhẹ khi di chuyển về phía waypoint (giữ tín hiệu hướng dẫn tối thiểu)
      if (!canSeeEnemy && fwdSpeed > 0.5f && dotWp > 0.0f)
        reward += 1.0f * DT_SCALE;

      return reward;
    }

    // =====================================================================
    // 📚 PHASE 1, 2, 3: DENSE REWARD (CẦM TAY CHỈ VIỆC)
    // =====================================================================

    // ══ KÊNH 1: VẬN ĐỘNG ══

    // [Anti-Kamikaze]: Phạt áp sát quá gần (< 1.8m) khi không có khiên
    if (outCurrDist < 1.8f && !actions.shield) {
      reward -= 5.0f * DT_SCALE;
    }

    if (noseInWall && speedNow < 0.2f && actions.forward) {
      // Phạt húc tường
      reward -= 4.0f * DT_SCALE;
    } else {
      if (phase == Phase::PHASE1 || phase == Phase::PHASE2) {
        // Chống Moonwalk: Bắt buộc đạp ga TỚI (fwdSpeed > 0) và HƯỚNG MẶT về đích
        if (fwdSpeed > 0.0f && dotWp > 0.0f) {
          reward += (fwdSpeed / maxSpeed) * dotWp * 10.0f * DT_SCALE;
        }
      } else {
        // Phase 3: Đi bình thường (chấp nhận lùi xe chiến thuật)
        if (moveDot > 0.0f)
          reward += (moveDot / maxSpeed) * 10.0f * DT_SCALE;
      }
    }

    // ══ KÊNH 2: KỶ LUẬT CẮM TRẠI & BEYBLADE (Áp dụng cho MỌI Phase 1-3) ══
    if (!canSeeEnemy) {
      // Đứng im khi chưa thấy địch → ăn phạt
      if (speedNow < 0.5f)
        reward -= 2.0f * DT_SCALE;
      // Xoay tít mù → ăn phạt nặng hơn
      if (angVel > 2.5f)
        reward -= 5.0f * DT_SCALE;
    } else {
      // Thấy địch rồi mà vẫn đứng ngồi ngáp → ăn phạt nhẹ
      if (speedNow < 0.5f && dotAim < 0.85f && angVel < 0.5f)
        reward -= 2.0f * DT_SCALE;
    }

    // ══ KÊNH 3: ĐỊNH HƯỚNG TẦM NHÌN ══
    if (canSeeEnemy) {
      // Thưởng xoay mặt hướng về địch (nhưng giảm tuyến tính khi quá gần)
      float aimScale = (outCurrDist > 3.0f) ? 10.0f : 5.0f;
      reward += dotAim * aimScale * DT_SCALE;
    } else if (!noseInWall) {
      // Chưa thấy địch → Nhìn theo Waypoint
      reward += dotWp * 5.0f * DT_SCALE;
    }

    // ══ KÊNH 4: KHIÊN (Chỉ mở ở Phase 3 trở lên) ══
    if (phase == Phase::PHASE3) {
      if (dangerAlert) {
        if (actions.shield)
          reward += 10.0f * DT_SCALE;
        if (speedNow > 1.0f || std::abs(fwdSpeed) > 1.0f)
          reward += 2.0f * DT_SCALE;
      } else {
        if (actions.shield)
          reward -= 2.0f * DT_SCALE;
      }
    }

    // ══ KÊNH 5: XẠ THỦ ══
    if (actions.shoot) {
      if (phase == Phase::PHASE1 || phase == Phase::PHASE2) {
        // Kỷ luật thép: Chỉ thưởng khi ngắm chuẩn VÀ thấy địch
        if (canSeeEnemy && dotAim > 0.90f) {
          reward += 15.0f * DT_SCALE;
        } else {
          reward -= 4.0f * DT_SCALE; // Phạt xả đạn linh tinh
        }
      } else if (phase == Phase::PHASE3) {
        // Đại học: Kỷ luật thép + kiểm tra đạn
        if (!canSeeEnemy)
          reward -= 6.0f * DT_SCALE;
        else {
          if (ammoLevel > 0.01f) {
            if (dotAim > 0.90f)
              reward += 10.0f * DT_SCALE;
            else
              reward -= 2.0f * DT_SCALE;
          } else
            reward -= 1.0f * DT_SCALE;
        }
      }
    } else {
      // Phạt chần chừ khi đã ngắm chuẩn ở P3
      if (phase == Phase::PHASE3 && canSeeEnemy && ammoLevel > 0.01f &&
          dotAim > 0.95f) {
        reward -= 5.0f * DT_SCALE;
      }
    }

  } else {
    outCurrDist = prevDist;
  }
  return reward;
}

// ─────────────────────────────────────────────────────────────────────────────
//  End-of-Episode Bonus (Xử lý Đỉnh cao cho Sparse Reward P4/5)
// ─────────────────────────────────────────────────────────────────────────────
inline float ComputeEndBonus(const Game &game, int agentIdx,
                             const int scoresBefore[4], int stepsTaken,
                             int maxSteps, bool agentDidShoot, Phase phase) {
  float bonus = 0.0f;
  bool agentAlive = false;
  const Tank *enemy = nullptr;

  for (auto *t : game.tanks) {
    if (t->playerIndex == agentIdx)
      agentAlive = true;
    else
      enemy = t;
  }

  bool enemyAlive = (enemy != nullptr);
  bool agentWin = (game.playerScores[agentIdx] > scoresBefore[agentIdx]);
  float timeRatio = 1.0f - (float)stepsTaken / (float)maxSteps;

  // =====================================================================
  // ⚔️ PHASE 4 & 5: LUẬT RỪNG (SPARSE REWARDS)
  // =====================================================================
  if (phase == Phase::PHASE4 || phase == Phase::PHASE5) {
    if (agentWin) {
      // Giết địch càng nhanh thưởng càng khủng
      bonus += 500.0f + (timeRatio * 500.0f);
    } else if (!agentAlive) {
      // Chết là mất trắng
      bonus -= 500.0f;
    } else if (agentAlive && enemyAlive && stepsTaken >= maxSteps - 2) {
      // Hết giờ mà không ai chết -> Cả 2 ăn phạt cực nặng
      bonus -= 500.0f;
    }
    return bonus;
  }

  // =====================================================================
  // 📚 PHASE 1, 2, 3: ĐÁNH GIÁ CHUẨN
  // =====================================================================
  if (agentWin) {
    // Thưởng thắng: Luôn cho 300 điểm dù thắng bằng cách nào
    // (agentDidShoot check cũ gây ra bug "sợ thắng")
    bonus += 300.0f;
    bonus += timeRatio * 200.0f;

    // Thưởng thêm nếu thắng bằng SÚNG (khuyến khích bắn tỉa)
    if (agentDidShoot)
      bonus += 50.0f;
  }

  if (agentAlive) {
    if (enemyAlive && stepsTaken >= maxSteps - 2) {
      bonus -= 200.0f; // Bỏ lỡ cơ hội giết địch
    } else {
      bonus += 100.0f; // Sống sót vẻ vang
    }
  } else {
    bonus -= 150.0f; // Bị hạ gục
  }

  return bonus;
}