# 🛡️ AZ Tank Game - Advanced AI Edition

AZ Tank là một dự án game bắn xe tăng 2D hiệu suất cao, được tối ưu hóa cho cả **người chơi (Local Multiplayer)** và **huấn luyện trí tuệ nhân tạo (Reinforcement Learning)**. 

Dự án sử dụng bộ đôi sức mạnh: **Raylib** cho đồ họa/UI mượt mà và **Box2D** cho mô phỏng vật lý chính xác đến từng pixel.

---

## 🚀 Tính năng nổi bật

### 🎮 Gameplay & Physics
- **Local Multiplayer**: Hỗ trợ 1–4 người chơi đấu với nhau hoặc đấu với Bot trên cùng một máy.
- **Vật lý Box2D**: Đạn nảy dội tường, va chạm thực tế, hiệu ứng đẩy xe khi bắn.
- **Vũ khí đa dạng**: Gatling Gun, Frag Mine (đạn nổ chùm), Homing Missile (tên lửa đuổi), và Death Ray (tia laser).
- **Cổng dịch chuyển (Portal)**: Hệ thống dịch chuyển tức thời ngẫu nhiên trên bản đồ.
- **Mê cung ngẫu nhiên**: Sinh bản đồ tự động bằng thuật toán *Recursive Backtracker* với các đường tắt (shortcuts) thông minh.

### 🤖 Hệ thống Bot AI Siêu cấp (NEW!)
Hệ thống Bot AI được xây dựng theo 5 cấp độ khó, sử dụng các thuật toán tiên tiến nhất:
- **A* Pathfinding (Grid-based)**: Tìm đường đi ngắn nhất qua mê cung phức tạp.
- **Fat Raycast (Multi-beam)**: Sử dụng hệ thống 5 tia laser để quét chướng ngại vật, đảm bảo xe tăng không bao giờ bị kẹt góc hoặc va chạm khi rẽ.
- **String Pulling (Path Smoothing)**: Làm mượt đường đi dích dắc của A* thành các đường thẳng tắp, dứt khoát.
- **Path Commitment**: Cơ chế ghi nhớ đường đi thông minh, chỉ tính toán lại A* khi mục tiêu thay đổi ô Caro, giúp tối ưu CPU và loại bỏ hiện tượng AI bị "co giật".
- **Predictive Aiming (Xạ thủ)**: Bot cấp độ 5 có khả năng tính toán vận tốc địch để bắn đón đầu (Aiming at future position).

---

## 🛠️ Kiến trúc Mã nguồn

Game được thiết kế theo mô hình **Logic - Renderer Decoupling**, cho phép logic game chạy độc lập hoàn toàn với đồ họa (thích hợp cho huấn luyện AI Headless).

### Core Logic (Header-only compatible with RL)
| Thành phần | Vai trò |
|---|---|
| `bot.h/.cpp` | **Bộ não AI**: A*, String Pulling, Predictive Aiming, Dodging logic. |
| `map.h/.cpp` | Quản lý lưới (Grid), sinh mê cung và thuật toán tìm đường A*. |
| `game.h/.cpp` | Engine chính điều phối toàn bộ thế giới vật lý và luật chơi. |
| `tank.h/.cpp` | Logic vật lý xe tăng, quản lý HP, Khiên và Vũ khí. |
| `bullet.h/.cpp` | Mô phỏng các loại đạn và hiệu ứng đặc biệt. |

### Graphics & UI
| Thành phần | Vai trò |
|---|---|
| `renderer.h/.cpp` | Hệ thống vẽ 2D cao cấp, bao gồm cả **Debug Grid** để quan sát tư duy của AI. |
| `ui.h/.cpp` | Giao diện cài đặt (Settings) và bảng điểm (HUD). |
| `main.cpp` | Điểm khởi đầu của ứng dụng, kết nối Input với Game Engine. |

---

## ⚙️ Hướng dẫn Cài đặt & Biên dịch

### Yêu cầu
- **CMake**: ≥ 3.10
- **Trình biên dịch**: GCC (MinGW-w64) hoặc MSVC.
- **Thư viện**: Raylib (Box2D đã được tích hợp sẵn).

### Biên dịch nhanh (Windows - MSYS2/MinGW)
```bash
# Clone dự án
git clone https://github.com/caohung2006/AZgame.git
cd AZgame

# Tạo thư mục build
mkdir build && cd build

# Cấu hình và biên dịch
cmake -G "MinGW Makefiles" ..
mingw32-make -j4

# Chạy game
./AZgame.exe
```

---

## 🕹️ Điều khiển (Mặc định)

| Hành động | Player 1 | Player 2 | Player 3 | Player 4 |
|---|:---:|:---:|:---:|:---:|
| **Di chuyển** | `W/A/S/D` | `↑/←/↓/→` | `I/J/K/L` | `Numpad 8/4/5/6` |
| **Bắn** | `Q` | `/` | `U` | `Numpad 7` |
| **Khiên** | `E` | `.` | `O` | `Numpad 9` |

> 💡 *Bạn có thể thay đổi phím điều khiển bất cứ lúc nào trong menu Settings (biểu tượng bánh răng ⚙️).*

---

## 📈 Tầm nhìn Reinforcement Learning
Dự án được thiết kế để các Agent AI (Python/PPO) có thể tương tác trực tiếp với `game.h` thông qua lớp Wrapper. Môi trường hỗ trợ quan sát theo lưới (Grid observation) hoặc tọa độ vật lý, cung cấp phần thưởng (reward shaping) dựa trên khả năng di chuyển và chiến đấu.

---
*Phát triển bởi [caohung2006](https://github.com/caohung2006)*