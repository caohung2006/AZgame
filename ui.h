#pragma once
#include "constants.h"

class UI {
public:
    static int ShowPlayerCountScreen();
    static void ShowKeyBindingScreen(int& fw, int& bw, int& tl, int& tr, int& sh, int playerIndex);
    static void DrawHUD(int playerScores[], int numPlayers);
    static bool CheckSettingsButtonClicked();
};