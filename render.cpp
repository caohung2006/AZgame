#include "game.h"

void Game::Draw() {
    BeginDrawing(); ClearBackground(RAYWHITE);
    map.Draw();
    portal.Draw();
    for (Tank* t : tanks) t->Draw();
    for (Item* item : items) item->Draw();
    for (Bullet* b : bullets) b->Draw();
    UI::DrawHUD(playerScores, numPlayers);
    EndDrawing();
}