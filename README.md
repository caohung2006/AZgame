# AZ Tank Game

AZ Tank là một tựa game bắn xe tăng 2D cổ điển hỗ trợ **1–4 người chơi** trên cùng 1 máy (Local Multiplayer), tích hợp hệ thống **AI tự học** (Neuroevolution — NEAT) với Curriculum Learning 5 giai đoạn.

Game được phát triển bằng **C++**, sử dụng **Raylib** (đồ họa/nhập liệu) và **Box2D** (vật lý). Game logic **tách hoàn toàn** khỏi đồ họa — có thể chạy **headless** (không cửa sổ) để train AI.

---

## 🎮 Tính năng nổi bật

- Hỗ trợ 1–4 người chơi cùng lúc trên 1 máy
- Vật lý thực tế: đạn nảy dội tường (`restitution = 1.0`), va chạm xe tăng
- **Cổng dịch chuyển (Portal)** ngẫu nhiên trên bản đồ (bật/tắt)
- **Hệ thống vũ khí**: Shotgun, Frag Mine, Homing Missile, Death Ray (bật/tắt)
- **Khiên bảo vệ**: chặn 1 phát đạn, cooldown 15 giây
- Bản đồ mê cung sinh ngẫu nhiên (Recursive Backtracker + shortcut)
- **NEAT AI** với Curriculum Learning 5 Phase (từ bắn bia → tự đấu)
- **Watch Mode**: Xem AI đã train đánh nhau trực tiếp

---

## ⚙️ Yêu cầu hệ thống

| Yêu cầu | Chi tiết |
|---|---|
| **OS** | Windows 10/11 |
| **Compiler** | GCC (MinGW) thông qua MSYS2 |
| **CMake** | ≥ 3.10 |
| **Raylib** | Cài qua MSYS2 (`pacman`) |
| **Box2D** | Đã tích hợp sẵn trong `box2d/`, tự động build |

---

## 🚀 Cài đặt & Biên dịch

### Bước 1: Cài đặt MSYS2

1. Tải từ [msys2.org](https://www.msys2.org/) và cài đặt (mặc định `C:\msys64`).
2. Mở **MSYS2 MinGW 64-bit** từ Start Menu.

### Bước 2: Cài đặt Toolchain & Raylib

```bash
# Cập nhật hệ thống
pacman -Syu

# Cài GCC, CMake, Make
pacman -S mingw-w64-x86_64-toolchain mingw-w64-x86_64-cmake

# Cài Raylib
pacman -S mingw-w64-x86_64-raylib
```

### Bước 3: Clone & Build

```bash
git clone https://github.com/<your-org>/AZgame.git
cd AZgame

# Tạo thư mục build
mkdir build && cd build

# Cấu hình CMake
cmake -G "MinGW Makefiles" ..

# Biên dịch (tạo ra 2 file: AZgame.exe và AZtrain.exe)
mingw32-make
```

> **Lưu ý:** Sau khi build xong, bạn sẽ có 2 file thực thi trong thư mục `build/`:
> - `AZgame.exe` — Game có giao diện đồ họa (chơi tay hoặc xem AI)
> - `AZtrain.exe` — Training headless (không cửa sổ, chạy nền)

---

## 🕹️ Chạy Game (Chơi tay)

```bash
cd build
./AZgame.exe
```

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

## 🤖 Xem AI đánh nhau (Watch Mode)

Thư mục `agents/` chứa các bộ não AI đã được train sẵn. Bạn có thể xem AI chiến đấu ngay mà **không cần train lại**.

### Cú pháp

```
./AZgame.exe --watch <brain.bin> [enemy_type] [phase]
```

| Tham số | Bắt buộc | Mô tả |
|---|:---:|---|
| `<brain.bin>` | ✅ | Đường dẫn tới file não AI (`.bin`) |
| `[enemy_type]` | ❌ | Loại đối thủ: `stationary`, `random`, `rule_v1`, `rule_v2`, `rule_v3`, hoặc đường dẫn tới file `.bin` khác |
| `[phase]` | ❌ | Phase môi trường (1–5), ảnh hưởng loại bản đồ và cài đặt game |

### Ví dụ

```bash
cd build

# Xem AI Phase 1 đánh với bia đứng yên (map trống)
./AZgame.exe --watch ../agents/Phase1_Basic_final.bin stationary 1

# Xem AI Phase 3 đánh với bot nghiệp dư (mê cung, có khiên)
./AZgame.exe --watch ../agents/Phase3_Fighter_final.bin rule_v2 3

# Xem AI Phase 4 đánh với Sniper Boss
./AZgame.exe --watch ../agents/Phase4_SniperBoss_gen110.bin rule_v3 4

# Xem 2 AI tự đánh nhau (Self-play)
./AZgame.exe --watch ../agents/Phase3_Fighter_final.bin ../agents/Phase1_Basic_final.bin 3
```

**Phím tắt khi xem:**
- **`V`** — Bật/tắt hiển thị Waypoint (chấm đỏ) của AI
- **`ESC`** — Thoát

---

## 🧠 Train AI (NEAT + Curriculum Learning)

### Chạy nhanh

```bash
cd build

# Train từ đầu (4 threads)
./AZtrain.exe 4

# Tiếp tục train từ checkpoint
./AZtrain.exe 4 ../agents/Phase1_Basic_final.bin
```

### Cú pháp

```
./AZtrain.exe <num_threads> [checkpoint.bin]
```

| Tham số | Mô tả |
|---|---|
| `<num_threads>` | Số luồng CPU (khuyên dùng = số core vật lý) |
| `[checkpoint.bin]` | Tùy chọn. File genome để tiếp tục train. Phase được nhận diện tự động từ tên file |

### Curriculum Learning — 5 giai đoạn

AI được dạy theo lộ trình từ dễ đến khó:

| Phase | Đối thủ | Bản đồ | Mục tiêu |
|:---:|---|---|---|
| **1** | Bia đứng yên | Trống (Sparse) | Học di chuyển + ngắm bắn cơ bản |
| **2** | Wanderer (chỉ đi, không bắn) | Mê cung | Học dẫn đường A* trong mê cung |
| **3** | Fighter (bắn nghiệp dư) | Mê cung | Học chiến đấu + né đạn + dùng khiên |
| **4** | Sniper Boss (ngắm chuẩn, có khiên) | Mê cung | Kỹ năng chiến đấu nâng cao |
| **5** | Self-play (đánh chính mình) | Mê cung | Tối ưu chiến thuật đỉnh cao |

Mỗi Phase yêu cầu AI đạt **ngưỡng điểm** trong **nhiều thế hệ liên tiếp** (streak) mới được thăng hạng. Nếu không đạt streak khi hết số generation tối đa, training sẽ **dừng lại** và lưu genome tốt nhất.

### Output

Trong quá trình train, thư mục `agents/` sẽ chứa:
- `PhaseX_Name_genY.bin` — Checkpoint mỗi 10 generation
- `PhaseX_Name_final.bin` — Genome tốt nhất khi kết thúc Phase
- `PhaseX_Name_log.csv` — Log `gen, best, avg, species` để vẽ biểu đồ

---

## 🛠️ Cấu trúc mã nguồn

```
AZgame/
├── include/            # Header files (game.h, tank.h, bullet.h, ...)
├── neat/               # Thuật toán NEAT (Genome, Network, Population, Species)
├── train/              # Training pipeline
│   ├── train.cpp       # Vòng lặp train chính
│   ├── Curriculum.h    # Cấu hình 5 Phase (threshold, streak, map, enemy)
│   ├── Fitness.h       # Hàm tính điểm (step reward + end bonus)
│   ├── Observation.h   # 36 inputs cho neural network
│   └── RuleEnemy.h     # Bot đối thủ (V1 Wanderer, V2 Fighter, V3 Sniper)
├── agents/             # Bộ não AI đã train (.bin) + training logs (.csv)
├── box2d/              # Thư viện vật lý (tích hợp sẵn)
├── fonts/              # Font chữ game
├── main.cpp            # Game loop + Watch Mode
├── game.cpp            # Engine logic chính
├── map.cpp             # Sinh mê cung + A* Pathfinding
├── renderer.cpp        # Vẽ đồ họa (Raylib)
├── CMakeLists.txt      # Cấu hình build
└── README.md           # File này
```

### Kiến trúc 2 lớp

| Lớp | File | Phụ thuộc |
|---|---|---|
| **Logic** (headless) | `game.cpp`, `tank.cpp`, `bullet.cpp`, `map.cpp`, `portal.cpp`, `item.cpp` | Box2D |
| **Đồ họa** | `renderer.cpp`, `ui.cpp`, `main.cpp` | Raylib + Box2D |
| **Training** | `train/train.cpp` + headers | Box2D + OpenMP |

> `AZtrain.exe` **không cần Raylib** — chỉ dùng Box2D + OpenMP, chạy headless hoàn toàn.

---

## ⚠️ Lỗi thường gặp

### 1. `Could not find a package configuration file provided by "raylib"`
- Đảm bảo đang dùng terminal **MSYS2 MinGW 64-bit** (không phải PowerShell mặc định)
- Kiểm tra: `which cmake` → phải là `/mingw64/bin/cmake`

### 2. `gcc: command not found` hoặc `mingw32-make: command not found`
- Chạy lại: `pacman -S mingw-w64-x86_64-toolchain`

### 3. `WARNING: FILEIO: [fonts/GameFont.ttf] Failed to open file`
- Không ảnh hưởng gameplay, font sẽ tự fallback
- Nếu muốn fix: chạy `AZgame.exe` từ thư mục gốc dự án thay vì `build/`

### 4. Train quá chậm
- Tăng số threads: `./AZtrain.exe 8`
- Kiểm tra CPU đang ở chế độ High Performance (không phải Power Saver)

---

## 📄 License

MIT License