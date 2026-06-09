#include "game.h"
#include "renderer.h"
#include <cmath>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <raylib.h>
#include <tuple>
#include <vector>
#include "bot.h"
#include <algorithm>

namespace py = pybind11;

// ============================================================================
//  BOUNCE HINT — Tìm đường bắn nảy tường tốt nhất cho AI
//  Phiên bản đơn giản của FindBounce trong bot.cpp, dùng để "phím bài" cho Agent.
//  Quét 180 tia (bước 2°), mỗi tia trace tối đa 4 lần nảy tường.
//  Nếu tìm thấy đường đạn trúng địch → trả về điểm đập tường đầu tiên.
// ============================================================================
namespace {

class BounceRayCastCallback : public b2RayCastCallback {
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

inline b2Vec2 BounceReflect(b2Vec2 incident, b2Vec2 normal) {
    float len = incident.Length();
    if (len < 1e-4f) return b2Vec2(0,0);
    b2Vec2 i(incident.x / len, incident.y / len);
    float d = i.x * normal.x + i.y * normal.y;
    b2Vec2 r(i.x - 2.f * d * normal.x, i.y - 2.f * d * normal.y);
    float rl = r.Length();
    return rl < 1e-4f ? b2Vec2(0,0) : b2Vec2(r.x / rl, r.y / rl);
}

inline float SegDist(b2Vec2 P, b2Vec2 A, b2Vec2 B) {
    b2Vec2 AB = B - A; b2Vec2 AP = P - A;
    float ab2 = AB.x*AB.x + AB.y*AB.y;
    if (ab2 < 1e-6f) return (P - A).Length();
    float t = (AP.x*AB.x + AP.y*AB.y) / ab2;
    if (t < 0.f) t = 0.f; if (t > 1.f) t = 1.f;
    b2Vec2 closest(A.x + t*AB.x, A.y + t*AB.y);
    return (P - closest).Length();
}

/// Tìm điểm đập tường đầu tiên của đường đạn nảy trúng enemy.
/// @param world    Thế giới Box2D
/// @param myPos    Vị trí xe tăng AI
/// @param enemyBody Body của kẻ địch
/// @param outPoint [OUT] Điểm đập tường đầu tiên (để AI ngắm vào đây)
/// @return true nếu tìm thấy đường bắn nảy hợp lệ
bool FindBounceHint(b2World& world, b2Vec2 myPos, b2Body* enemyBody, b2Vec2& outPoint) {
    const float step = 0.035f;  // ~2° per ray
    const int numRays = (int)(2.f * 3.14159265f / step);
    const float bulletR = 3.0f / SCALE;
    const float muzzleOffset = 22.5f / SCALE;
    const float selfSafe = 1.5f; // Bán kính an toàn ~45px
    float bestScore = 1e9f;
    bool found = false;

    for (int i = 0; i < numRays; i++) {
        float a = i * step;
        b2Vec2 dir(-sinf(a), cosf(a));
        b2Vec2 muzzle = myPos + muzzleOffset * dir;
        b2Vec2 pos = muzzle;
        b2Vec2 d = dir;
        float rem = 80.f;
        b2Vec2 firstWall(0,0);
        bool gotWall = false, hitEnemy = false, selfHit = false;

        for (int bounce = 0; bounce < 4 && rem > 1.0f; bounce++) {
            BounceRayCastCallback cb;
            world.RayCast(&cb, pos, pos + rem * d);
            if (!cb.hit) break;

            // Kiểm tra self-hit sau khi nảy tường
            if (bounce > 0) {
                float distToSelf = SegDist(myPos, pos, cb.point);
                if (distToSelf < selfSafe) { selfHit = true; break; }
            }

            if (!gotWall && cb.hitStatic) {
                firstWall = cb.point;
                gotWall = true;
            }
            if (cb.body == enemyBody) {
                hitEnemy = true;
                break;
            }
            if (!cb.hitStatic) break;

            float dist = (cb.point - pos).Length();
            rem -= dist;
            // Phản xạ
            d = BounceReflect(cb.point - pos, cb.normal);
            if (d.LengthSquared() < 0.01f) break;
            pos = cb.point + bulletR * cb.normal + 0.02f * d;
        }

        if (hitEnemy && gotWall && !selfHit) {
            float dist = (firstWall - myPos).Length();
            if (dist < bestScore) {
                bestScore = dist;
                outPoint = firstWall;
                found = true;
            }
        }
    }
    return found;
}

} // anonymous namespace

/**
 * @class RLEnv
 * @brief Lớp bao bọc (Wrapper) môi trường trò chơi để tương thích với các thư
 * viện Reinforcement Learning (như OpenAI Gym). Lớp này quản lý việc khởi tạo
 * trò chơi, thực hiện các bước đi (step) và thu thập trạng thái (state).
 */
class RLEnv {
private:
  Game *game;
  Renderer *renderer = nullptr;
  bool isRendering = false;
  int maxSteps;
  int currentStep;
  int lastScores[4];
  float lastDistanceToTarget;

  int trainingMode;
  std::vector<int> lastAction0 = {0, 0, 0};
  std::vector<int> lastAction1 = {0, 0, 0};

  // Reward shaping state
  float minDistanceReached;
  int prevDangerBulletCount = 0;  // Đếm đạn nguy hiểm frame trước (cho Dodge Success Reward)
  b2Vec2 posHistory[60];
  int historyCount = 0;
  int historyIndex = 0;

  // Persistent Bot instances (giữ state giữa các frame: cachedPath, evasionTimer, ...)
  Bot* bots[4] = {nullptr, nullptr, nullptr, nullptr};


  float getRawDistanceToEnemy(int playerIdx) {
    Tank *myTank = nullptr;
    Tank *enemyTank = nullptr;
    for (auto t : game->tanks) {
      if (t->playerIndex == playerIdx) {
        myTank = t;
      } else if (!t->isDestroyed) {
        enemyTank = t;
      }
    }
    if (myTank && enemyTank) {
      b2Vec2 diff =
          myTank->body->GetPosition() - enemyTank->body->GetPosition();
      return diff.Length() * SCALE; // Khoảng cách tính bằng pixel
    }
    return 1000.0f; // Trả về khoảng cách an toàn rất lớn nếu địch đã chết
  }

public:
  // Hàm khởi tạo môi trường
  RLEnv(int num_players = 2, bool map_enabled = false,
        bool items_enabled = false, int training_mode = 0,
        bool bot_self_immune = false) {
    game = new Game();                    // Tạo đối tượng Game mới
    game->numPlayers = num_players;       // Số lượng người chơi
    game->mapEnabled = map_enabled;       // Có sử dụng bản đồ (vật cản) không
    game->itemsEnabled = items_enabled;   // Có xuất hiện vật phẩm không
    game->portalsEnabled = items_enabled; // Cổng dịch chuyển
    game->botSelfDamageImmune = bot_self_immune; // Bot miễn nhiễm đạn tự bắn
    trainingMode = training_mode;
    maxSteps = (trainingMode == 2) ? 1000 : 8000;
    currentStep = 0;
    for (int i = 0; i < 4; i++)
      lastScores[i] = 0;
  }

  ~RLEnv() {
    for (int i = 0; i < 4; i++) { delete bots[i]; bots[i] = nullptr; }
    if (isRendering) {
      CloseWindow();
      delete renderer;
    }
    delete game;
  }

  bool render() {
    if (!isRendering) {
      InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "AZGame RL Training Watcher");
      SetTargetFPS(60); // Cap frame rate để dễ nhìn trong lúc train
      renderer = new Renderer();
      isRendering = true;
    }

    if (WindowShouldClose()) {
      CloseWindow();
      isRendering = false;
      delete renderer;
      renderer = nullptr;
      return false;
    }

    // renderer chỉ draw logic cũ, game logic đã được update trong step()
    renderer->Update(*game, 1.0f / 60.0f);

    BeginDrawing();
    ClearBackground({20, 20, 25, 255}); // Nền tối
    if (renderer)
      renderer->DrawWorld(*game);



    EndDrawing();

    return true;
  }

  /**
   * @brief Khởi tạo lại ván chơi (Reset)
   * Thường gọi khi bắt đầu ván mới hoặc khi AI bị chết/hết thời gian.
   * @return Trạng thái ban đầu của ván chơi.
   */
  std::vector<float> reset() {
    game->ResetMatch(); // Gọi hàm reset trong engine game
    currentStep = 0;
    // Cập nhật lại điểm số ban đầu
    for (int i = 0; i < 4; i++)
      lastScores[i] = game->playerScores[i];
    lastDistanceToTarget = 0.0f;
    
    // Reset reward shaping state
    minDistanceReached = 9999.0f;
    prevDangerBulletCount = 0;
    historyCount = 0;
    historyIndex = 0;

    // Reset Bot state (map mới → path cũ vô nghĩa)
    for (int i = 0; i < 4; i++) { delete bots[i]; bots[i] = nullptr; }

    // Gán miễn nhiễm đạn tự bắn cho bot tanks (player != 0)
    if (game->botSelfDamageImmune) {
        for (auto t : game->tanks) {
            if (t->playerIndex != 0) { // Player 0 = AI agent, còn lại = bot
                t->selfDamageImmune = true;
            }
        }
    }

    return getState(0);
  }

  /**
   * @brief Thực hiện một hành động (Step) trong môi trường.
   * @param action Mã hành động (0-5) từ phía AI Python gửi sang.
   * @return Một Python Tuple chứa (Trạng thái mới, Phần thưởng, Ván chơi kết
   * thúc chưa).
   */
  py::tuple step(std::vector<int> action0,
                 std::vector<int> action1 = std::vector<int>()) {
    
    Tank *myTank = nullptr;
    Tank *enemyTank = nullptr;
    for (auto t : game->tanks) {
      if (t->playerIndex == 0) myTank = t;
      else if (!t->isDestroyed) enemyTank = t;
    }

    bool isEnemyInSight = false;
    float dotProd = 0.0f;
    if (myTank && enemyTank && !enemyTank->isDestroyed) {
      b2Vec2 forwardDir(-sinf(myTank->body->GetAngle()), cosf(myTank->body->GetAngle()));
      b2Vec2 toEnemy = enemyTank->body->GetPosition() - myTank->body->GetPosition();
      toEnemy.Normalize();
      dotProd = forwardDir.x * toEnemy.x + forwardDir.y * toEnemy.y;

      float rayLength = std::sqrt(SCREEN_WIDTH * SCREEN_WIDTH + SCREEN_HEIGHT * SCREEN_HEIGHT) / SCALE;
      b2Vec2 p2 = myTank->body->GetPosition() + rayLength * forwardDir;
      EnemyForwardRayCastCallback cbTarget(enemyTank->body);
      game->world.RayCast(&cbTarget, myTank->body->GetPosition(), p2);
      isEnemyInSight = cbTarget.hitEnemy;
    }

    float shootReward = 0.0f;
    TankActions tankActions0;
    // MultiDiscrete([3, 3, 2]) Action Space
    if (action0.size() == 3) {
      if (action0[0] == 1) tankActions0.forward = true;
      else if (action0[0] == 2) tankActions0.backward = true;

      if (action0[1] == 1) tankActions0.turnLeft = true;
      else if (action0[1] == 2) tankActions0.turnRight = true;

      if (action0[2] == 1 && trainingMode != 2) {
          // === Kiểm tra có Bounce Hint hợp lệ không + tính độ chính xác === 
          bool hasBounceTarget = false;
          float bounceAimPrecision = 0.0f; // Cos(góc lệch nòng vs bounce point)
          if (myTank && enemyTank && !enemyTank->isDestroyed && game->mapEnabled) {
              b2Vec2 bp;
              hasBounceTarget = FindBounceHint(game->world,
                  myTank->body->GetPosition(), enemyTank->body, bp);
              if (hasBounceTarget) {
                  // Tính góc giữa nòng súng và hướng tới bounce point
                  b2Vec2 toBounce = bp - myTank->body->GetPosition();
                  float bounceAngle = atan2f(-toBounce.x, toBounce.y);
                  float aimError = bounceAngle - myTank->body->GetAngle();
                  // Normalize angle to [-PI, PI]
                  while (aimError > PI) aimError -= 2*PI;
                  while (aimError < -PI) aimError += 2*PI;
                  bounceAimPrecision = cosf(aimError); // 1.0 = hoàn hảo, 0 = vuông góc
              }
          }

          if (isEnemyInSight) {
              // Thấy địch trực tiếp → cho phép bắn + thưởng ngắm chuẩn
              tankActions0.shoot = true;
              shootReward += 0.05f + std::max(0.0f, dotProd * 0.05f);
          } else if (hasBounceTarget) {
              // Có đường bounce → thưởng theo ĐỘ CHÍNH XÁC ngắm
              tankActions0.shoot = true;
              if (bounceAimPrecision > 0.97f) {
                  shootReward += 0.08f;
              } else if (bounceAimPrecision > 0.90f) {
                  shootReward += 0.03f;
              } else {
                  shootReward -= 0.03f;
              }
          } else if (!game->mapEnabled) {
              // BÃI TRỐNG (Phase 1-4): CHO BẮN TỰ DO → AI tự học từ kết quả
              // Chỉ phạt nhẹ nếu ngắm quá chệch (không chặn bắn!)
              tankActions0.shoot = true;
              if (dotProd > 0.3f) {
                  shootReward += 0.01f;   // Ngắm gần đúng hướng → khuyến khích
              } else {
                  shootReward -= 0.01f;   // Ngắm lệch → phạt nhẹ (KHÔNG chặn bắn)
              }
          } else {
              // MÊ CUNG: Bỏ chặn bắn! Để AI tự do khám phá cách đạn nảy.
              // Chỉ phạt cực nhẹ để nó không xả đạn vô tội vạ.
              tankActions0.shoot = true;
              shootReward -= 0.005f; 
          }
      }
      
      lastAction0 = action0;
    }

    std::vector<TankActions> all_actions(game->numPlayers);
    all_actions[0] = tankActions0; // Người chơi 0 là AI

    // Người chơi 1 là đối thủ (nếu có action1 truyền tới)
    if (game->numPlayers > 1 && action1.size() == 3) {
      TankActions tankActions1;
      if (action1[0] == 1) tankActions1.forward = true;
      else if (action1[0] == 2) tankActions1.backward = true;

      if (action1[1] == 1) tankActions1.turnLeft = true;
      else if (action1[1] == 2) tankActions1.turnRight = true;

      if (action1[2] == 1) tankActions1.shoot = true;
      
      lastAction1 = action1;
      all_actions[1] = tankActions1;
    }



    // Cập nhật logic game với thời gian cố định 60 FPS (khoảng 0.016s mỗi bước)
    game->Update(all_actions, 1.0f / 60.0f);
    currentStep++;

    // ╔══════════════════════════════════════════════════════════════════╗
    // ║         REWARD FUNCTION v3 — ĐƠN GIẢN, KHÔNG MÂU THUẪN        ║
    // ║  Chỉ 5 tín hiệu: Alive + Approach + Shoot + Kill/Death + Time ║
    // ╚══════════════════════════════════════════════════════════════════╝

    float reward = 0.0f;

    // --- Tìm lại tank sau khi Update ---
    myTank = nullptr;
    enemyTank = nullptr;
    for (auto t : game->tanks) {
      if (t->playerIndex == 0) myTank = t;
      else if (!t->isDestroyed) enemyTank = t;
    }
    bool p0Alive = (myTank != nullptr && !myTank->isDestroyed);

    // ====================================================================
    //  1. ALIVE BONUS — Sống = tốt. Đơn giản vậy thôi.
    //     +0.005/frame × 8000 = +40/episode baseline
    // ====================================================================
    if (p0Alive) {
        reward += 0.005f;
    }

    // ====================================================================
    //  2. APPROACH SHAPING — Tiến gần địch = tốt, lùi xa = xấu
    //     Dùng A* distance (mê cung) hoặc Euclidean (bãi trống)
    //     Đây là TÍN HIỆU DUY NHẤT về di chuyển — không cần camping/rush/zone
    // ====================================================================
    float currentDistToTarget = 0.0f;
    if (game->mapEnabled && p0Alive && enemyTank && !enemyTank->isDestroyed) {
        int pathDist = 0;
        game->map.GetNextWaypoint(game->world,
            myTank->body->GetPosition(), enemyTank->body->GetPosition(), pathDist);
        currentDistToTarget = pathDist;
    } else if (p0Alive && enemyTank && !enemyTank->isDestroyed) {
        currentDistToTarget = (myTank->body->GetPosition()
            - enemyTank->body->GetPosition()).Length();
    }

    if (p0Alive && enemyTank && !enemyTank->isDestroyed && lastDistanceToTarget > 0.1f) {
        float delta = lastDistanceToTarget - currentDistToTarget;
        delta = std::max(-2.0f, std::min(2.0f, delta));
        reward += delta * 0.1f;
    }
    lastDistanceToTarget = currentDistToTarget;

    // ====================================================================
    //  3. SHOOT ACCURACY — Chỉ thưởng/phạt khi AI BẤM BẮN
    //     Không phạt khi không bắn. Không phạt quá nặng khi bắn trượt.
    // ====================================================================
    reward += shootReward;

    // ====================================================================
    //  4. DODGE REWARD (Tín hiệu Né đạn) — Rất cần cho Phase khó
    //     Phạt nhẹ khi đạn bay thẳng vào người, thưởng khi đạn chệch đi
    // ====================================================================
    int currentDangerBullets = 0;
    if (p0Alive && myTank) {
        for (auto b : game->bullets) {
            if (!b || b->time <= 0 || b->ownerPlayerIndex == 0) continue;
            b2Vec2 toMe = myTank->body->GetPosition() - b->body->GetPosition();
            float dist = toMe.Length();
            if (dist > 8.0f || dist < 0.2f) continue;
            
            b2Vec2 bVel = b->body->GetLinearVelocity();
            float bSpd = bVel.Length();
            if (bSpd < 0.5f) continue;
            
            b2Vec2 bDir(bVel.x / bSpd, bVel.y / bSpd);
            float dot = (bDir.x * toMe.x + bDir.y * toMe.y) / dist;
            if (dot < 0.3f) continue; // Đạn không bay về hướng mình
            
            float perpDist = fabsf(bDir.x * toMe.y - bDir.y * toMe.x);
            if (perpDist < 2.5f) { // Quỹ đạo đạn cắt ngang người
                currentDangerBullets++;
                reward -= 0.01f; // Phạt nhẹ nguy hiểm
            }
        }
    }
    
    // Thưởng khi số đạn nguy hiểm giảm (né thành công)
    if (p0Alive && prevDangerBulletCount > currentDangerBullets) {
        reward += 0.03f * (prevDangerBulletCount - currentDangerBullets);
    }
    prevDangerBulletCount = currentDangerBullets;

    // ====================================================================
    //  5. KILL / DEATH / SUICIDE — Tín hiệu chính
    // ====================================================================
    int scoreDiff = game->playerScores[0] - lastScores[0];
    if (scoreDiff > 0) {
      float killBonus = 100.0f;
      if (game->mapEnabled && game->itemsEnabled) killBonus = 200.0f;
      else if (game->mapEnabled)                   killBonus = 150.0f;
      reward += killBonus * scoreDiff;
      lastScores[0] = game->playerScores[0];
    }

    if (!p0Alive && game->needsRestart) {
        bool suicided = false;
        for (const auto& death : game->recentDeaths) {
            if (death.playerIndex == 0 && death.killerIndex == 0) {
                suicided = true;
                break;
            }
        }
        reward -= suicided ? 120.0f : 100.0f;
    }

    // ====================================================================
    //  5. TIMEOUT — Hết giờ mà chưa giết được = phạt nhẹ
    // ====================================================================
    bool isTimeout = (currentStep >= maxSteps);
    bool done = game->needsRestart || isTimeout || (!p0Alive);

    if (isTimeout && p0Alive) {
      if (trainingMode == 2) {
        reward += 100.0f;
      } else {
        reward -= 20.0f;  // Phạt cố định, đơn giản
      }
    }

    auto state = getState(0); // Lấy trạng thái mới sau khi thực hiện hành động
    return py::make_tuple(state, reward, done, isTimeout);
  }

  /**
   * @brief Trích xuất các đặc trưng trạng thái (Observations) để AI ra quyết
   * định. Các giá trị thường được chuẩn hóa về khoảng [0, 1] hoặc [-1, 1] để
   * mạng neural hoạt động tốt.
   */
  // Lớp hỗ trợ tia laser đo khoảng cách cho Radar
  class RadarRayCastCallback : public b2RayCastCallback {
  public:
    float closestFraction = 1.0f;
    float ReportFixture(b2Fixture *fixture, const b2Vec2 &point,
                        const b2Vec2 &normal, float fraction) override {
      // Chỉ tính vật cản tĩnh (Tường)
      if (fixture->GetBody()->GetType() == b2_staticBody) {
        if (fraction < closestFraction) {
          closestFraction = fraction;
        }
        return fraction; // Trả về fraction giúp tối ưu kết quả tìm kiếm của
                         // Box2D
      }
      return -1.0f; // Bỏ qua vật thể khác (đạn, xe tăng khác)
    }
  };

  class EnemyForwardRayCastCallback : public b2RayCastCallback {
  public:
    b2Body *enemyBody;
    bool hitEnemy = false;

    EnemyForwardRayCastCallback(b2Body *enemy) : enemyBody(enemy) {}

    float ReportFixture(b2Fixture *fixture, const b2Vec2 &point,
                        const b2Vec2 &normal, float fraction) override {
      b2Body *hitBody = fixture->GetBody();
      if (hitBody->GetType() == b2_staticBody) {
        hitEnemy = false;
        return fraction; // Chạm tường -> cắt tia
      }
      if (hitBody == enemyBody) {
        hitEnemy = true;
        return fraction; // Chạm địch -> cắt tia
      }
      return -1.0f; // Bỏ qua vật thể khác (đạn, xe của chính mình, v.v.)
    }
  };

  std::vector<float> getState(int playerIdx) {
    std::vector<float> state;

    Tank *myTank = nullptr;
    Tank *enemyTank = nullptr;
    for (auto t : game->tanks) {
      if (t->playerIndex == playerIdx) {
        myTank = t;
      } else if (!t->isDestroyed) {
        enemyTank = t;
      }
    }

    if (myTank && !myTank->isDestroyed) {
      b2Vec2 myPos = myTank->body->GetPosition();
      float myAngle = myTank->body->GetAngle();
      
      b2Vec2 forwardDir(-sinf(myAngle), cosf(myAngle));
      b2Vec2 rightDir(cosf(myAngle), sinf(myAngle));
      
      float rayLength = std::sqrt(SCREEN_WIDTH * SCREEN_WIDTH + SCREEN_HEIGHT * SCREEN_HEIGHT) / SCALE;

      // Nhóm 1: Self State (5 Tham số)
      state.push_back(cosf(myAngle)); // [0] Heading Cos
      state.push_back(sinf(myAngle)); // [1] Heading Sin
      
      b2Vec2 myVel = myTank->body->GetLinearVelocity();
      state.push_back((myVel.x * rightDir.x + myVel.y * rightDir.y) / 3.0f);   // [2] Local Vx (chuẩn hóa = max speed 3.0)
      state.push_back((myVel.x * forwardDir.x + myVel.y * forwardDir.y) / 3.0f); // [3] Local Vy
      state.push_back(myTank->body->GetAngularVelocity() / 3.0f);              // [4] Angular Velocity (chuẩn hóa)

      // Nhóm 2: Enemy Info (8 Tham số)
      if (enemyTank) {
        b2Vec2 toEnemy = enemyTank->body->GetPosition() - myPos;
        // Project toEnemy onto Local axes
        float localX = toEnemy.x * rightDir.x + toEnemy.y * rightDir.y;
        float localY = toEnemy.x * forwardDir.x + toEnemy.y * forwardDir.y;
        state.push_back(localX / rayLength); // [5] Enemy Local X
        state.push_back(localY / rayLength); // [6] Enemy Local Y
        state.push_back(std::min(1.0f, toEnemy.Length() / rayLength)); // [7] Enemy Distance
        
        // Line of Sight
        EnemyForwardRayCastCallback cbTarget(enemyTank->body);
        game->world.RayCast(&cbTarget, myPos, enemyTank->body->GetPosition());
        state.push_back(cbTarget.hitEnemy ? 1.0f : 0.0f); // [8] Line of Sight
        
        b2Vec2 enemyVel = enemyTank->body->GetLinearVelocity();
        state.push_back((enemyVel.x * rightDir.x + enemyVel.y * rightDir.y) / 3.0f);   // [9] Enemy Local Vx
        state.push_back((enemyVel.x * forwardDir.x + enemyVel.y * forwardDir.y) / 3.0f); // [10] Enemy Local Vy
        
        float enemyAngle = enemyTank->body->GetAngle();
        state.push_back(cosf(enemyAngle - myAngle)); // [11] Enemy Heading Cos
        state.push_back(sinf(enemyAngle - myAngle)); // [12] Enemy Heading Sin

        // [MỚI] Tốc độ địch tiến về phía mình (approach speed)
        b2Vec2 toMe = myPos - enemyTank->body->GetPosition();
        float toMeDist = toMe.Length();
        float approachSpeed = 0.0f;
        if (toMeDist > 0.1f) {
            approachSpeed = (toMe.x * enemyVel.x + toMe.y * enemyVel.y) / toMeDist; // Dương = địch đang lại gần
        }
        state.push_back(std::max(-1.0f, std::min(1.0f, approachSpeed / 3.0f))); // [13] Enemy Approach Speed

        // [MỚI] Địch có thấy mình không? (Reverse Line of Sight)
        EnemyForwardRayCastCallback cbReverse(myTank->body);
        game->world.RayCast(&cbReverse, enemyTank->body->GetPosition(), myPos);
        state.push_back(cbReverse.hitEnemy ? 1.0f : 0.0f); // [14] Am I Visible to Enemy
      } else {
        for(int i=0; i<10; i++) state.push_back(0.0f); // [5-14]
      }

      // Nhóm 3: Bullet Radar (8 Tham số) - 2 viên đạn nguy hiểm nhất (bay về phía mình)
      struct BulletData {
        b2Vec2 pos;
        b2Vec2 vel;
        float ttc;
        float missDist;
        float priority; // TTC càng nhỏ thì ưu tiên càng cao
      };
      std::vector<BulletData> enemyBullets;
      for (auto b : game->bullets) {
        if (b->time > 0.0f) {
          b2Vec2 bPos = b->body->GetPosition();
          b2Vec2 bVel = b->body->GetLinearVelocity();
          b2Vec2 relPos = myPos - bPos;
          b2Vec2 relVel = bVel - myVel;
          
          float speedSqr = relVel.LengthSquared();
          float ttc = 1.0f; // Max
          float missDist = 1.0f;
          float priority = 999.0f;
          
          if (speedSqr > 0.001f) {
            float t = relPos.x * relVel.x + relPos.y * relVel.y;
            t /= speedSqr;
            if (t > 0) { // Bullet is moving towards tank
              ttc = std::min(1.0f, t / 5.0f); // Normalize max 5 seconds
              b2Vec2 closestPoint = bPos + t * bVel;
              b2Vec2 myExpectedPos = myPos + t * myVel;
              missDist = std::min(1.0f, (closestPoint - myExpectedPos).Length() / (100.0f/SCALE));
              priority = ttc;
            }
          }
          enemyBullets.push_back({bPos, bVel, ttc, missDist, priority});
        }
      }
      
      // Sort by priority (ascending)
      std::sort(enemyBullets.begin(), enemyBullets.end(), [](const BulletData& a, const BulletData& b) {
        return a.priority < b.priority;
      });
      
      for (int i = 0; i < 2; i++) {
        if (i < enemyBullets.size()) {
          b2Vec2 toBullet = enemyBullets[i].pos - myPos;
          float localX = toBullet.x * rightDir.x + toBullet.y * rightDir.y;
          float localY = toBullet.x * forwardDir.x + toBullet.y * forwardDir.y;
          state.push_back(localX / rayLength); // [13, 17]
          state.push_back(localY / rayLength); // [14, 18]
          state.push_back(enemyBullets[i].ttc); // [15, 19]
          state.push_back(enemyBullets[i].missDist); // [16, 20]
        } else {
          state.push_back(0.0f);
          state.push_back(0.0f);
          state.push_back(1.0f); // Max TTC
          state.push_back(1.0f); // Max Miss Dist
        }
      }

      // Nhóm 4: Wall Radar (8 Tham số)
      float scanAngles[] = {-135.0f, -90.0f, -45.0f, 0.0f, 45.0f, 90.0f, 135.0f, 180.0f};
      for (int i = 0; i < 8; i++) {
        float rad = myAngle + scanAngles[i] * PI / 180.0f;
        b2Vec2 p2 = myPos + rayLength * b2Vec2(-sinf(rad), cosf(rad));
        RadarRayCastCallback cb;
        game->world.RayCast(&cb, myPos, p2);
        state.push_back(cb.closestFraction); // [21-28]
      }

      // Nhóm 5: A* Navigation (3 Tham số)
      if (game->mapEnabled && enemyTank) {
        int pathDist = 0;
        b2Vec2 waypoint = game->map.GetNextWaypoint(game->world, myPos, enemyTank->body->GetPosition(), pathDist);
        b2Vec2 toWP = waypoint - myPos;
        float localX = toWP.x * rightDir.x + toWP.y * rightDir.y;
        float localY = toWP.x * forwardDir.x + toWP.y * forwardDir.y;
        
        state.push_back(localX / rayLength); // [29] Waypoint Local X
        state.push_back(localY / rayLength); // [30] Waypoint Local Y
        state.push_back(std::min(1.0f, pathDist / 48.0f)); // [31] Waypoint Dist
      } else {
        state.push_back(0.0f); // [29]
        state.push_back(0.0f); // [30]
        state.push_back(1.0f); // [31]
      }

      // Nhóm 6: Status (5 Tham số)
      state.push_back(myTank->currentWeapon != ItemType::NORMAL ? std::min(1.0f, myTank->ammo / 5.0f) : 0.0f); // [32] My Ammo
      state.push_back(std::max(0.0f, 1.0f - myTank->shootCooldownTimer / 0.5f)); // [33] My Shoot Cooldown
      state.push_back(enemyTank && enemyTank->currentWeapon != ItemType::NORMAL ? std::min(1.0f, enemyTank->ammo / 5.0f) : 0.0f); // [34] Enemy Ammo
      state.push_back(myTank->hasShield ? 1.0f : 0.0f); // [35] Shield Active
      state.push_back(std::max(0.0f, 1.0f - myTank->shieldCooldownTimer / 15.0f)); // [36] Shield Cooldown

      // Nhóm 6b: Weapon Type One-Hot (5 Tham số: NORMAL, GATLING, FRAG, MISSILE, DEATH_RAY)
      state.push_back(myTank->currentWeapon == ItemType::NORMAL    ? 1.0f : 0.0f); // [37]
      state.push_back(myTank->currentWeapon == ItemType::GATLING   ? 1.0f : 0.0f); // [38]
      state.push_back(myTank->currentWeapon == ItemType::FRAG      ? 1.0f : 0.0f); // [39]
      state.push_back(myTank->currentWeapon == ItemType::MISSILE   ? 1.0f : 0.0f); // [40]
      state.push_back(myTank->currentWeapon == ItemType::DEATH_RAY ? 1.0f : 0.0f); // [41]

      // Nhóm 7: Previous Action One-Hot (8 Tham số)
      std::vector<int> lastAct = (playerIdx == 0) ? lastAction0 : lastAction1;
      int mMove = lastAct[0];
      int mTurn = lastAct[1];
      int mShoot = lastAct[2];
      
      // Move (0, 1, 2)
      state.push_back(mMove == 0 ? 1.0f : 0.0f); // [42]
      state.push_back(mMove == 1 ? 1.0f : 0.0f); // [43]
      state.push_back(mMove == 2 ? 1.0f : 0.0f); // [44]
      
      // Turn (0, 1, 2)
      state.push_back(mTurn == 0 ? 1.0f : 0.0f); // [45]
      state.push_back(mTurn == 1 ? 1.0f : 0.0f); // [46]
      state.push_back(mTurn == 2 ? 1.0f : 0.0f); // [47]
      
      // Shoot (0, 1)
      state.push_back(mShoot == 0 ? 1.0f : 0.0f); // [48]
      state.push_back(mShoot == 1 ? 1.0f : 0.0f); // [49]

      // Nhóm 8: Bounce Hint (3 Tham số) — "Phím bài" cho AI bắn nảy tường
      // Dùng cùng thuật toán ray-tracing 180 tia như Bot C++ để tìm điểm đập tường
      // tối ưu. AI chỉ cần học: "Xoay nòng khớp với bounceLocalX/Y rồi bắn".
      if (enemyTank && !enemyTank->isDestroyed && game->mapEnabled) {
          b2Vec2 bouncePoint(0,0);
          bool hasBounce = FindBounceHint(game->world, myPos, enemyTank->body, bouncePoint);
          if (hasBounce) {
              state.push_back(1.0f); // [50] Has Bounce Target
              b2Vec2 toBounce = bouncePoint - myPos;
              float bLocalX = toBounce.x * rightDir.x + toBounce.y * rightDir.y;
              float bLocalY = toBounce.x * forwardDir.x + toBounce.y * forwardDir.y;
              state.push_back(bLocalX / rayLength); // [51] Bounce Local X
              state.push_back(bLocalY / rayLength); // [52] Bounce Local Y
          } else {
              state.push_back(0.0f); // [50] No bounce available
              state.push_back(0.0f); // [51]
              state.push_back(0.0f); // [52]
          }
      } else {
          state.push_back(0.0f); // [50]
          state.push_back(0.0f); // [51]
          state.push_back(0.0f); // [52]
      }

    } else {
      // Tank dead -> fill 55 zeros (52 cũ + 3 bounce hint)
      state.insert(state.end(), 55, 0.0f);
    }

    return state;
  }

  std::vector<int> getBotAction(int level, int playerIdx) {
      if (playerIdx < 0 || playerIdx >= 4) return {0, 0, 0};

      // Tạo Bot persistent nếu chưa có, hoặc nếu level thay đổi (do RuleBasedBot.sample_level)
      if (!bots[playerIdx] || bots[playerIdx]->level != level) {
          delete bots[playerIdx];
          bots[playerIdx] = new Bot(level, playerIdx);
          bots[playerIdx]->fastMode = true;  // Tối ưu hiệu năng cho training
      }

      TankActions acts = bots[playerIdx]->GetAction(game);
      std::vector<int> result(3, 0);
      
      if (acts.forward) result[0] = 1;
      else if (acts.backward) result[0] = 2;
      
      if (acts.turnLeft) result[1] = 1;
      else if (acts.turnRight) result[1] = 2;
      
      if (acts.shoot) result[2] = 1;
      
      return result;
  }
};

PYBIND11_MODULE(azgame_env, m) {
  m.doc() = "Môi trường học tăng cường Pybind11 cho AZGame xe tăng";
  py::class_<RLEnv>(m, "RLEnv")
      .def(py::init<int, bool, bool, int, bool>(), py::arg("num_players") = 2,
           py::arg("map_enabled") = true, py::arg("items_enabled") = true,
           py::arg("training_mode") = 0, py::arg("bot_self_immune") = false)
      .def("reset", &RLEnv::reset)
      .def("step", &RLEnv::step, py::arg("action0"),
           py::arg("action1") = std::vector<int>())
      .def("get_state", &RLEnv::getState, py::arg("playerIdx"))
      .def("render", &RLEnv::render)
      .def("get_bot_action", &RLEnv::getBotAction, py::arg("level"), py::arg("playerIdx"));
}
