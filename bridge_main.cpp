#include <raylib.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

#include <box2d/box2d.h>

#include "Constants.h"
#include "game.h"
#include "renderer.h"

namespace {
const char* kControlFile = "bridge_control.txt";
const char* kStateTmpFile = "bridge_state.tmp";
const char* kStateFile = "bridge_state.json";
const char* kWaypointsFile = "bridge_waypoints.txt";

struct WaypointOverlay {
  std::vector<Vector2> points;
  int currentIdx = 0;
  bool valid = false;
};

std::vector<TankActions> ReadActions(int numPlayers) {
  std::vector<TankActions> actions(std::max(numPlayers, 1));
  std::ifstream in(kControlFile);
  if (!in) {
    return actions;
  }

  int p = 0;
  int fw = 0, bw = 0, tl = 0, tr = 0, sh = 0, shield = 0;
  while (in >> p >> fw >> bw >> tl >> tr >> sh >> shield) {
    if (p < 0 || p >= static_cast<int>(actions.size())) {
      continue;
    }
    actions[p].forward = fw != 0;
    actions[p].backward = bw != 0;
    actions[p].turnLeft = tl != 0;
    actions[p].turnRight = tr != 0;
    actions[p].shoot = sh != 0;
    actions[p].shield = shield != 0;
  }

  return actions;
}

WaypointOverlay ReadWaypointsOverlay() {
  WaypointOverlay out;
  std::ifstream in(kWaypointsFile);
  if (!in) {
    return out;
  }
  out.valid = true;

  std::string line;
  while (std::getline(in, line)) {
    if (line.empty()) {
      continue;
    }

    std::istringstream ls(line);
    std::string tag;
    ls >> tag;
    if (tag == "idx") {
      ls >> out.currentIdx;
      continue;
    }

    float x = 0.0f;
    float y = 0.0f;
    std::istringstream ps(line);
    if (ps >> x >> y) {
      out.points.push_back({x, y});
    }
  }

  if (out.currentIdx < 0) {
    out.currentIdx = 0;
  }
  if (!out.points.empty() && out.currentIdx >= static_cast<int>(out.points.size())) {
    out.currentIdx = static_cast<int>(out.points.size()) - 1;
  }

  return out;
}

void DrawWaypointsOverlay(const WaypointOverlay& overlay) {
  if (overlay.points.empty()) {
    return;
  }

  Vector2 prev = {overlay.points[0].x * SCALE,
                  SCREEN_HEIGHT - overlay.points[0].y * SCALE};

  for (size_t i = 0; i < overlay.points.size(); ++i) {
    Vector2 p = {overlay.points[i].x * SCALE,
                 SCREEN_HEIGHT - overlay.points[i].y * SCALE};
    

    DrawCircleV(p, 3.0f, ColorAlpha(BLUE, 0.95f));
    prev = p;
  }

  DrawText(TextFormat("wps: %d  idx: %d", static_cast<int>(overlay.points.size()),
                      overlay.currentIdx),
           12, 58, 18, MAROON);
}

void WriteState(const Game& game, float dt) {
  std::ostringstream ss;
  ss << "{\n";
  ss << "  \"screen_width\": " << SCREEN_WIDTH << ",\n";
  ss << "  \"screen_height\": " << SCREEN_HEIGHT << ",\n";
  ss << "  \"scale\": " << SCALE << ",\n";
  ss << "  \"dt\": " << dt << ",\n";
  ss << "  \"needs_restart\": " << (game.needsRestart ? "true" : "false") << ",\n";

  ss << "  \"scores\": [";
  for (int i = 0; i < 4; ++i) {
    if (i) ss << ", ";
    ss << game.playerScores[i];
  }
  ss << "],\n";

  ss << "  \"tanks\": [";
  for (size_t i = 0; i < game.tanks.size(); ++i) {
    if (i) ss << ",";
    const Tank* t = game.tanks[i];
    ss << "{"
       << "\"player_index\":" << t->playerIndex << ","
       << "\"x\":" << t->body->GetPosition().x << ","
       << "\"y\":" << t->body->GetPosition().y << ","
       << "\"angle\":" << t->body->GetAngle() << ","
       << "\"has_shield\":" << (t->hasShield ? "true" : "false")
       << "}";
  }
  ss << "],\n";

  ss << "  \"bullets\": [";
  for (size_t i = 0; i < game.bullets.size(); ++i) {
    if (i) ss << ",";
    const Bullet* b = game.bullets[i];
    ss << "{"
       << "\"x\":" << b->body->GetPosition().x << ","
       << "\"y\":" << b->body->GetPosition().y << ","
       << "\"owner\":" << b->ownerPlayerIndex
       << "}";
  }
  ss << "],\n";

  bool firstWall = true;
  ss << "  \"walls\": [";
  for (const auto* body : game.map.GetWalls()) {
    for (const auto* fixture = body->GetFixtureList(); fixture;
         fixture = fixture->GetNext()) {
      if (!firstWall) ss << ",";
      const auto aabb = fixture->GetAABB(0);
      ss << "{"
         << "\"min_x\":" << aabb.lowerBound.x << ","
         << "\"min_y\":" << aabb.lowerBound.y << ","
         << "\"max_x\":" << aabb.upperBound.x << ","
         << "\"max_y\":" << aabb.upperBound.y
         << "}";
      firstWall = false;
    }
  }
  ss << "]\n";

  ss << "}\n";

  std::ofstream out(kStateTmpFile, std::ios::trunc);
  out << ss.str();
  out.close();
  std::remove(kStateFile);
  std::rename(kStateTmpFile, kStateFile);
}
}  // namespace

int main() {
  std::srand(static_cast<unsigned int>(std::time(nullptr)));

  Game game;
  game.numPlayers = 2;
  game.itemsEnabled = false;
  game.portalsEnabled = false;
  game.shieldsEnabled = false;
  game.ResetMatch();

  std::ofstream(kControlFile, std::ios::trunc).close();
  std::ofstream(kWaypointsFile, std::ios::trunc).close();

  InitWindow(SCREEN_WIDTH, SCREEN_HEIGHT, "AZ Game Bridge");
  SetTargetFPS(60);
  WaypointOverlay waypointOverlay;

  while (!WindowShouldClose()) {
    if (game.needsRestart) {
      game.ResetMatch();
    }

    std::vector<TankActions> actions = ReadActions(game.numPlayers);

    // Player 2 can still be driven manually from keyboard.
    if (game.numPlayers > 1) {
      actions[1].forward = actions[1].forward || IsKeyDown(KEY_UP);
      actions[1].backward = actions[1].backward || IsKeyDown(KEY_DOWN);
      actions[1].turnLeft = actions[1].turnLeft || IsKeyDown(KEY_LEFT);
      actions[1].turnRight = actions[1].turnRight || IsKeyDown(KEY_RIGHT);
      actions[1].shoot = actions[1].shoot || IsKeyPressed(KEY_SLASH);
      actions[1].shield = actions[1].shield || IsKeyPressed(KEY_PERIOD);
    }

    float dt = GetFrameTime();
    game.Update(actions, dt);
    Renderer::Update(game, dt);
    WriteState(game, dt);
    WaypointOverlay latestOverlay = ReadWaypointsOverlay();
    if (latestOverlay.valid) {
      waypointOverlay = latestOverlay;
    }

    BeginDrawing();
    ClearBackground({245, 240, 230, 255});
    Renderer::DrawWorld(game);
    DrawWaypointsOverlay(waypointOverlay);
    DrawText("P0: Python via bridge_control.txt", 12, 10, 18, DARKGRAY);
    DrawText("P1: Arrow Keys + / + .", 12, 34, 18, DARKGRAY);
    EndDrawing();
  }

  CloseWindow();
  return 0;
}
