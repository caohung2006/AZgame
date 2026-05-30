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

### Output

Trong quá trình train, thư mục `agents/` sẽ chứa:
- `PhaseX_Name_genY.bin` — Checkpoint mỗi 10 generation
- `PhaseX_Name_final.bin` — Genome tốt nhất khi kết thúc Phase
- `PhaseX_Name_log.csv` — Log `gen, best, avg, species` để vẽ biểu đồ

---

## 🧬 Thuật toán NEAT — Giải thích chi tiết

### NEAT là gì?

**NEAT** (NeuroEvolution of Augmenting Topologies) là thuật toán tiến hóa mạng nơ-ron nhân tạo, được đề xuất bởi Kenneth O. Stanley & Risto Miikkulainen (2002). Khác với Deep RL truyền thống (PPO, DQN) cần đạo hàm ngược (backpropagation), NEAT **tiến hóa cả cấu trúc lẫn trọng số** của mạng nơ-ron thông qua chọn lọc tự nhiên.

**Tại sao dùng NEAT thay vì Deep RL?**
- Không cần GPU — chạy hoàn toàn trên CPU
- Không cần hyperparameter phức tạp (learning rate, batch size, replay buffer...)
- Tự động tìm kiến trúc mạng tối ưu (không cần chọn số layer, số neuron)
- Dễ song song hóa (mỗi cá thể chạy độc lập → OpenMP)
- Phù hợp với action space nhỏ (6 hành động boolean)

### Cấu trúc Genome (Bộ gen)

Mỗi cá thể AI được mã hóa bằng một **Genome** gồm 2 loại gene:

```
Genome
├── NodeGene[]     # Danh sách nơ-ron
│   ├── id         # Số định danh duy nhất
│   ├── type       # INPUT | HIDDEN | OUTPUT
│   └── bias       # Hệ số lệch
│
└── ConnGene[]     # Danh sách liên kết (synapse)
    ├── inNode     # Nơ-ron nguồn
    ├── outNode    # Nơ-ron đích
    ├── weight     # Trọng số liên kết [-5.0, +5.0]
    ├── enabled    # Bật/tắt liên kết
    └── innovation # Số đánh dấu lịch sử đột biến (Innovation Number)
```

**Genome khởi tạo:** Fully-connected từ 36 input → 6 output (tổng 216 liên kết, chưa có hidden node). Qua quá trình tiến hóa, mạng sẽ tự mọc thêm nơ-ron ẩn và liên kết mới.

### Innovation Number — Chìa khóa của NEAT

Mỗi khi đột biến cấu trúc (thêm liên kết/nơ-ron) xảy ra, một **Innovation Number** duy nhất toàn cục được gán. Cơ chế này giải quyết bài toán **Competing Conventions** — cho phép NEAT so sánh và lai ghép 2 genome có cấu trúc khác nhau bằng cách đối chiếu innovation number thay vì vị trí gene.

```
Innovation Tracker (Singleton toàn cục)
├── Lưu cache: (fromNode, toNode) → innovationNumber
├── Nếu đột biến trùng cấu trúc → trả về cùng innovation number
└── Nếu đột biến mới → cấp innovation number mới (++counter)
```

### 4 phép đột biến (Mutation)

| Phép đột biến | Xác suất | Mô tả |
|---|:---:|---|
| **Mutate Weights** | 80% | Với mỗi liên kết: 90% perturb ±N(0, 0.35), 10% random lại hoàn toàn |
| **Add Connection** | 5% | Thêm 1 liên kết mới giữa 2 nơ-ron chưa kết nối (kiểm tra không tạo vòng lặp) |
| **Add Node** | 2% | Chèn 1 nơ-ron ẩn vào giữa liên kết có sẵn: `A→B` thành `A→(new)→B` |
| **Toggle Connection** | 3% | Bật/tắt ngẫu nhiên 1 liên kết |

### Lai ghép (Crossover)

Khi 2 bố mẹ lai ghép, NEAT dùng innovation number để căn chỉnh gene:

```
Parent 1 (fitter):  [1] [2] [3] [4]     [6] [7]
Parent 2:           [1] [2]     [4] [5] [6]
                    ─── ─── ─── ─── ─── ─── ───
Child inherits:     [1] [2] [3] [4]     [6] [7]
                     ↑   ↑       ↑       ↑   ↑
                   match match  p1only  match p1only
```

- **Matching genes** (cùng innovation): chọn ngẫu nhiên từ bố hoặc mẹ
- **Disjoint/Excess genes** (chỉ có ở 1 bên): luôn lấy từ bố mẹ **khỏe hơn**
- **Kiểm tra vòng lặp**: Nếu gene kế thừa tạo cycle → tự động disable

### Phân loài (Speciation)

NEAT phân quần thể thành các **loài** (species) dựa trên khoảng cách tương thích:

```
δ(g1, g2) = c1 × E/N + c2 × D/N + c3 × W̄
```

| Ký hiệu | Ý nghĩa | Giá trị |
|---|---|---|
| `E` | Số excess genes (thừa ở đuôi) | — |
| `D` | Số disjoint genes (thừa ở giữa) | — |
| `W̄` | Trung bình chênh lệch trọng số matching genes | — |
| `N` | Cố định = 1.0 (vì mạng lớn 216+ conns) | — |
| `c1, c2` | Hệ số excess/disjoint | 1.0 |
| `c3` | Hệ số trọng số | 0.4 |
| **Ngưỡng** | Nếu `δ < threshold` → cùng loài | 2.0 (tự điều chỉnh) |

**Mục đích phân loài**: Bảo vệ đổi mới cấu trúc khỏi bị loại bỏ quá sớm. Khi 1 nơ-ron mới vừa mọc, fitness ban đầu thường tệ hơn → nếu không có loài bảo vệ, nó sẽ bị tiêu diệt trước khi kịp tối ưu trọng số.

**Ngưỡng tự điều chỉnh**: Hệ thống nhắm giữ 10–15 loài. Nếu quá ít loài → giảm ngưỡng (chia nhỏ hơn); quá nhiều → tăng ngưỡng (gộp lại).

### Vòng lặp tiến hóa (Evolve)

Mỗi thế hệ thực hiện các bước:

```
1. SPECIATE     → Phân loại 300 cá thể vào các loài
2. EVALUATE     → Mỗi cá thể chơi 10 ván (parallel OpenMP) → tính fitness
3. ADJUST       → Fitness chia cho kích thước loài (fitness sharing)
4. STAGNATION   → Loài không cải thiện sau 30 thế hệ → bị xóa (trừ loài chứa cá thể tốt nhất)
5. OFFSPRING    → Mỗi loài sinh con theo tỷ lệ fitness đóng góp
   ├── 50% Crossover: Lai ghép 2 bố mẹ (tournament k=3)
   └── 50% Clone:     Nhân bản + đột biến
6. ELITISM      → Top 2 cá thể toàn cục được bảo toàn nguyên vẹn
7. REPEAT       → Quay lại bước 1
```

**Tournament Selection (k=3):** Chọn 3 cá thể ngẫu nhiên trong loài, lấy cá thể khỏe nhất làm bố/mẹ. Cân bằng giữa exploitation (chọn tốt nhất) và exploration (cho cá thể yếu cơ hội).

### Siêu tham số NEAT

| Tham số | Giá trị | Mô tả |
|---|:---:|---|
| `POP_SIZE` | 300 | Kích thước quần thể |
| `WEIGHT_MUTATE_RATE` | 80% | Tỷ lệ đột biến trọng số |
| `WEIGHT_PERTURB_STR` | 0.35 | Độ mạnh nhiễu Gaussian |
| `ADD_CONN_RATE` | 5% | Tỷ lệ thêm liên kết |
| `ADD_NODE_RATE` | 2% | Tỷ lệ thêm nơ-ron |
| `DISABLE_RATE` | 3% | Tỷ lệ tắt liên kết |
| `CROSSOVER_RATE` | 50% | Tỷ lệ lai ghép (vs clone + mutate) |
| `STAGNATION_LIMIT` | 30 | Số thế hệ trì trệ tối đa trước khi xóa loài |
| `COMPAT_THRESHOLD` | 2.0 | Ngưỡng phân loài (tự điều chỉnh) |

---

## 📊 Quy trình huấn luyện (Training Pipeline)

### Tổng quan luồng xử lý

```
                    ┌─────────────────────────────────────────────────┐
                    │           AZtrain.exe (Headless)                │
                    │                                                 │
                    │  Population (300 Genomes)                       │
                    │       │                                         │
                    │       ▼                                         │
                    │  ┌──────────┐    ┌──────────────────────┐       │
                    │  │ Phase 1  │───►│ Phase 2              │──►... │
                    │  │ Bắn bia  │    │ Dẫn đường mê cung    │       │
                    │  └──────────┘    └──────────────────────┘       │
                    │                                                 │
                    │  Mỗi Phase:                                     │
                    │  ┌─────────────────────────────────────┐        │
                    │  │ For gen = 1 to maxGenerations:      │        │
                    │  │   1. Sinh seed ngẫu nhiên           │        │
                    │  │   2. EvaluateAll (OpenMP parallel)   │        │
                    │  │   3. Evolve (speciate + reproduce)   │        │
                    │  │   4. Check promotion threshold       │        │
                    │  │   5. Checkpoint mỗi 10 gen           │        │
                    │  └─────────────────────────────────────┘        │
                    │                                                 │
                    │  Thăng hạng khi: best ≥ threshold               │
                    │  trong N thế hệ liên tiếp (streak)              │
                    └─────────────────────────────────────────────────┘
```

### Curriculum Learning — 5 giai đoạn

AI được dạy theo lộ trình từ dễ đến khó. Mỗi Phase thay đổi đối thủ, bản đồ, và luật chơi:

#### Phase 1: Basic (Bắn bia)

| Thuộc tính | Giá trị |
|---|---|
| **Đối thủ** | Bia đứng yên (Stationary) |
| **Bản đồ** | Sparse (ít tường, gần như trống) |
| **Khiên / Item / Portal** | Tắt / Tắt / Tắt |
| **Đạn** | Tồn tại 2.5s, tối đa 3 viên |
| **Max steps** | 1200 (= 20 giây ở 60 FPS) |
| **Seeds** | 10 ván/cá thể |
| **Ngưỡng thăng hạng** | 500 điểm (streak 10 gen) |
| **Generations tối đa** | 500 |
| **Mục tiêu học** | Di chuyển cơ bản + ngắm bắn mục tiêu tĩnh |

#### Phase 2: Wanderer (Lách mê cung)

| Thuộc tính | Giá trị |
|---|---|
| **Đối thủ** | Rule V1 — Wanderer (đi lang thang, KHÔNG bắn) |
| **Bản đồ** | Normal (mê cung đầy đủ) |
| **Khiên / Item / Portal** | Tắt / Tắt / Tắt |
| **Đạn** | Tồn tại 2.5s, tối đa 3 viên |
| **Max steps** | 1500 (= 25 giây) |
| **Ngưỡng thăng hạng** | 480 điểm (streak 10 gen) |
| **League mixing** | 30% trộn STATIONARY (ôn bài Phase 1) |
| **Mục tiêu học** | Dẫn đường A* trong mê cung + đuổi theo mục tiêu di chuyển |

#### Phase 3: Fighter (Chiến đấu)

| Thuộc tính | Giá trị |
|---|---|
| **Đối thủ** | Rule V2 — Fighter (đuổi theo + bắn nghiệp dư, ngắm lệch ~0.3 rad) |
| **Bản đồ** | Normal (mê cung) |
| **Khiên** | **BẬT** (AI phải học dùng khiên) |
| **Đạn** | Tồn tại 3.5s, tối đa 3 viên |
| **Ngưỡng thăng hạng** | 480 điểm (streak 10 gen) |
| **League mixing** | 25% trộn Rule V1 (ôn mê cung) |
| **Mục tiêu học** | Né đạn + bắn trả + dùng khiên đúng lúc |

#### Phase 4: Sniper Boss (Xạ thủ)

| Thuộc tính | Giá trị |
|---|---|
| **Đối thủ** | Rule V3 — Sniper Boss (ngắm chuẩn ~0.1 rad, dừng xe bắn, có khiên phản xạ) |
| **Bản đồ** | Normal (mê cung) |
| **Đạn** | Tồn tại 7s, tối đa 5 viên |
| **Ngưỡng thăng hạng** | 400 điểm (streak 10 gen) |
| **League mixing** | 35% trộn Rule V2 (ôn chiến đấu) |
| **Mục tiêu học** | Kỹ năng chiến đấu nâng cao chống lại đối thủ mạnh |

#### Phase 5: Tournament (Tự đấu)

| Thuộc tính | Giá trị |
|---|---|
| **Đối thủ** | Self-play (bản sao tốt nhất thế hệ trước) |
| **Bản đồ** | Normal (mê cung) |
| **Đạn** | Tồn tại 7s, tối đa 5 viên |
| **Ngưỡng thăng hạng** | 300 điểm (streak 10 gen) |
| **League mixing** | 50% trộn Rule V3 (giữ kỷ luật xạ thủ) |
| **Mục tiêu học** | Tối ưu chiến thuật đỉnh cao, tìm meta-strategy |

### Cơ chế thăng hạng (Promotion)

Một Phase được coi là "tốt nghiệp" khi:
1. AI đạt fitness **≥ ngưỡng** (promotionThreshold)
2. Duy trì được trong **N thế hệ liên tiếp** (streakRequired = 10)
3. Đã train qua ít nhất **5% tổng số generation** tối đa

Nếu hết generation mà không đạt streak → training **dừng lại** và lưu genome tốt nhất. Người dùng có thể:
- Chạy lại từ checkpoint: `./AZtrain.exe 4 agents/PhaseX_Name_final.bin`
- Điều chỉnh `Curriculum.h` rồi train lại

### League Training (Trộn đối thủ)

Để tránh AI bị **catastrophic forgetting** (quên kỹ năng cũ khi học kỹ năng mới), mỗi Phase trộn một tỷ lệ đối thủ cũ:

```
Phase 2: 30% trộn Stationary (đứng yên)     → Ôn bắn bia
Phase 3: 25% trộn Rule V1 (lang thang)       → Ôn lách mê cung
Phase 4: 35% trộn Rule V2 (bắn nghiệp dư)   → Ôn chiến đấu cơ bản
Phase 5: 50% trộn Rule V3 (xạ thủ)           → Giữ kỷ luật xạ thủ
```

---

## 🔬 Mạng nơ-ron AI — Chi tiết kỹ thuật

### Observation (36 inputs)

Mạng nơ-ron nhận **36 tín hiệu đầu vào** mỗi frame (1/60 giây):

| Index | Tên | Mô tả | Phạm vi |
|:---:|---|---|:---:|
| **0–1** | Agent Position | Tọa độ x, y chuẩn hóa | [0, 1] |
| **2–3** | Agent Heading | cos(θ), sin(θ) của góc xe | [-1, 1] |
| **4–5** | Waypoint Direction | Hướng tới A* waypoint (local frame) | [-1, 1] |
| **6** | Waypoint Distance | Khoảng cách tới waypoint / 5m | [0, 1] |
| **7–8** | Enemy Direction | Hướng tới địch (local frame) | [-1, 1] |
| **9** | Enemy Distance | Khoảng cách tới địch / max | [0, 1] |
| **10** | Line of Sight | Có nhìn thấy địch không (Box2D Raycast) | {0, 1} |
| **11–12** | Enemy Velocity | Vận tốc địch (local frame) | [-1, 1] |
| **13–16** | Bullet #1 | Vị trí + vận tốc viên đạn gần nhất (local frame) | [-1, 1] |
| **17–20** | Bullet #2 | Vị trí + vận tốc viên đạn gần thứ 2 | [-1, 1] |
| **21** | Ammo Level | 1 − (đạn đang bay / 5) | [0, 1] |
| **22** | Shield Active | Khiên đang bật? | {0, 1} |
| **23** | Shield Cooldown | 1 − (thời gian chờ / 15s) | [0, 1] |
| **24–30** | Radar (7 tia) | Khoảng cách tới vật cản: 0°, ±30°, ±90°, ±150° | [0, 1] |
| **31** | Angular Velocity | Tốc độ xoay / 5.0 | [-1, 1] |
| **32** | Enemy Ammo | 1 − (đạn địch đang bay / 5) | [0, 1] |
| **33–34** | Agent Velocity | Vận tốc tịnh tiến (local frame) | [-1, 1] |
| **35** | Shoot Cooldown | 1 − (thời gian chờ bắn / 0.5s) | [0, 1] |

**Bullet Danger System**: 2 viên đạn nguy hiểm nhất được ưu tiên hiển thị. Tiêu chí:
1. Đạn đang bay **về phía** agent (dot product < 0) được ưu tiên
2. Trong cùng nhóm, đạn **gần hơn** được ưu tiên
3. Chỉ xét đạn trong bán kính 8m
4. Không lọc đạn của chính mình (vì đạn nảy tường có thể tự sát)

**A* Waypoint**: Được tính lại mỗi 3 frame bằng A* trên grid 6×8 ô. Kỹ thuật **Pure Pursuit** (cà rốt trên gậy) giúp xe di chuyển mượt, không giật:
- Chiếu vị trí xe lên đoạn thẳng đường đi
- Đặt điểm "cà rốt" trước xe 3m trên trục
- Anti-corner cutting: Nếu xe lệch trục > 1m → bắt phải đi tới tâm ngã rẽ trước

### Action Space (6 outputs)

| Output | Activation | Hành động |
|:---:|:---:|---|
| **0** | sigmoid > 0.5 **VÀ** > output[1] | Tiến |
| **1** | sigmoid > 0.5 **VÀ** > output[0] | Lùi |
| **2** | sigmoid > 0.5 **VÀ** > output[3] | Quay trái |
| **3** | sigmoid > 0.5 **VÀ** > output[2] | Quay phải |
| **4** | sigmoid > 0.5 | Bắn |
| **5** | sigmoid > 0.5 | Khiên |

**Exclusive actions**: Forward/Backward và TurnLeft/TurnRight là exclusive — chỉ hành động có giá trị cao hơn được thực thi. Điều này tránh AI bị "đơ" khi 2 output đều > 0.5.

### Decode Genome → Network

Genome được giải mã thành mạng feed-forward bằng **Kahn's Topological Sort**:

```
1. Xây đồ thị có hướng từ enabled ConnGenes
2. Tính in-degree mỗi node
3. Kahn's Algorithm: lặp lấy node có in-degree = 0
4. Forward pass theo thứ tự topo:
   - INPUT:  identity (giá trị từ observation)
   - HIDDEN: tanh(Σ(input × weight) + bias)
   - OUTPUT: sigmoid(Σ(input × weight) + bias)
```

---

## 💰 Hàm Fitness — Cách AI được chấm điểm

### Phase 1–3: Dense Reward (Cầm tay chỉ việc)

Fitness = tổng step reward mỗi frame + end-of-episode bonus.

#### Step Reward (tính mỗi frame)

Hệ thống 2 chế độ:

**NAVIGATION MODE** (chưa thấy địch hoặc xa > 8m):
- Thưởng di chuyển về hướng A* waypoint: `(fwdSpeed/maxSpeed) × dotWaypoint × 8.0 × dt`
- Phạt đứng im khi chưa thấy địch: -2.0 × dt
- Phạt xoay tít mù (beyblade): -5.0 × dt

**ENGAGEMENT MODE** (thấy địch ≤ 8m):
- **Ngắm bắn** — Tín hiệu chính:
  - Sweet spot (3–8m): `dotAim × 10.0 × dt` (thưởng mạnh nhất)
  - Gần (< 3m): `dotAim × 4.0 × dt`
  - Xa (> 8m): `dotAim × 3.0 × dt`
- **Quản lý khoảng cách** — "giếng hấp dẫn" ở 3–8m:
  - Xa > 8m: thưởng tiến gần (tối đa ~5 pts/s)
  - Sweet spot 3–8m: thưởng liên tục +3.0 × dt
  - Gần 1.8–3m: phạt nhẹ -3.0 × dt
  - Quá gần < 1.8m: phạt kamikaze -5.0 × dt

**Kỷ luật (mọi lúc):**
- Phạt húc tường: -6.0 × dt (nếu đang tiến)
- Thưởng lùi khi sát tường: +1.0 × dt
- Anti-retreat (Phase 3+): Phạt đi lùi khi thấy địch ở sweet spot

**Xạ thủ:**
- Bắn chuẩn (dotAim > 0.90, thấy địch, < 10m): +15.0 × dt (Phase 1–2)
- Bắn bừa / không thấy địch: phạt -4.0 × dt

**Khiên (Phase 3):**
- Bật khiên đúng lúc (có đạn bay tới): **+10.0 điểm** (one-shot, không nhân dt)
- Bật khiên lãng phí: **-5.0 điểm**
- Đang che đạn: +3.0 × dt
- Né đạn bằng di chuyển: +2.0 × dt

#### End-of-Episode Bonus

| Kết quả | Điểm | Ghi chú |
|---|:---:|---|
| **Thắng** | +400 + timeBonus + 50 (nếu có bắn) | Thắng nhanh = thưởng cao hơn |
| **Hòa** (hết giờ, có chiến đấu) | -300 | Ép phải giết địch |
| **Hòa** (P3, không bắn phát nào) | -500 | Rùa rụt cổ bị phạt nặng nhất |
| **Chết** | -400 | Tệ nhưng vẫn tốt hơn rùa rụt cổ |

**Thang điểm ưu tiên**: Thắng (+400) > Hòa có bắn (-300) > Chết (-400) > Rùa rụt cổ (-500)

### Phase 4–5: Sparse Reward (Luật rừng)

Step reward chỉ giữ:
- Phạt đứng im khi chưa thấy địch: -2.0 × dt
- Phạt beyblade: -3.0 × dt
- Thưởng nhẹ đi hướng waypoint: +1.0 × dt
- Phạt húc tường: tương tự Phase 1–3

End-of-episode bonus mạnh hơn nhiều:
- **Thắng**: +500 + timeBonus × 500
- **Thua/Hòa**: -500

---

## 🤖 Bot đối thủ (Rule-based Enemy)

### V1 — Wanderer (Phase 2)

- **Hành vi**: Tuần tra ngẫu nhiên trên bản đồ bằng A* pathfinding
- **Bắn**: KHÔNG BAO GIỜ bắn (để AI Phase 2 tập trung học dẫn đường)
- **Di chuyển**: Hash vị trí hiện tại → chọn ô mục tiêu → A* → lái theo waypoint
- **Mục đích**: Dạy AI đuổi theo mục tiêu di chuyển trong mê cung

### V2 — Fighter (Phase 3)

- **Hành vi**: Đuổi theo Agent bằng A* + bắn khi thấy
- **Ngắm**: Lệch ~0.3 rad (nghiệp dư)
- **Tầm bắn**: < 7m + phải thấy trực tiếp (LOS)
- **Giới hạn đạn**: Tối đa 2 viên trên sân
- **Mục đích**: Dạy AI né đạn + bắn trả + dùng khiên

### V3 — Sniper Boss (Phase 4)

- **Hành vi**: Đuổi theo Agent + dừng xe ngắm bắn khi thấy
- **Ngắm**: Lệch ±0.2 rad (rất chuẩn), chỉ bắn khi < 0.1 rad
- **Tốc độ quay**: Chỉ quay 50% frame (nerf để AI có thời gian phản ứng)
- **Radar**: 3 tia phía trước để thoát kẹt tường
- **Khiên**: Phản xạ khi đạn bay tới < 2.5m
- **Giới hạn đạn**: Tối đa 2 viên trên sân
- **Mục đích**: Ép AI phải phát triển kỹ năng chiến đấu nâng cao

---

## 🛠️ Cấu trúc mã nguồn

```
AZgame/
├── include/            # Header files (game.h, tank.h, bullet.h, ...)
│   ├── Constants.h     # Hằng số vật lý, cấu trúc TankActions, DeathEvent
│   ├── AZRandom.h      # Thread-local RNG (an toàn với OpenMP)
│   ├── game.h          # Class Game — quản lý toàn bộ trạng thái
│   ├── tank.h          # Class Tank — xe tăng (di chuyển, bắn, khiên)
│   ├── bullet.h        # Class Bullet — đạn (bay, đuổi, frag)
│   ├── map.h           # Class GameMap — mê cung + A* pathfinding
│   ├── portal.h        # Class Portal — cổng dịch chuyển A↔B
│   ├── item.h          # Class Item — hộp vũ khí (Shotgun, Frag, Missile, Laser)
│   ├── renderer.h      # Class Renderer — vẽ đồ họa + particle
│   └── ui.h            # Class UI — giao diện cài đặt + HUD
│
├── neat/               # Thuật toán NEAT (Neuroevolution)
│   ├── Genome.h        # Bộ gen: NodeGene + ConnGene, Mutate, Crossover, Save/Load
│   ├── Network.h       # Giải mã Genome → mạng feed-forward (Kahn topo sort)
│   ├── Population.h    # Quần thể 300 cá thể: Speciate, Evolve, EvaluateAll (OpenMP)
│   ├── Species.h       # Loài: fitness sharing, stagnation tracking
│   └── Innovation.h    # Global Innovation Number Tracker (singleton)
│
├── train/              # Training pipeline
│   ├── train.cpp       # Vòng lặp train chính (main cho AZtrain.exe)
│   ├── Curriculum.h    # Cấu hình 5 Phase (threshold, streak, map, enemy)
│   ├── Fitness.h       # Hàm tính điểm (step reward + end bonus)
│   ├── Observation.h   # 36 inputs cho neural network (radar, LOS, bullet danger)
│   └── RuleEnemy.h     # Bot đối thủ (V1 Wanderer, V2 Fighter, V3 Sniper)
│
├── agents/             # Bộ não AI đã train (.bin) + training logs (.csv)
├── box2d/              # Thư viện vật lý (tích hợp sẵn, tự động build)
├── fonts/              # Font chữ game
├── images/             # Ảnh tài nguyên
│
├── main.cpp            # Game loop + Watch Mode (phụ thuộc Raylib)
├── game.cpp            # Engine logic chính (KHÔNG phụ thuộc Raylib)
├── map.cpp             # Sinh mê cung Recursive Backtracker + A* + Pure Pursuit
├── tank.cpp            # Logic xe tăng: di chuyển, bắn, va chạm
├── bullet.cpp          # Logic đạn: bay, đuổi, hết hạn
├── portal.cpp          # Logic cổng dịch chuyển
├── item.cpp            # Logic hộp vũ khí
├── renderer.cpp        # Vẽ đồ họa (Raylib) + particle effect
├── ui.cpp              # Giao diện Settings + HUD
├── CMakeLists.txt      # Cấu hình build (2 target: AZgame + AZtrain)
└── README.md           # File này
```

### Kiến trúc 2 lớp

| Lớp | File | Phụ thuộc | Mục đích |
|---|---|---|---|
| **Logic** (headless) | `game.cpp`, `tank.cpp`, `bullet.cpp`, `map.cpp`, `portal.cpp`, `item.cpp` | Box2D | Engine thuần túy, chạy được không cần màn hình |
| **Đồ họa** | `renderer.cpp`, `ui.cpp`, `main.cpp` | Raylib + Box2D | Chỉ dùng cho chơi tay và Watch Mode |
| **Training** | `train/train.cpp` + headers | Box2D + OpenMP | Headless, song song, không cần Raylib |

> `AZtrain.exe` **không cần Raylib** — chỉ dùng Box2D + OpenMP, chạy headless hoàn toàn.

### AI-Ready Architecture

Chìa khóa thiết kế: struct `TankActions` là interface duy nhất để điều khiển xe tăng.

```cpp
struct TankActions {
    bool forward, backward;     // Di chuyển
    bool turnLeft, turnRight;   // Xoay
    bool shoot, shield;         // Chiến đấu
};

// Người chơi: bàn phím → TankActions
// AI NEAT:    neural network output → TankActions
// Rule Bot:   heuristic → TankActions
// Tất cả đều đi qua cùng 1 hàm: Game::Update(actions, dt)
```

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