# AZ Tank Game

AZ Tank là một tựa game bắn xe tăng 2D cổ điển hỗ trợ **1–4 người chơi** trên cùng 1 máy (Local Multiplayer). Game được phát triển bằng **C++**, sử dụng **Raylib** (đồ họa/nhập liệu) và **Box2D** (vật lý).

### ✨ Kiến trúc đặc biệt
Game logic **tách hoàn toàn** khỏi đồ họa — có thể chạy **headless** (không cửa sổ) để train **Reinforcement Learning**. Xem thêm: [`docs/architecture_overview.md`](docs/architecture_overview.md)

---

## 🎮 Tính năng nổi bật
- Hỗ trợ 1–4 người chơi cùng lúc trên 1 máy
- Tùy chỉnh phím điều khiển cho từng người chơi
- Vật lý thực tế: đạn nảy dội tường, va chạm xe tăng
- **Cổng dịch chuyển (Portal)** ngẫu nhiên xuất hiện trên bản đồ (có thể bật/tắt)
- **Hệ thống vũ khí**: Shotgun, Frag Mine, Homing Missile, Death Ray (có thể bật/tắt)
- **Khiên bảo vệ**: chặn 1 phát đạn, cooldown 15 giây
- Bản đồ mê cung sinh ngẫu nhiên (Recursive Backtracker + shortcut)
- Giao diện Settings với nút bánh răng, font Outfit Medium

---

## ⚙️ Yêu cầu hệ thống
- **Trình biên dịch C++**: GCC (MinGW) hoặc MSVC
- **CMake**: ≥ 3.10
- **Raylib**: cài qua MSYS2 hoặc vcpkg
- **Box2D**: đã tích hợp sẵn trong thư mục `box2d/`, tự động build qua CMake

---

## 🚀 Hướng dẫn Cài đặt & Biên dịch

### 🟢 Cách 1: MSYS2 (Khuyên dùng cho Windows)

**Bước 1: Cài đặt MSYS2**
1. Tải từ [msys2.org](https://www.msys2.org/) và cài đặt (mặc định `C:\msys64`).
2. Mở **MSYS2 MinGW 64-bit** từ Start Menu.

**Bước 2: Cài đặt Toolchain & Raylib**
```bash
# Cập nhật hệ thống
pacman -Syu

# Cài GCC, CMake, Make
pacman -S mingw-w64-x86_64-toolchain mingw-w64-x86_64-cmake

# Cài Raylib
pacman -S mingw-w64-x86_64-raylib
```

**Bước 3: Biên dịch**
```bash
cd /d/path/to/AZgame
mkdir Build && cd Build
cmake -G "MinGW Makefiles" ..
mingw32-make
```

**Bước 4: Chạy Game**
```bash
./AZgame.exe
```

### 🔵 Cách 2: vcpkg + Visual Studio / MinGW
```bash
vcpkg install raylib
mkdir Build && cd Build
cmake -DCMAKE_TOOLCHAIN_FILE=[path_to_vcpkg]/scripts/buildsystems/vcpkg.cmake ..
cmake --build .
```

---

## 🕹️ Hướng dẫn chơi

Click vào **biểu tượng bánh răng** ⚙️ (góc trên phải) để mở Settings:
- Chỉnh số lượng người chơi (1–4)
- Bật/tắt cổng dịch chuyển và vật phẩm
- Cài đặt lại phím cho từng người chơi

### Phím mặc định

| | Tiến | Lùi | Trái | Phải | Bắn | Khiên |
|---|:---:|:---:|:---:|:---:|:---:|:---:|
| **Player 1** 🟢 | `W` | `S` | `A` | `D` | `Q` | `E` |
| **Player 2** 🔵 | `↑` | `↓` | `←` | `→` | `/` | `.` |
| **Player 3** 🔴 | `I` | `K` | `J` | `L` | `U` | `O` |
| **Player 4** 🟡 | `Num8` | `Num5` | `Num4` | `Num6` | `Num7` | `Num9` |

> ⚠️ Đạn nảy dội tường — cẩn thận đừng trúng đạn của chính mình!

---

## 🛠️ Cấu trúc mã nguồn

### Lớp Logic (Không phụ thuộc Raylib — dùng được cho RL)
| File | Vai trò |
|---|---|
| `Constants.h` | Hằng số, `PlayerConfig`, `TankActions` (input trừu tượng) |
| `tank.h/.cpp` | Xe tăng: di chuyển, bắn, va chạm, khiên. Nhận `TankActions` |
| `bullet.h/.cpp` | Đạn: bay, nảy, tên lửa đuổi (homing), frag nổ chùm |
| `map.h/.cpp` | Sinh mê cung ngẫu nhiên (Recursive Backtracker) |
| `portal.h/.cpp` | Cổng dịch chuyển A↔B |
| `item.h/.cpp` | Hộp vũ khí (Box2D sensor) |
| `game.h/.cpp` | **Engine chính**: `Update(actions, dt)`, `ResetMatch()` |

### Lớp Đồ Họa (Phụ thuộc Raylib)
| File | Vai trò |
|---|---|
| `renderer.h/.cpp` | Vẽ toàn bộ game objects (tank, bullet, map, portal, item) |
| `ui.h/.cpp` | Settings UI, HUD (bảng điểm, nút bánh răng), font tùy chỉnh |
| `main.cpp` | Game loop: keyboard → `TankActions` → `Game::Update()` → `Renderer` |

### Dùng cho RL Training
```cpp
// train.cpp — chỉ cần include "game.h", KHÔNG cần Raylib
#include "game.h"
int main() {
    Game game;
    game.ResetMatch();
    while (!game.needsRestart) {
        std::vector<TankActions> actions(2);
        actions[0].forward = true;  // agent output
        game.Update(actions, 1.0f / 60.0f);
    }
}
```

---

## ⚠️ Lỗi thường gặp

1. **`Could not find a package configuration file provided by "raylib"`**
   - Đảm bảo đang dùng terminal **MSYS2 MinGW 64-bit** (không phải PowerShell mặc định)
   - Kiểm tra: `which cmake` → phải là `/mingw64/bin/cmake`

2. **`gcc: command not found` hoặc `mingw32-make: command not found`**
   - Chạy lại: `pacman -S mingw-w64-x86_64-toolchain`