#pragma once
#include <raylib.h>
#include <box2d/box2d.h>
#include <vector>
#include <math.h>

using namespace std;

const float SCALE = 30.0f;


// 1. Đưa định nghĩa class Tank sang file header để các file khác nhận diện được
class Tank {
public:
    b2Body* body;
    void Init();  // Đổi Constructor thành hàm Init để tránh lỗi văng game
    void Move();
    void Info();
};

// 3. Khai báo lớp Đạn (Bullet)
class Bullet {
public:
    b2Body* body;
    Bullet(b2Vec2 position, b2Vec2 velocity);
    pair<float, float> Info();
};

extern b2World world;
extern vector<b2Body*> walls;
extern Tank tank; // 2. Báo cho hệ thống biết biến tank có tồn tại
extern vector<Bullet*> bullets; // Danh sách chứa các viên đạn

void map();
void WorldAZ();
void Render();