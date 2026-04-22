#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include "game.h"
#include "renderer.h"
#include <raylib.h>
#include <tuple>
#include <vector>
#include <cmath>

namespace py = pybind11;

/**
 * @class RLEnv
 * @brief Lớp bao bọc (Wrapper) môi trường trò chơi để tương thích với các thư viện Reinforcement Learning (như OpenAI Gym).
 * Lớp này quản lý việc khởi tạo trò chơi, thực hiện các bước đi (step) và thu thập trạng thái (state).
 */
class RLEnv {
private:
    Game* game;
    Renderer* renderer = nullptr;
    bool isRendering = false;
    int maxSteps;
    int currentStep;
    int lastScores[4];
    float lastDistanceToEnemy;
    int trainingMode;

    float getRawDistanceToEnemy(int playerIdx) {
        Tank* myTank = nullptr;
        Tank* enemyTank = nullptr;
        for (auto t: game->tanks) {
            if (t->playerIndex == playerIdx) { myTank = t; }
            else if (!t->isDestroyed) { enemyTank = t; }
        }
        if (myTank && enemyTank) {
            b2Vec2 diff = myTank->body->GetPosition() - enemyTank->body->GetPosition();
            return diff.Length() * SCALE; // Khoảng cách tính bằng pixel
        }
        return 0.0f;
    }

public:
    // Hàm khởi tạo môi trường
    RLEnv(int num_players = 2, bool map_enabled = false, bool items_enabled = false, int training_mode = 0) {
        game = new Game(); // Tạo đối tượng Game mới
        game->numPlayers = num_players; // Số lượng người chơi
        game->mapEnabled = map_enabled; // Có sử dụng bản đồ (vật cản) không
        game->itemsEnabled = items_enabled; // Có xuất hiện vật phẩm không
        game->portalsEnabled = items_enabled; // Cổng dịch chuyển
        maxSteps = 5000; // Số bước tối đa trong một ván chơi
        currentStep = 0; // Bước hiện tại
        trainingMode = training_mode;
        for(int i=0; i<4; i++) lastScores[i] = 0; // Lưu trữ điểm số trước đó để tính phần thưởng
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
        renderer->Update(*game, 1.0f/60.0f);
        
        BeginDrawing();
        ClearBackground({20, 20, 25, 255}); // Nền tối
        if (renderer) renderer->DrawWorld(*game);
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
        for(int i=0; i<4; i++) lastScores[i] = game->playerScores[i];
        lastDistanceToEnemy = getRawDistanceToEnemy(0);
        return getState(0); // Trả về trạng thái của người chơi 0
    }

    /**
     * @brief Thực hiện một hành động (Step) trong môi trường.
     * @param action Mã hành động (0-5) từ phía AI Python gửi sang.
     * @return Một Python Tuple chứa (Trạng thái mới, Phần thưởng, Ván chơi kết thúc chưa).
     */
    py::tuple step(int action0, int action1 = -1) {
        TankActions tankActions0;
        // Map mã hành động số nguyên sang các hành động của xe tăng
        if (action0 == 0) tankActions0.forward = true;   // Tiến
        if (action0 == 1) tankActions0.backward = true;  // Lùi
        if (action0 == 2) tankActions0.turnLeft = true;  // Quay trái
        if (action0 == 3) tankActions0.turnRight = true; // Quay phải
        
        // Khóa súng của AI nếu đang ở chế độ Evasion (mode=2)
        if (action0 == 4 && trainingMode != 2) tankActions0.shoot = true;

        std::vector<TankActions> all_actions(game->numPlayers);
        all_actions[0] = tankActions0; // Người chơi 0 là AI

        // Người chơi 1 là đối thủ (nếu có action1 truyền tới)
        if (game->numPlayers > 1 && action1 >= 0 && action1 <= 4) {
            TankActions tankActions1;
            if (action1 == 0) tankActions1.forward = true;
            if (action1 == 1) tankActions1.backward = true;
            if (action1 == 2) tankActions1.turnLeft = true;
            if (action1 == 3) tankActions1.turnRight = true;
            if (action1 == 4) tankActions1.shoot = true;
            all_actions[1] = tankActions1;
        }

        // Cập nhật logic game với thời gian cố định 60 FPS (khoảng 0.016s mỗi bước)
        game->Update(all_actions, 1.0f/60.0f);
        currentStep++;

        float reward = -0.05f; // Điểm thưởng cho bước này (Time Penalty hạn chế AI câu giờ)
        
        // --- LOGIC TÍNH PHẦN THƯỞNG (Reward) ---
        
        Tank* myTank = nullptr;
        for (auto t : game->tanks) if (t->playerIndex == 0) myTank = t;

        // Phạt đâm tường
        if (myTank) {
            for (b2ContactEdge* edge = myTank->body->GetContactList(); edge; edge = edge->next) {
                if (edge->contact->IsTouching() && edge->other->GetType() == b2_staticBody) {
                    reward -= 2.0f; // Điểm âm vì đâm tường
                    break;
                }
            }
        }

        // 1. Thưởng khi bắn trúng kẻ địch (Giết địch +100)
        int scoreDiff = game->playerScores[0] - lastScores[0];
        if (scoreDiff > 0) {
            reward += 100.0f * scoreDiff; // Điểm thưởng lớn khi lập công
            lastScores[0] = game->playerScores[0];
        }
        
        // 2. Kiểm tra xem xe AI (index 0) còn sống hay không
        bool p0Alive = (myTank != nullptr && !myTank->isDestroyed);

        if (!p0Alive) {
            reward -= 100.0f; // Bị tiêu diệt (bị bắn trúng phạt -100 vì game 1-hit)
        } 
        
        // 3. Shaping Reward (Khoảng cách cũ - mới) * 0.1
        float currentDist = getRawDistanceToEnemy(0);
        if (p0Alive && currentDist > 0.1f) {
            reward += (lastDistanceToEnemy - currentDist) * 0.1f;
        }
        lastDistanceToEnemy = currentDist;

        // Kiểm tra điều kiện kết thúc: Engine yêu cầu, hết bước tối đa, hoặc AI chết.
        bool done = game->needsRestart || (currentStep >= maxSteps) || (!p0Alive);

        auto state = getState(0); // Lấy trạng thái mới sau khi thực hiện hành động
        return py::make_tuple(state, reward, done);
    }

    /**
     * @brief Trích xuất các đặc trưng trạng thái (Observations) để AI ra quyết định.
     * Các giá trị thường được chuẩn hóa về khoảng [0, 1] hoặc [-1, 1] để mạng neural hoạt động tốt.
     */
// Lớp hỗ trợ tia laser đo khoảng cách cho Radar
class RadarRayCastCallback : public b2RayCastCallback {
public:
    float closestFraction = 1.0f;
    float ReportFixture(b2Fixture* fixture, const b2Vec2& point, const b2Vec2& normal, float fraction) override {
        // Chỉ tính vật cản tĩnh (Tường)
        if (fixture->GetBody()->GetType() == b2_staticBody) {
            if (fraction < closestFraction) {
                closestFraction = fraction;
            }
            return fraction; // Trả về fraction giúp tối ưu kết quả tìm kiếm của Box2D
        }
        return -1.0f; // Bỏ qua vật thể khác (đạn, xe tăng khác)
    }
};

    std::vector<float> getState(int playerIdx) {
        std::vector<float> state;
        
        Tank* myTank = nullptr;
        Tank* enemyTank = nullptr;
        for (auto t: game->tanks) {
            if (t->playerIndex == playerIdx) { myTank = t; }
            else { enemyTank = t; }
        }

        if (myTank && !myTank->isDestroyed) {
            b2Vec2 myPos = myTank->body->GetPosition();
            float myAngle = myTank->body->GetAngle();

            // 1. Cảm biến Radar (8 tia cách nhau 45 độ)
            float rayLength = std::sqrt(SCREEN_WIDTH*SCREEN_WIDTH + SCREEN_HEIGHT*SCREEN_HEIGHT) / SCALE; // Đường chéo màn phình
            for (int i = 0; i < 8; i++) {
                float angle = myAngle + i * (PI / 4.0f);
                b2Vec2 p2 = myPos + rayLength * b2Vec2(-sinf(angle), cosf(angle));
                
                RadarRayCastCallback cb;
                game->world.RayCast(&cb, myPos, p2);
                state.push_back(cb.closestFraction); // Đã chuẩn hóa 0.0 -> 1.0
            }

            // --- Đặc trưng của Kẻ địch so với Bản thân ---
            if (enemyTank && !enemyTank->isDestroyed) {
                b2Vec2 toEnemy = enemyTank->body->GetPosition() - myPos;
                
                // Góc tới Enemy (so với hướng thẳng của mặt xe)
                // Vec chuẩn chỉ lên trên (-sin, cos). Ta dùng atan2 để lấy góc hướng về enemy.
                float absoluteAngleToEnemy = atan2f(-toEnemy.x, toEnemy.y); 
                float relativeAngle = absoluteAngleToEnemy - myAngle;
                
                // 2. Góc nhìn đến Địch (2 floats: cos và sin)
                state.push_back(cosf(relativeAngle));
                state.push_back(sinf(relativeAngle));

                // 3. Khoảng cách đường chim bay đến Địch (chuẩn hóa)
                state.push_back(std::min(1.0f, toEnemy.Length() / rayLength));
            } else {
                state.push_back(1.0f); // Không có địch -> Góc cos = 1
                state.push_back(0.0f); // sin = 0
                state.push_back(1.0f); // Khoảng cách xa nhất = 1.0
            }

            // 4. Số lượng đạn mình đang bay (chuẩn hóa tỷ lệ theo 5 đạn)
            int myBulletCount = 0;
            for (auto b : game->bullets) {
                if (b->ownerPlayerIndex == playerIdx && b->time > 0.0f) myBulletCount++;
            }
            state.push_back(std::min(1.0f, myBulletCount / 5.0f));

            // 5. Máu hiện tại (Chúng ta đang dùng 1-hit kill, nên sống là 1.0)
            state.push_back(1.0f);
        } else {
            // Xe chết (hoặc lỗi), chèn đủ 13 số 0.0f
            state.insert(state.end(), 13, 0.0f);
        }

        return state;
    }
};

PYBIND11_MODULE(azgame_env, m) {
    m.doc() = "Môi trường học tăng cường Pybind11 cho AZGame xe tăng";
    py::class_<RLEnv>(m, "RLEnv")
        .def(py::init<int, bool, bool, int>(), 
             py::arg("num_players") = 2, 
             py::arg("map_enabled") = true, 
             py::arg("items_enabled") = true,
             py::arg("training_mode") = 0)
        .def("reset", &RLEnv::reset)
        .def("step", &RLEnv::step, py::arg("action0"), py::arg("action1") = -1)
        .def("get_state", &RLEnv::getState, py::arg("playerIdx"))
        .def("render", &RLEnv::render);
}
