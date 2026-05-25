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

## 📈 Hệ thống Học tăng cường (Reinforcement Learning - RL)

Dự án này tích hợp một môi trường huấn luyện xe tăng tự động chất lượng cao dựa trên phương pháp **Học tăng cường**. Bằng cách bọc game engine viết bằng C++ qua thư viện **Pybind11**, Agent Python có thể tương tác trực tiếp với môi trường vật lý với hiệu suất cực cao (>1000 FPS ở chế độ headless).

---

### 1. Thuật toán cốt lõi & Thư viện sử dụng
- **Thuật toán**: **PPO (Proximal Policy Optimization)** được lấy từ thư viện **Stable-Baselines3 (SB3)**. Đây là một thuật toán thuộc nhóm Policy Gradient ổn định, hiệu quả và phổ biến bậc nhất hiện nay.
- **Mạng nơ-ron (Policy)**: Sử dụng **MLP (Multi-Layer Perceptron) Policy** với kiến trúc mạng nơ-ron truyền thẳng. MLP Policy nhỏ gọn giúp giảm thiểu overhead truyền dữ liệu giữa RAM và VRAM, giúp việc huấn luyện trên CPU đạt tốc độ cực nhanh.
- **Thư viện bọc (Wrapper)**: 
  - [rl_env_wrapper.cpp](file:///c:/Users/Admin/Downloads/AZgame-hung%20%281%29/AZgame-hung/rl_env_wrapper.cpp): Bọc Game Engine C++ thành module Python `azgame_env` bằng Pybind11.
  - [gymnasium_wrapper.py](file:///c:/Users/Admin/Downloads/AZgame-hung%20%281%29/AZgame-hung/train_AI/gymnasium_wrapper.py): Kế thừa chuẩn `gymnasium.Env` để tương thích hoàn toàn với các thuật toán của Stable-Baselines3.

---

### 2. Không gian Quan sát (Observation Space - 45 chiều)
Trạng thái (State) trả về cho AI là một vector gồm **45 số thực** (`np.float32`), được chuẩn hóa về khoảng `[-1.0, 1.0]` hoặc `[0.0, 1.0]`:

| Tên nhóm đặc trưng | Số chiều | Chỉ số trong Vector | Chi tiết mô tả |
| :--- | :---: | :---: | :--- |
| **Self State** | 5 | `[0 -> 4]` | Góc xoay xe tăng (Cos/Sin), Vận tốc tuyến tính cục bộ (Vx, Vy) chia cho max speed 3.0, và Vận tốc góc. |
| **Enemy Info** | 8 | `[5 -> 12]` | Vị trí tương đối của kẻ địch (Local X, Local Y), Khoảng cách chuẩn hóa, Trạng thái tầm nhìn (Line of Sight - 1.0 nếu nhìn thấy, 0.0 nếu bị tường cản), Vận tốc cục bộ của kẻ địch (Vx, Vy), Góc hướng tương đối (Cos/Sin). |
| **Bullet Radar** | 8 | `[13 -> 20]` | Thông tin về 2 viên đạn nguy hiểm nhất đang bay về phía xe tăng (ưu tiên theo thời gian va chạm TTC nhỏ nhất). Gồm: Vị trí tương đối (Local X, Local Y), TTC (Time to Collision), Khoảng cách trượt gần nhất (Miss Distance). |
| **Wall Radar** | 8 | `[21 -> 28]` | Khoảng cách quét từ xe tăng đến các bức tường gần nhất ở 8 hướng xung quanh (`-135°`, `-90°`, `-45°`, `0°`, `45°`, `90°`, `135°`, `180°`). |
| **A\* Navigation** | 3 | `[29 -> 31]` | Tọa độ tương đối của Waypoint tiếp theo theo đường đi tối ưu A* (Local X, Local Y) và Khoảng cách đường đi (Path Distance). |
| **Status Info** | 5 | `[32 -> 36]` | Lượng đạn hiện tại, Thời gian hồi chiêu bắn, Lượng đạn của đối thủ, Trạng thái khiên (Active), Thời gian hồi chiêu khiên. |
| **Prev Actions** | 8 | `[37 -> 44]` | Mã hóa One-Hot của hành động ở bước trước đó của cả hai xe tăng (Di chuyển, Xoay, Bắn) để AI học cách duy trì quán tính hành động. |

---

### 3. Không gian Hành động (Action Space)
Sử dụng không gian hành động rời rạc nhiều nhánh **`MultiDiscrete([3, 3, 2])`**, cho phép AI nhấn đồng thời nhiều phím:
- **Nhánh di chuyển (Move)**: `0` = Đứng yên (Idle), `1` = Tiến (Forward), `2` = Lùi (Backward).
- **Nhánh xoay (Turn)**: `0` = Đứng yên, `1` = Quay trái (Turn Left), `2` = Quay phải (Turn Right).
- **Nhánh bắn (Shoot)**: `0` = Không bắn, `1` = Bắn.

---

### 4. Thiết kế Phần thưởng (Reward Shaping)
Để định hướng AI học nhanh và đúng mục tiêu, hệ thống phần thưởng được thiết kế cực kỳ chi tiết:
- **Thời gian (Time Penalty)**: `-0.015` điểm mỗi bước để thúc ép AI kết thúc trận đấu nhanh, tránh hành vi đi vòng tròn hoặc đứng im.
- **Tiếp cận mục tiêu (Progress Reward)**: Thưởng/Phạt dựa trên sự thay đổi khoảng cách đường đi (theo A* hoặc đường thẳng) đến kẻ địch: `(Khoảng cách cũ - Khoảng cách mới) * 0.1`.
- **Đâm tường & Kẹt góc (Wall & Stuck Penalty)**:
  - Phạt đâm tường: `-0.02` điểm.
  - Phạt kẹt góc (Stuck Penalty): `-0.03` điểm nếu nhấn phím di chuyển sát tường nhưng vận tốc thực tế lại xấp xỉ bằng 0.
- **Ngắm bắn & Spam đạn (Shooting Reward/Penalty)**:
  - Thưởng bắn chuẩn: Từ `+0.05` đến `+0.1` điểm nếu nổ súng khi kẻ địch đang nằm trong tầm ngắm (Line of Sight).
  - Phạt spam đạn: `-0.1` điểm nếu nổ súng bừa bãi khi không thấy kẻ địch.
- **Phân định Thắng/Thua (Combat Reward)**:
  - Tiêu diệt kẻ địch: `+100.0` điểm.
  - Bị kẻ địch tiêu diệt: `-100.0` điểm.
  - Tự sát (bị đạn nảy của chính mình bắn trúng): `-250.0` điểm (Phạt cực nặng để AI học cách né đạn nảy).
  - Đâm/ôm kẻ địch (Ramming Penalty): `-0.02` điểm (tránh AI chọn giải pháp lao vào tự sát cùng địch).
- **Phần thưởng Điều hướng (A\* Navigation & Facing)**:
  - Thưởng di chuyển bám đường: `+0.005` điểm nếu hướng mặt và di chuyển đúng theo Waypoint của thuật toán A*.
  - Thưởng hướng mặt về địch (Facing Reward): `+0.004` điểm nếu luôn hướng nòng súng về phía đối thủ.
- **Vùng chiến đấu tối ưu (Combat Zone Shaping)** (Khi chơi trên bản đồ trống):
  - Phạt đứng quá xa (>350 pixel): `-0.02` điểm.
  - Phạt đứng quá gần (<80 pixel): `-0.05` điểm.

---

### 5. Chiến lược Huấn luyện Tăng tiến (Curriculum Learning) & Đấu tập (Self-Play)
Huấn luyện AI từ con số 0 trong môi trường phức tạp (mê cung, đạn nảy, nhặt đồ) rất dễ bị phân kỳ. Dự án giải quyết vấn đề này bằng hai kỹ thuật chính:

#### A. Lộ trình học 8 Giai đoạn (Curriculum Learning)
Độ khó của game và sức mạnh của đối thủ (Bot viết bằng luật mã hóa - Rule-based) được nâng dần qua từng giai đoạn:

1. **Phase 1: Nắm quyền kiểm soát (500k steps)**: Huấn luyện trên bản đồ trống, đối thủ là Bot Level 1 đứng im. AI chỉ học cách lái xe và ngắm bắn cơ bản.
2. **Phase 2: Rượt đuổi (800k steps)**: Bản đồ trống, Bot Level 2 biết di chuyển tự do. AI học cách Tracking (bám đuổi) mục tiêu di động.
3. **Phase 3: Né đạn cơ bản (1.0M steps)**: Bản đồ trống, Bot Level 3 bắn liên tục. AI bắt đầu học cách lách ngang (Strafe) để né đạn bay trực diện.
4. **Phase 4: Nhập môn mê cung (1.0M steps)**: Bật bản đồ mê cung có tường chắn. Để AI không bị ngợp, đối thủ được hạ xuống Bot Level 1 đứng im. AI học cách di chuyển theo A* và né đâm tường.
5. **Phase 5: Tác chiến đô thị (1.5M steps)**: Có mê cung, đối thủ là Bot Level 3/Level 2. AI học cách kết hợp giữa việc né tường và lướt lách tránh đạn.
6. **Phase 6: Chống Bắn Đón - Juking (2.0M steps)**: Bot đối thủ nâng lên Level 4 (biết bắn đón đầu). AI phải tự học cách di chuyển "nhấp nhả" (Juking) đổi hướng đột ngột để lừa hướng bắn của địch.
7. **Phase 7: Ép góc - Cornering (2.5M steps)**: Đối thủ nâng lên Bot Level 5 (biết tự né đạn). AI học cách dồn ép đối thủ vào chân tường hoặc góc hẹp để triệt tiêu đường lui trước khi dứt điểm.
8. **Phase 8: Đấu toàn diện - Full Game (3.0M steps)**: Bản đồ mê cung đầy đủ, bật tính năng nhặt vật phẩm hỗ trợ (Tăng đạn nảy, Giáp khiên bảo vệ).

#### B. Đấu tập nâng cao (Self-Play với Opponent Pool)
Để tránh tình trạng AI bị "học tủ" (overfitting) với một đối thủ cố định hoặc rơi vào vòng lặp kéo-búa-bao chiến thuật (Rock-Paper-Scissors cycle):
- Hệ thống duy trì một **Opponent Pool** chứa mô hình hiện tại, các checkpoint cũ đã lưu trong suốt quá trình train.
- Khi bắt đầu mỗi Episode mới, môi trường sẽ bốc ngẫu nhiên một đối thủ từ Pool hoặc Rule-based Bot để đối đầu với Agent. Điều này liên tục đa dạng hóa thử thách và buộc AI phải học các kỹ năng mang tính tổng quát hóa cao.

---

### 6. Hướng dẫn Chạy & Huấn luyện AI

Toàn bộ tài nguyên huấn luyện nằm trong thư mục [train_AI/](file:///c:/Users/Admin/Downloads/AZgame-hung%20%281%29/AZgame-hung/train_AI).

> [!IMPORTANT]
> Trước khi chạy script huấn luyện Python, bạn cần chắc chắn đã biên dịch thành công file thư viện liên kết C++ (file `.pyd` trên Windows hoặc `.so` trên Linux/macOS) nằm trong thư mục [build/](file:///c:/Users/Admin/Downloads/AZgame-hung%20%281%29/AZgame-hung/build) thông qua CMake.

#### A. Cài đặt các thư viện cần thiết
```bash
pip install stable-baselines3 gymnasium numpy torch tensorboard
```

#### B. Chạy huấn luyện tự động (Toàn bộ 8 Giai đoạn)
Mặc định huấn luyện Headless (không đồ họa) giúp tối đa hóa tốc độ FPS.
```bash
python train_AI/train_ai.py --pipeline
```

#### C. Chạy huấn luyện tự động và xem trực tiếp màn hình
> [!WARNING]
> Việc mở màn hình đồ họa (`--render`) sẽ khóa tốc độ game bằng tốc độ hiển thị mắt nhìn (60 FPS). Tốc độ train sẽ bị dìm xuống mức rất thấp. Chỉ nên dùng khi cần debug kiểm tra hành vi.
```bash
python train_AI/train_ai.py --pipeline --render
```

#### D. Tiếp tục học từ một Phase bị gián đoạn
Nếu quá trình huấn luyện bị ngắt giữa chừng ở Phase X, bạn có thể chạy tiếp tục từ checkpoint gần nhất:
```bash
python train_AI/train_ai.py --phase 4 --resume
```

#### E. Xem AI trình diễn sau khi train xong
Sau khi huấn luyện thành công Phase X, bạn có thể thưởng thức AI di chuyển và bắn hạ đối thủ bằng lệnh:
```bash
python train_AI/train_ai.py --test 8
```

---
*Phát triển bởi [caohung2006](https://github.com/caohung2006)*