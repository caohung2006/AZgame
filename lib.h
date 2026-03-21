#pragma once
#include <raylib.h>
#include <box2d/box2d.h>
#include <vector>
#include <math.h>
#include <string>

using namespace std;

const float SCALE = 30.0f;

// Định nghĩa kích thước màn hình chung cho toàn bộ Project
const int SCREEN_WIDTH = 1024;
const int SCREEN_HEIGHT = 768;

// 1. Đưa định nghĩa class Tank sang file header để các file khác nhận diện được
class Tank {
public:
    b2Body* body;
    
    int playerIndex; // Lưu số thứ tự của người chơi (0, 1, 2, 3)
    // Khai báo các biến lưu trữ phím bấm như một thuộc tính của Xe tăng
    int forwardKey;
    int backwardKey;
    int turnLeftKey;
    int turnRightKey;
    int shootKey;
    float shootCooldownTimer; // Bộ đếm thời gian hồi chiêu khi bắn đạn
    bool isDestroyed; // Cờ đánh dấu xe tăng đã bị phá huỷ

    void Init(int _playerIndex, int _forwardKey = 0, int _backwardKey = 0, int _turnLeftKey = 0, int _turnRightKey = 0, int _shootKey = 0);  // Đổi Constructor thành hàm Init để tránh lỗi văng game
    void Move();
    void Info();
};

// 3. Khai báo lớp Đạn (Bullet)
class Bullet {
public:
    b2Body* body;
    float time; // Biến đếm ngược thời gian tồn tại của đạn (giây)
    Bullet(b2Vec2 position, b2Vec2 velocity);
    pair<float, float> Info();
};

extern b2World world;
extern vector<b2Body*> walls;
extern vector<Tank*> tanks; // Báo cho hệ thống biết danh sách xe tăng
extern vector<Bullet*> bullets; // Danh sách chứa các viên đạn
extern int playerScores[4]; // Mảng lưu điểm số của 4 người chơi
extern int numPlayersGlobal; // Biến toàn cục lưu số lượng người chơi

void Lifetime(float dt);
void BuildMap();
void RunGame();
void DrawGame();
int ShowPlayerCountScreen();
void ShowKeyBindingScreen(int& fw, int& bw, int& tl, int& tr, int& sh, int playerIndex);