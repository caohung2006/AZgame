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

      // Thưởng nhẹ khi di chuyển về phía waypoint (giữ tín hiệu hướng dẫn)
      if (!canSeeEnemy && fwdSpeed > 0.5f && dotWp > 0.0f)
        reward += 1.0f * DT_SCALE;

      // Approach nhẹ — chỉ là hint, không chi phối
      float approachDelta = prevDist - outCurrDist;
      if (approachDelta > 0.01f)
        reward += std::min(approachDelta * 2.0f, 0.5f) * DT_SCALE;

      // ★ PHẠT TƯỜNG + LÙI XE (Kỹ năng sinh tồn — phải duy trì ở MỌI Phase)
      if (noseInWall) {
        if (actions.forward)
          reward -= 6.0f * DT_SCALE;
        else if (actions.backward)
          reward += 1.0f * DT_SCALE;
        else
          reward -= 2.0f * DT_SCALE;
      }

      return reward;
    }

    // =====================================================================
    // 📚 PHASE 1, 2, 3: DENSE REWARD (CẦM TAY CHỈ VIỆC)
    // =====================================================================
    //
    // Triết lý thiết kế: 2 CHẾ ĐỘ rõ ràng
    //   NAVIGATION (chưa thấy địch): Đi theo waypoint, tìm kiếm
    //   ENGAGEMENT (thấy địch):      Giữ tầm bắn, ngắm, bắn
    // =====================================================================

    // ══ KÊNH 0: QUẢN LÝ KHOẢNG CÁCH (Distance Management) ══
    // Tạo "giếng hấp dẫn" tại 3-8m: AI bị hút về tầm bắn tối ưu
    if (outCurrDist > 8.0f) {
      // Xa quá → thưởng tiến gần (nhưng YẾU HƠN aim reward)
      float approachDelta = prevDist - outCurrDist;
      if (approachDelta > 0.01f) {
        reward += std::min(approachDelta * 3.0f, 0.08f) * DT_SCALE * 60.0f;
        // Tối đa ~5 pts/s — luôn thấp hơn aim reward (8 pts/s)
      }
    } else if (outCurrDist >= 3.0f) {
      // ★ SWEET SPOT (3-8m): Thưởng LIÊN TỤC cho việc giữ vị trí tối ưu
      reward += 3.0f * DT_SCALE;
    } else if (outCurrDist >= 1.8f) {
      // Hơi gần (1.8-3m): Phạt nhẹ để đẩy ra sweet spot
      reward -= 3.0f * DT_SCALE;
    }
    // < 1.8m: Anti-Kamikaze bên dưới sẽ phạt nặng hơn

    // ══ KÊNH 1: VẬN ĐỘNG ══

    // [Anti-Kamikaze]: Phạt áp sát quá gần (< 1.8m)
    // Bypass khi khiên ĐANG BẬT (trạng thái thực, không phải nút bấm)
    bool shieldActive = (agentObs[22] > 0.5f);
    if (outCurrDist < 1.8f && !(shieldActive && phase >= Phase::PHASE3)) {
      reward -= 5.0f * DT_SCALE;
    }

    if (noseInWall) {
      if (actions.forward) {
        reward -= 6.0f * DT_SCALE; // Phạt húc tường
      } else if (actions.backward) {
        reward += 1.0f * DT_SCALE; // Thưởng lùi NHẸ (chặn wall-scraping exploit)
      } else {
        reward -= 2.0f * DT_SCALE; // Phạt idle ở tường
      }
    } else if (!canSeeEnemy || outCurrDist > 8.0f) {
      // ── NAVIGATION MODE ──
      // Kích hoạt khi: chưa thấy địch HOẶC thấy địch nhưng còn XA (>8m)
      // Fix: Phase 1 SPARSE luôn canSeeEnemy=true → cần outCurrDist>8m để AI học lái
      if (phase == Phase::PHASE1 || phase == Phase::PHASE2) {
        if (fwdSpeed > 0.0f && dotWp > 0.0f) {
          reward += (fwdSpeed / maxSpeed) * dotWp * 8.0f * DT_SCALE;
        }
      } else {
        if (moveDot > 0.0f)
          reward += (moveDot / maxSpeed) * 8.0f * DT_SCALE;
      }

      // [Anti-Retreat P3+]: Phạt đi lùi khi không sát tường và chưa thấy địch
      // → Ép AI phải TIẾN VỀ PHÍA TRƯỚC để tìm địch, không được lùi tránh
      // BỎ QUA phạt nếu đang có cảnh báo đạn (để AI tự do lùi né đạn)
      if (phase >= Phase::PHASE3 && actions.backward && !dangerAlert) {
        reward -= 4.0f * DT_SCALE;
      }
    } else {
      // ── ENGAGEMENT MODE (canSeeEnemy && dist <= 8m && !noseInWall) ──

      // [Anti-Retreat P3+]: Phạt đi LÙI khi THẤY ĐỊCH ở tầm bắn
      // → Đây là exploit chính của "turtle": lùi xe + giơ khiên
      // NGOẠI TRỪ: (1) đạn đang bay tới (dangerAlert), (2) quá gần (< 3m) cần lùi ra sweet spot
      if (phase >= Phase::PHASE3 && actions.backward && !dangerAlert && outCurrDist >= 3.0f) {
        reward -= 6.0f * DT_SCALE;
      }
    }

    // [Anti-Retreat P3+]: Phạt tăng khoảng cách khi thấy địch (bỏ chạy khỏi sweet spot)
    // CHỈ phạt khi đang ở sweet spot (>=3m). Nếu <3m thì được phép lùi ra.
    if (phase >= Phase::PHASE3 && canSeeEnemy && outCurrDist >= 3.0f && outCurrDist <= 8.0f && !dangerAlert) {
      float retreatDelta = outCurrDist - prevDist;
      if (retreatDelta > 0.01f) {
        reward -= retreatDelta * 5.0f; // Khoảng -15 pts/s nếu lùi max tốc, cực kỳ đanh thép
      }
    }

    // ══ KÊNH 2: KỶ LUẬT CẮM TRẠI & BEYBLADE (MỌI Phase 1-3) ══
    if (!canSeeEnemy) {
      // Đứng im khi chưa thấy địch → ăn phạt
      if (speedNow < 0.5f)
        reward -= 2.0f * DT_SCALE;
      // Xoay tít mù → ăn phạt nặng hơn
      if (angVel > 2.5f)
        reward -= 5.0f * DT_SCALE;
    } else if (outCurrDist > 8.0f) {
      // Thấy địch nhưng XA → phạt đứng im (ép phải tiến gần)
      if (speedNow < 0.5f)
        reward -= 3.0f * DT_SCALE;
    } else {
      // Thấy địch ở tầm gần mà vẫn đứng ngồi không ngắm → ăn phạt
      if (speedNow < 0.3f && dotAim < 0.80f && angVel < 0.5f)
        reward -= 3.0f * DT_SCALE;
    }

    // ══ KÊNH 3: NGẮM BẮN (CHỈ khi thấy địch) ══
    if (canSeeEnemy) {
      // Thưởng ngắm chính xác — ĐÂY LÀ TÍN HIỆU CHÍNH khi engagement
      float aimScale;
      if (outCurrDist < 3.0f)
        aimScale = 4.0f; // Gần: giảm, anti-kamikaze bù
      else if (outCurrDist < 8.0f)
        aimScale = 10.0f; // ★ Sweet spot: thưởng MẠNH NHẤT
      else
        aimScale = 3.0f; // Xa: giảm, ép tiến gần
      reward += dotAim * aimScale * DT_SCALE;
    } else if (!noseInWall) {
      // Chưa thấy địch → Nhìn theo Waypoint (hỗ trợ navigation)
      reward += dotWp * 4.0f * DT_SCALE;
    }

    // ══ KÊNH 4: KHIÊN (Phase 3+) ══
    if (phase == Phase::PHASE3) {
      bool shieldReady = (agentObs[23] > 0.99f);  // Cooldown xong chưa?
      // shieldActive đã khai báo ở trên (dòng anti-kamikaze)

      if (actions.shield && shieldReady && !shieldActive) {
        if (dangerAlert) {
          // ★ BẬT ĐÚNG LÚC: Đạn tới -> Thưởng NÓNG 1 lần lớn (không nhân DT)
          reward += 10.0f;
        } else {
          // Bật khiên lãng phí -> Phạt NÓNG 1 lần (không nhân DT)
          reward -= 5.0f;
        }
      }

      // Thưởng/phạt khi khiên đang active
      if (shieldActive) {
        if (dangerAlert) {
          reward += 3.0f * DT_SCALE;  // Đang che đạn → tốt
        }
      }

      // Thưởng né đạn bằng di chuyển (dù có khiên hay không)
      if (dangerAlert && speedNow > 1.0f)
        reward += 2.0f * DT_SCALE;
    }

    // ══ KÊNH 5: XẠ THỦ ══
    if (actions.shoot) {
      if (phase == Phase::PHASE1 || phase == Phase::PHASE2) {
        if (canSeeEnemy && dotAim > 0.90f && outCurrDist < 10.0f) {
          reward += 15.0f * DT_SCALE; // Bắn chuẩn: thưởng cao nhất
        } else {
          reward -= 4.0f * DT_SCALE; // Xả đạn bừa
        }
      } else if (phase == Phase::PHASE3) {
        if (!canSeeEnemy)
          reward -= 4.0f * DT_SCALE;  // 6→4: Giảm nhẹ để AI không sợ bắn
        else {
          if (ammoLevel > 0.01f) {
            if (dotAim > 0.85f && outCurrDist < 10.0f)
              reward += 12.0f * DT_SCALE; // 10→12, 0.90→0.85: Dễ đạt hơn + thưởng cao hơn
            else if (dotAim > 0.60f && outCurrDist < 10.0f)
              reward -= 1.0f * DT_SCALE;  // Bắn lệch nhẹ: phạt NHẸ (không -5)
            else
              reward -= 3.0f * DT_SCALE;  // 5→3: Bắn bừa vẫn phạt nhưng NHẸ HƠN
          } else
            reward -= 1.0f * DT_SCALE;
        }
      }
    } else {
      // Phạt chần chừ khi ngắm tương đối chuẩn tại sweet spot ở P3
      // 0.95→0.85: Hạ ngưỡng để AI bị ép bắn sớm hơn, không chờ ngắm hoàn hảo
      if (phase == Phase::PHASE3 && canSeeEnemy && ammoLevel > 0.01f &&
          dotAim > 0.85f && outCurrDist >= 3.0f && outCurrDist < 8.0f) {
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
      bonus += 500.0f + (timeRatio * 500.0f);
    } else if (!agentAlive) {
      bonus -= 500.0f;
    } else if (agentAlive && enemyAlive && stepsTaken >= maxSteps - 2) {
      bonus -= 500.0f;
    }
    return bonus;
  }

  // =====================================================================
  // 📚 PHASE 1, 2, 3: ĐÁNH GIÁ CHUẨN
  // =====================================================================
  if (agentWin) {
    bonus += 400.0f; // Tăng phần thưởng thắng để bù đắp rủi ro
    bonus += timeRatio * 200.0f;
    if (agentDidShoot)
      bonus += 50.0f;
  } else if (agentAlive) {
    if (enemyAlive && stepsTaken >= maxSteps - 2) {
      // Hết giờ nhưng vẫn sống (Hòa)
      if (phase == Phase::PHASE3 && !agentDidShoot) {
        bonus -= 500.0f; // Rùa rụt cổ, không bắn phát nào
      } else {
        bonus -= 300.0f; // Cố gắng chiến đấu nhưng chưa giết được (Phạt nặng)
      }
    } else {
      bonus += 100.0f; // Sống sót (enemy chết do bom/ngoại cảnh)
    }
  } else {
    // CHẾT LÀ ĐIỀU TỒI TỆ (Nhưng vẫn tốt hơn rùa rụt cổ)
    // Thang điểm: Thắng (+400) > Hòa có bắn (-300) > Chết (-400) > Rùa rụt cổ (-500)
    bonus -= 400.0f; 
  }

  return bonus;
}