#include "ui.h"

int UI::ShowPlayerCountScreen() {
    int count = 0;
    while (!WindowShouldClose() && count == 0) {
        int key = GetKeyPressed();
        if (key >= KEY_ONE && key <= KEY_FOUR) count = key - KEY_ZERO;
        BeginDrawing(); ClearBackground(RAYWHITE);
        DrawText("CHON SO LUONG NGUOI CHOI (1 - 4)", SCREEN_WIDTH/2 - MeasureText("CHON SO LUONG NGUOI CHOI (1 - 4)", 30)/2, 250, 30, DARKBLUE);
        EndDrawing();
    }
    return (count == 0) ? 1 : count;
}

void UI::ShowKeyBindingScreen(int& fw, int& bw, int& tl, int& tr, int& sh, int playerIndex) {
    int state = 0; const char* prompts[] = {"DI TIEN", "DI LUI", "XOAY TRAI", "XOAY PHAI", "BAN DAN"};
    while (!WindowShouldClose() && state < 5) {
        int key = GetKeyPressed();
        if (key > 0) { 
            if (state == 0) fw = key; else if (state == 1) bw = key; else if (state == 2) tl = key; else if (state == 3) tr = key; else if (state == 4) sh = key;
            state++;
        }
        BeginDrawing(); ClearBackground(RAYWHITE);
        DrawText(TextFormat("CAI DAT PHIM CHO NGUOI CHOI %d", playerIndex), 300, 150, 30, DARKBLUE);
        if (state < 5) DrawText(prompts[state], 400, 300, 20, RED);
        DrawText(TextFormat("Phim DI TIEN: %c", state>0?(char)fw:'?'), 350, 420, 20, state>0?DARKGREEN:GRAY);
        DrawText(TextFormat("Phim DI LUI: %c", state>1?(char)bw:'?'), 350, 450, 20, state>1?DARKGREEN:GRAY);
        DrawText(TextFormat("Phim XOAY TRAI: %c", state>2?(char)tl:'?'), 350, 480, 20, state>2?DARKGREEN:GRAY);
        DrawText(TextFormat("Phim XOAY PHAI: %c", state>3?(char)tr:'?'), 350, 510, 20, state>3?DARKGREEN:GRAY);
        DrawText(TextFormat("Phim BAN DAN: %c", state>4?(char)sh:'?'), 350, 540, 20, state>4?DARKGREEN:GRAY);
        EndDrawing();
    }
}

void UI::DrawHUD(int scores[], int numPlayers) {
    // DrawText("WASD to move", 10, 10, 20, BLACK);
    DrawRectangle(SCREEN_WIDTH - 120, 10, 110, 30, LIGHTGRAY); DrawRectangleLines(SCREEN_WIDTH - 120, 10, 110, 30, DARKGRAY); DrawText("SETTINGS", SCREEN_WIDTH - 105, 16, 20, DARKGRAY);
    int secW = SCREEN_WIDTH / numPlayers;
    for (int i = 0; i < numPlayers; i++) {
        const char* txt = TextFormat("Player %d: %d", i + 1, scores[i]);
        DrawText(txt, (i * secW) + (secW / 2) - MeasureText(txt, 24)/2, SCREEN_HEIGHT - 40, 24, DARKBLUE);
    }
}

bool UI::CheckSettingsButtonClicked() {
    if (IsMouseButtonPressed(MOUSE_BUTTON_LEFT)) {
        Vector2 mp = GetMousePosition();
        return (mp.x >= SCREEN_WIDTH - 120 && mp.x <= SCREEN_WIDTH - 10 && mp.y >= 10 && mp.y <= 40);
    }
    return false;
}