#pragma once
#include <raylib.h>
#include <box2d/box2d.h>
#include <vector>
#include <string>
#include <math.h>

using namespace std;

// Tỷ lệ chuyển đổi giữa pixel (Raylib) và mét (Box2D)
// Lưu ý: Box2D mô phỏng vật lý chính xác nhất với các vật thể từ 0.1 đến 10 mét.
const float SCALE = 30.0f;

// Kích thước độ phân giải của cửa sổ trò chơi
const int SCREEN_WIDTH = 1024;
const int SCREEN_HEIGHT = 768;