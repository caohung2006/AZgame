#pragma once
#include "constants.h"

/**
 * @class UI
 * @brief Lớp tĩnh quản lý giao diện tương tác và hiển thị (User Interface)
 * Bao gồm: nút bánh răng, màn hình cài đặt tổng, cài đặt phím, HUD điểm số
 */
class UI {
public:
    /// Mở màn hình Cài đặt tổng hợp: số người chơi, bật/tắt portal & item, cài phím
    static void ShowSettingsScreen(int& numPlayers, bool& portalsEnabled, bool& itemsEnabled,
        std::vector<PlayerConfig>& configs);

    /// Mở màn hình gán phím cho 1 người chơi (6 phím: di chuyển, bắn, khiên)
    static void ShowKeyBindingScreen(int& fw, int& bw, int& tl, int& tr, int& sh, int& shield, int playerIndex);

    /// Vẽ HUD: nút bánh răng + bảng điểm
    static void DrawHUD(int playerScores[], int numPlayers);

    /// Kiểm tra click vào nút bánh răng
    static bool CheckSettingsButtonClicked();
};