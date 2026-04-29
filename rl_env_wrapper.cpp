#include "game.h"
#include "renderer.h"
#include <cmath>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <raylib.h>
#include <tuple>
#include <vector>
#include "bot.h"

namespace py = pybind11;

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
        bool items_enabled = false, int training_mode = 0) {
    game = new Game();                    // Tạo đối tượng Game mới
    game->numPlayers = num_players;       // Số lượng người chơi
    game->mapEnabled = map_enabled;       // Có sử dụng bản đồ (vật cản) không
    game->itemsEnabled = items_enabled;   // Có xuất hiện vật phẩm không
    game->portalsEnabled = items_enabled; // Cổng dịch chuyển
    trainingMode = training_mode;
    maxSteps = (trainingMode == 2) ? 1000 : 5000; // Phase 2 (học né) chỉ cần sống 16s
    currentStep = 0;                      // Bước hiện tại
    for (int i = 0; i < 4; i++)
      lastScores[i] = 0; // Lưu trữ điểm số trước đó để tính phần thưởng
  }

  ~RLEnv() {
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


    return getState(0); // Trả về trạng thái của người chơi 0
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
          if (isEnemyInSight) {
              tankActions0.shoot = true;
              shootReward += 0.05f + std::max(0.0f, dotProd * 0.05f); // Thưởng ngắm chuẩn
          } else {
              tankActions0.shoot = false; // Tịch thu lệnh bắn
              shootReward -= 0.1f; // Phạt spam bắn mù
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

    float reward =
        -0.015f; // Tăng nhẹ Time Penalty (từ -0.01) để ép AI hành động

    // --- LOGIC TÍNH PHẦN THƯỞNG (Reward) ---

    myTank = nullptr;
    enemyTank = nullptr;
    for (auto t : game->tanks) {
      if (t->playerIndex == 0)
        myTank = t;
      else if (!t->isDestroyed)
        enemyTank = t;
    }
    // 2. Kiểm tra trạng thái AI (index 0)
    bool p0Alive = (myTank != nullptr && !myTank->isDestroyed);

    // 0. Khuyến khích tiếp cận mục tiêu (Progress Reward) thay cho tốc độ
    float currentDistToTarget = 0.0f;
    if (game->mapEnabled && p0Alive && enemyTank && !enemyTank->isDestroyed) {
        int pathDist = 0;
        game->map.GetNextWaypoint(game->world, myTank->body->GetPosition(), enemyTank->body->GetPosition(), pathDist);
        currentDistToTarget = pathDist;
    } else if (p0Alive && enemyTank && !enemyTank->isDestroyed) {
        currentDistToTarget = (myTank->body->GetPosition() - enemyTank->body->GetPosition()).Length();
    }
    
    if (p0Alive && enemyTank && !enemyTank->isDestroyed && lastDistanceToTarget > 0.0f) {
        reward += (lastDistanceToTarget - currentDistToTarget) * 0.1f;
    }
    lastDistanceToTarget = currentDistToTarget;

    // Phạt đâm tường (tăng mạnh để AI sợ tường)
    if (myTank) {
      for (b2ContactEdge *edge = myTank->body->GetContactList(); edge;
           edge = edge->next) {
        if (edge->contact->IsTouching() &&
            edge->other->GetType() == b2_staticBody) {
          reward -= 0.02f;

          // Stuck Penalty: Nếu nhấn tiến/lùi mà không di chuyển được (vận tốc
          // thấp) khi chạm tường
          float speed = myTank->body->GetLinearVelocity().Length();
          if (speed < 0.2f && action0.size() == 3 &&
              (action0[0] == 1 || action0[0] == 2)) {
            reward -= 0.03f; // Phạt nặng để AI học cách lùi ra hoặc xoay đi
                             // hướng khác
          }
          break;
        }
      }
    }

    // Phạt / Thưởng bắn (đã tính trước khi Update để có Action Masking)
    reward += shootReward;

    // 1. Thưởng khi GIẾT (score tăng) = +100
    int scoreDiff = game->playerScores[0] - lastScores[0];
    if (scoreDiff > 0) {
      reward += 100.0f * scoreDiff;
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
        if (suicided) {
            reward -= 250.0f; // Phạt tự sát nặng
        } else {
            reward -= 100.0f; // Bị địch giết
        }
    }

    // Phạt đâm/ôm sát kẻ địch (Ramming Penalty)
    if (p0Alive && enemyTank && !enemyTank->isDestroyed) {
      for (b2ContactEdge *edge = myTank->body->GetContactList(); edge; edge = edge->next) {
        if (edge->contact->IsTouching() && edge->other == enemyTank->body) {
          reward -= 0.02f; // Phạt ôm địch
          break;
        }
      }
    }

    // 3. Shaping Reward: Vùng chiến đấu tối ưu
    //    CHỈ DÙNG khi KHÔNG có bản đồ (A* sẽ thay thế distance shaping)
    if (!game->mapEnabled) {
      float currentDist = getRawDistanceToEnemy(0);
      if (p0Alive && currentDist < 1000.0f) {
        if (currentDist > 350.0f) {
          reward -= 0.02f;
        } else if (currentDist < 80.0f) {
          reward -= 0.05f;
        }
      }

    }

    // 3b. A* Following Reward: Thưởng khi di chuyển theo hướng waypoint
    //     Chỉ dùng khi CÓ bản đồ — thay thế distance shaping
    if (game->mapEnabled && p0Alive && enemyTank && !enemyTank->isDestroyed) {
      int pathDist = 0;
      b2Vec2 waypoint =
          game->map.GetNextWaypoint(game->world, myTank->body->GetPosition(),
                                    enemyTank->body->GetPosition(), pathDist);
      b2Vec2 toWP = waypoint - myTank->body->GetPosition();
      float wpAbsAngle = atan2f(-toWP.x, toWP.y);
      float wpRelAngle = wpAbsAngle - myTank->body->GetAngle();
      float wpFacing = cosf(wpRelAngle);

      // Thưởng khi hướng về waypoint VÀ đang tiến tới
      if (wpFacing > 0.7f && action0.size() == 3 && action0[0] == 1) {
        reward += 0.005f; // Giảm từ 0.025 để khuyến khích bám đường an toàn
      }
    }

    // 4. Facing Reward: Thưởng khi hướng mặt đúng về phía địch
    if (p0Alive && enemyTank && !enemyTank->isDestroyed) {
      b2Vec2 toEnemy =
          enemyTank->body->GetPosition() - myTank->body->GetPosition();
      float absAngle = atan2f(-toEnemy.x, toEnemy.y);
      float relAngle = absAngle - myTank->body->GetAngle();
      float facingScore = cosf(relAngle);
      if (facingScore > 0.85f) {
        reward += 0.004f; // Giảm từ 0.015 để khuyến khích bám đuổi
      }
    }



    // Kiểm tra điều kiện kết thúc
    bool isTimeout = (currentStep >= maxSteps);
    bool done = game->needsRestart || isTimeout || (!p0Alive);

    if (isTimeout && p0Alive) {
      if (trainingMode == 2) {
        reward += 100.0f; // CHẾ ĐỘ NÉ TRÁNH: Sống sót = THẮNG!
      } else {
        reward -= 100.0f; // CHẾ ĐỘ CHIẾN ĐẤU: Phạt câu giờ ngang bị giết
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
      } else {
        for(int i=0; i<8; i++) state.push_back(0.0f); // [5-12]
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
        if (b->ownerPlayerIndex != playerIdx && b->time > 0.0f) {
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

      // Nhóm 7: Previous Action One-Hot (8 Tham số)
      std::vector<int> lastAct = (playerIdx == 0) ? lastAction0 : lastAction1;
      int mMove = lastAct[0];
      int mTurn = lastAct[1];
      int mShoot = lastAct[2];
      
      // Move (0, 1, 2)
      state.push_back(mMove == 0 ? 1.0f : 0.0f); // [37]
      state.push_back(mMove == 1 ? 1.0f : 0.0f); // [38]
      state.push_back(mMove == 2 ? 1.0f : 0.0f); // [39]
      
      // Turn (0, 1, 2)
      state.push_back(mTurn == 0 ? 1.0f : 0.0f); // [40]
      state.push_back(mTurn == 1 ? 1.0f : 0.0f); // [41]
      state.push_back(mTurn == 2 ? 1.0f : 0.0f); // [42]
      
      // Shoot (0, 1)
      state.push_back(mShoot == 0 ? 1.0f : 0.0f); // [43]
      state.push_back(mShoot == 1 ? 1.0f : 0.0f); // [44]

    } else {
      // Tank dead -> fill 45 zeros
      state.insert(state.end(), 45, 0.0f);
    }

    return state;
  }

  std::vector<int> getBotAction(int level, int playerIdx) {
      Bot bot(level, playerIdx);
      TankActions acts = bot.GetAction(game);
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
      .def(py::init<int, bool, bool, int>(), py::arg("num_players") = 2,
           py::arg("map_enabled") = true, py::arg("items_enabled") = true,
           py::arg("training_mode") = 0)
      .def("reset", &RLEnv::reset)
      .def("step", &RLEnv::step, py::arg("action0"),
           py::arg("action1") = std::vector<int>())
      .def("get_state", &RLEnv::getState, py::arg("playerIdx"))
      .def("render", &RLEnv::render)
      .def("get_bot_action", &RLEnv::getBotAction, py::arg("level"), py::arg("playerIdx"));
}
