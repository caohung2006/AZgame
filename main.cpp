#include "game.h"
#include <ctime>
#include <cstdlib>

int main() {
    srand((unsigned int)time(NULL)); // Khởi tạo mầm random dựa trên thời gian thực
    Game game;
    game.Run();
    return 0;
}