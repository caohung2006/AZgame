# AZ Tank Game

AZ Tank là một tựa game bắn xe tăng đồ hoạ 2D cổ điển hỗ trợ nhiều người chơi (Local Multiplayer) lên đến 4 người. Game được phát triển bằng ngôn ngữ **C++**, sử dụng thư viện **Raylib** để xử lý đồ hoạ/nhập liệu và **Box2D** để mô phỏng vật lý chân thực (đạn nảy, cổng dịch chuyển, va chạm).

---

## 🎮 Tính năng nổi bật
- Hỗ trợ từ 1 đến 4 người chơi cùng lúc trên cùng 1 máy tính.
- Tự do tùy chỉnh phím điều khiển cho từng người chơi.
- Vật lý thực tế: Đạn nảy dội tường, va chạm giữa các xe tăng.
- Cơ chế **Cổng dịch chuyển (Portal)** ngẫu nhiên xuất hiện trên bản đồ.
- Bản đồ mê cung đặc trưng với các khu vực ẩn nấp.

---

## ⚙️ Yêu cầu hệ thống
Dự án này sử dụng **CMake** để quản lý quá trình biên dịch. Để chạy được trên Windows, bạn cần cài đặt **MSYS2** để có thể tải trình biên dịch C++ (GCC) và thư viện Raylib.

*(Lưu ý: Thư viện Box2D đã được tích hợp sẵn trong thư mục `box2d` của mã nguồn, tự động build qua CMake).*

---

## 🚀 Hướng dẫn Cài đặt & Biên dịch (Dành cho Windows sử dụng MSYS2)

### Bước 1: Cài đặt MSYS2
1. Truy cập trang chủ MSYS2 và tải file cài đặt `.exe`.
2. Tiến hành cài đặt MSYS2 vào máy (thường là `C:\msys64`).
3. Sau khi cài xong, mở ứng dụng **MSYS2 UCRT64** (hoặc **MSYS2 MinGW 64-bit**) từ Start Menu.

### Bước 2: Cài đặt Toolchain (GCC, CMake, Make) và Raylib
Trong cửa sổ terminal của MSYS2, chạy các lệnh sau để cập nhật hệ thống và cài đặt các công cụ cần thiết:

```bash
# 1. Cập nhật các gói phần mềm hệ thống (Bấm Y nếu được hỏi)
pacman -Syu

# 2. Cài đặt trình biên dịch GCC, CMake và công cụ Make (phiên bản MinGW 64-bit)
pacman -S mingw-w64-x86_64-toolchain mingw-w64-x86_64-cmake

# 3. Cài đặt thư viện Raylib
pacman -S mingw-w64-x86_64-raylib
```

### Bước 3: Biên dịch dự án
Vẫn trong terminal của MSYS2 (khuyên dùng **MSYS2 MinGW 64-bit**), sử dụng lệnh `cd` để điều hướng đến thư mục mã nguồn chứa file `CMakeLists.txt` của game. Ví dụ:
```bash
cd /d/HungCao/Project/AZgame
```

Sau đó, tiến hành tạo thư mục build và biên dịch:
```bash
# Tạo thư mục build để chứa file thực thi
mkdir build
cd build

# Khởi tạo cấu hình CMake
cmake -G "MinGW Makefiles" ..

# Bắt đầu biên dịch (Quá trình này sẽ compile cả Box2D và Source game)
mingw32-make
```

### Bước 4: Chạy Game
Sau khi biên dịch thành công, một file `AZgame.exe` sẽ xuất hiện trong thư mục `build`. Bạn có thể chạy ngay bằng lệnh:
```bash
./AZgame.exe
```
*Hoặc vào trực tiếp thư mục `build` bằng File Explorer của Windows và nhấp đúp vào `AZgame.exe`.*

---

## 🕹️ Hướng dẫn chơi
Khi vào game, bạn có thể click vào nút **SETTINGS** ở góc trên bên phải để tùy chỉnh số lượng người chơi và thiết lập lại phím.

**Phím mặc định (dành cho 2 người chơi đầu tiên):**
- **Người chơi 1 (Xe tăng Xanh lá):**
  - Di chuyển: `W`, `A`, `S`, `D`
  - Bắn đạn: `Q`
- **Người chơi 2 (Xe tăng Xanh dương):**
  - Di chuyển: `Lên`, `Xuống`, `Trái`, `Phải` (Phím mũi tên)
  - Bắn đạn: `/` (Phím gạch chéo)

*(Lưu ý: Đạn bắn ra sẽ bị dội lại khi chạm tường, hãy cẩn thận đừng để trúng đạn của chính mình nhé!)*

---

## 🛠️ Cấu trúc mã nguồn
- `main.cpp` / `world.cpp`: Vòng lặp chính của game và quản lý logic ván đấu, điểm số.
- `tank.cpp`: Quản lý vật lý Box2D cho xe tăng, thao tác phím và xử lý bắn.
- `bullet.cpp`: Khởi tạo đạn, hệ thống phản hồi va chạm và quản lý thời gian tồn tại.
- `gamemap.cpp`: Thuật toán sinh các khối tường trong mê cung bằng b2_staticBody.
- `render.cpp`: Xử lý vẽ đồ họa toàn bộ môi trường và nhân vật bằng Raylib.
- `portal.cpp`: Thuật toán sinh ngẫu nhiên và dịch chuyển người chơi qua không gian.

*Nếu bạn cần thêm sự trợ giúp trong quá trình phát triển, vui lòng tham khảo tại [Google Gemini](https://gemini.google.com/).*