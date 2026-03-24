#pragma once
#include "constants.h"

// Lớp tĩnh (Static Class) quản lý giao diện tương tác và hiển thị (User Interface)
class UI {
public:
    // Mở màn hình chọn số người chơi, trả về số lượng được chọn (1 đến 4)
    static int ShowPlayerCountScreen();
    
    // Mở màn hình cho phép người chơi ấn phím để thay đổi nút điều khiển mặc định
    static void ShowKeyBindingScreen(int& fw, int& bw, int& tl, int& tr, int& sh, int playerIndex);
    
    // Vẽ lớp hiển thị thông tin phía trên (Heads-Up Display): Điểm số, chữ hướng dẫn
    static void DrawHUD(int playerScores[], int numPlayers);
    
    // Kiểm tra xem người dùng có nhấp chuột vào khung nút SETTINGS hay không
    static bool CheckSettingsButtonClicked();
};