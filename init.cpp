#include "lib.h"

// Hàm hiển thị màn hình chọn số lượng người chơi
int ShowPlayerCountScreen() {
    int count = 0;
    while (!WindowShouldClose() && count == 0) {
        int key = GetKeyPressed();
        if (key >= KEY_ONE && key <= KEY_FOUR) {
            count = key - KEY_ZERO; // Trừ đi mã ASCII gốc để ra số lượng (1 đến 4)
        }

        BeginDrawing();
        ClearBackground(RAYWHITE);
        DrawText("CHON SO LUONG NGUOI CHOI (1 - 4)", SCREEN_WIDTH/2 - MeasureText("CHON SO LUONG NGUOI CHOI (1 - 4)", 30)/2, 250, 30, DARKBLUE);
        DrawText("Bam cac phim 1, 2, 3 hoac 4 tren ban phim de chon...", SCREEN_WIDTH/2 - MeasureText("Bam cac phim 1, 2, 3 hoac 4 tren ban phim de chon...", 20)/2, 320, 20, DARKGRAY);
        EndDrawing();
    }
    return (count == 0) ? 1 : count; // Mặc định là 1 người chơi nếu lỡ tắt cửa sổ
}

// Hàm hiển thị màn hình cài đặt phím và lưu kết quả vào các biến tham chiếu
void ShowKeyBindingScreen(int& fw, int& bw, int& tl, int& tr, int& sh, int playerIndex) {
    int state = 0; // Trạng thái: 0=Tiến, 1=Lùi, 2=Trái, 3=Phải, 4=Bắn
    const char* prompts[] = {
        "Nhan phim cho chuc nang: DI TIEN (Forward)",
        "Nhan phim cho chuc nang: DI LUI (Backward)",
        "Nhan phim cho chuc nang: XOAY TRAI (Turn Left)",
        "Nhan phim cho chuc nang: XOAY PHAI (Turn Right)",
        "Nhan phim cho chuc nang: BAN DAN (Shoot)"
    };

    // Gán tạm phím mặc định đề phòng người chơi tắt cửa sổ giữa chừng
    fw = KEY_W; bw = KEY_S; tl = KEY_A; tr = KEY_D; sh = KEY_Q;

    // Vòng lặp giao diện cài đặt (Thoát khi đủ 5 phím)
    while (!WindowShouldClose() && state < 5) {
        int key = GetKeyPressed();
        
        // Nếu có phím được bấm (mã phím > 0)
        if (key > 0) { 
            if (state == 0) fw = key;
            else if (state == 1) bw = key;
            else if (state == 2) tl = key;
            else if (state == 3) tr = key;
            else if (state == 4) sh = key;
            state++; // Chuyển sang cài đặt phím tiếp theo
        }

        BeginDrawing();
        ClearBackground(RAYWHITE);
        
        const char* title = TextFormat("CAI DAT PHIM CHO NGUOI CHOI %d", playerIndex);
        DrawText(title, SCREEN_WIDTH/2 - MeasureText(title, 30)/2, 150, 30, DARKBLUE);
        
        if (state < 5) {
            DrawText(prompts[state], SCREEN_WIDTH/2 - MeasureText(prompts[state], 20)/2, 300, 20, RED);
        }

        // Hiển thị tiến trình cài đặt (Ép kiểu char để hiện chữ cái tương ứng của phím)
        DrawText(TextFormat("Phim DI TIEN: %c", state > 0 ? (char)fw : '?'), 350, 420, 20, state > 0 ? DARKGREEN : GRAY);
        DrawText(TextFormat("Phim DI LUI: %c", state > 1 ? (char)bw : '?'), 350, 450, 20, state > 1 ? DARKGREEN : GRAY);
        DrawText(TextFormat("Phim XOAY TRAI: %c", state > 2 ? (char)tl : '?'), 350, 480, 20, state > 2 ? DARKGREEN : GRAY);
        DrawText(TextFormat("Phim XOAY PHAI: %c", state > 3 ? (char)tr : '?'), 350, 510, 20, state > 3 ? DARKGREEN : GRAY);
        DrawText(TextFormat("Phim BAN DAN: %c", state > 4 ? (char)sh : '?'), 350, 540, 20, state > 4 ? DARKGREEN : GRAY);

        EndDrawing();
    }
}