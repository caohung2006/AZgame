#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include "game.h"
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
    int maxSteps;
    int currentStep;
    int lastScores[4];

public:
    // Hàm khởi tạo môi trường
    RLEnv(int num_players = 2, bool map_enabled = false, bool items_enabled = false) {
        game = new Game(); // Tạo đối tượng Game mới
        game->numPlayers = num_players; // Số lượng người chơi
        game->mapEnabled = map_enabled; // Có sử dụng bản đồ (vật cản) không
        game->itemsEnabled = items_enabled; // Có xuất hiện vật phẩm không
        game->portalsEnabled = items_enabled; // Cổng dịch chuyển
        maxSteps = 5000; // Số bước tối đa trong một ván chơi
        currentStep = 0; // Bước hiện tại
        for(int i=0; i<4; i++) lastScores[i] = 0; // Lưu trữ điểm số trước đó để tính phần thưởng
    }

    ~RLEnv() {
        delete game;
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
        return getState(0); // Trả về trạng thái của người chơi 0
    }

    /**
     * @brief Thực hiện một hành động (Step) trong môi trường.
     * @param action Mã hành động (0-5) từ phía AI Python gửi sang.
     * @return Một Python Tuple chứa (Trạng thái mới, Phần thưởng, Ván chơi kết thúc chưa).
     */
    py::tuple step(int action) {
        TankActions tankActions;
        // Map mã hành động số nguyên sang các hành động của xe tăng
        if (action == 1) tankActions.forward = true;   // Tiến
        if (action == 2) tankActions.backward = true;  // Lùi
        if (action == 3) tankActions.turnLeft = true;  // Quay trái
        if (action == 4) tankActions.turnRight = true; // Quay phải
        if (action == 5) tankActions.shoot = true;     // Bắn

        std::vector<TankActions> all_actions(game->numPlayers);
        all_actions[0] = tankActions; // Người chơi 0 là AI
        // Dự phòng cho các bot khác chơi cùng nếu cần

        // Cập nhật logic game với thời gian cố định 60 FPS (khoảng 0.016s mỗi bước)
        game->Update(all_actions, 1.0f/60.0f);
        currentStep++;

        float reward = 0.0f; // Điểm thưởng cho bước này
        
        // --- LOGIC TÍNH PHẦN THƯỞNG (Reward) ---
        
        // 1. Thưởng khi bắn trúng kẻ địch (điểm tăng)
        int scoreDiff = game->playerScores[0] - lastScores[0];
        if (scoreDiff > 0) {
            reward += 50.0f * scoreDiff; // Điểm thưởng lớn khi lập công
            lastScores[0] = game->playerScores[0];
        }
        
        // 2. Kiểm tra xem xe AI (index 0) còn sống hay không
        bool p0Alive = false;
        for(auto t : game->tanks) {
            if (t->playerIndex == 0) p0Alive = true;
        }

        if (!p0Alive) {
            reward -= 50.0f; // Bị tiêu diệt -> Phạt nặng để AI học cách né tránh.
        } else {
            reward += 0.01f; // Thưởng nhỏ mỗi tick nếu còn sống (khuyến khích sinh tồn).
        }

        // Kiểm tra điều kiện kết thúc: Engine yêu cầu, hết bước tối đa, hoặc AI chết.
        bool done = game->needsRestart || (currentStep >= maxSteps) || (!p0Alive);

        auto state = getState(0); // Lấy trạng thái mới sau khi thực hiện hành động
        return py::make_tuple(state, reward, done);
    }

    /**
     * @brief Trích xuất các đặc trưng trạng thái (Observations) để AI ra quyết định.
     * Các giá trị thường được chuẩn hóa về khoảng [0, 1] hoặc [-1, 1] để mạng neural hoạt động tốt.
     */
    std::vector<float> getState(int playerIdx) {
        std::vector<float> state;
        
        // --- Đặc trưng của xe AI (Bản thân) ---
        Tank* myTank = nullptr;
        for (auto t: game->tanks) {
            if (t->playerIndex == playerIdx) { myTank = t; break; }
        }

        if (myTank) {
            b2Vec2 pos = myTank->body->GetPosition();
            float angle = myTank->body->GetAngle();
            state.push_back(pos.x / SCREEN_WIDTH);  // X chuẩn hóa (0-1)
            state.push_back(pos.y / SCREEN_HEIGHT); // Y chuẩn hóa (0-1)
            state.push_back(cosf(angle));           // Hướng hướng về trục X
            state.push_back(sinf(angle));           // Hướng hướng về trục Y
            state.push_back(myTank->ammo / 10.0f);  // Lượng đạn còn lại
        } else {
            state.insert(state.end(), 5, 0.0f); // Nếu xe chết, toàn bộ đặc trưng là 0
        }

        // --- Đặc trưng của 1 Kẻ địch ---
        Tank* enemyTank = nullptr;
        for (auto t: game->tanks) {
            if (t->playerIndex != playerIdx) { enemyTank = t; break; }
        }

        if (enemyTank) {
            b2Vec2 pos = enemyTank->body->GetPosition();
            float angle = enemyTank->body->GetAngle();
            state.push_back(pos.x / SCREEN_WIDTH);
            state.push_back(pos.y / SCREEN_HEIGHT);
            state.push_back(cosf(angle));
            state.push_back(sinf(angle));
        } else {
            state.insert(state.end(), 4, 0.0f); // Nếu không có kẻ địch (hoặc đã chết)
        }

        // Tổng cộng trả về vector 9 số thực.
        return state;
    }
};

PYBIND11_MODULE(azgame_env, m) {
    m.doc() = "Môi trường học tăng cường Pybind11 cho AZGame xe tăng";
    py::class_<RLEnv>(m, "RLEnv")
        .def(py::init<int, bool, bool>(), 
             py::arg("num_players") = 2, 
             py::arg("map_enabled") = true, 
             py::arg("items_enabled") = true)
        .def("reset", &RLEnv::reset)
        .def("step", &RLEnv::step);
}
