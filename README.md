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

## 🚀 Hướng dẫn Cài đặt & Biên dịch

### 🟢 Cách 1: Sử dụng MSYS2 (Khuyên dùng cho Windows)

**Bước 1: Cài đặt MSYS2**
1. Truy cập trang chủ MSYS2 và tải file cài đặt `.exe`.
2. Tiến hành cài đặt MSYS2 vào máy (thường là `C:\msys64`).
3. Sau khi cài xong, mở ứng dụng **MSYS2 UCRT64** (hoặc **MSYS2 MinGW 64-bit**) từ Start Menu.

**Bước 2: Cài đặt Toolchain (GCC, CMake, Make) và Raylib**
Trong cửa sổ terminal của MSYS2, chạy các lệnh sau để cập nhật hệ thống và cài đặt các công cụ cần thiết:

```bash
# 1. Cập nhật các gói phần mềm hệ thống (Bấm Y nếu được hỏi)
pacman -Syu

# 2. Cài đặt trình biên dịch GCC, CMake và công cụ Make (phiên bản MinGW 64-bit)
pacman -S mingw-w64-x86_64-toolchain mingw-w64-x86_64-cmake

# 3. Cài đặt thư viện Raylib
pacman -S mingw-w64-x86_64-raylib
```

**Bước 3: Biên dịch dự án**
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

**Bước 4: Chạy Game**
Sau khi biên dịch thành công, một file `AZgame.exe` sẽ xuất hiện trong thư mục `build`. Bạn có thể chạy ngay bằng lệnh:
```bash
./AZgame.exe
```
*Hoặc vào trực tiếp thư mục `build` bằng File Explorer của Windows và nhấp đúp vào `AZgame.exe`.*

### 🔵 Cách 2: Cài đặt thủ công (Tải CMake trực tiếp & Dùng IDE có sẵn)
Nếu bạn không muốn cài đặt công cụ qua MSYS2, bạn có thể thiết lập môi trường độc lập như sau:

**1. Cài đặt Trình biên dịch & CMake:**
- **Trình biên dịch C++:** Bạn có thể sử dụng Visual Studio (chọn workload *C++ Desktop Development*) hoặc trình biên dịch MinGW-w64.
- **CMake:** Tải file cài đặt `.msi` mới nhất từ Trang chủ CMake. Trong quá trình cài, hãy nhớ tích chọn **"Add CMake to the system PATH"** để có thể gọi lệnh CMake từ bất kỳ terminal nào.

**2. Cài đặt thư viện Raylib:**
- Trên Windows, cách tốt nhất là cài đặt qua trình quản lý thư viện **vcpkg** của Microsoft:
  ```bash
  vcpkg install raylib
  ```
- *Hoặc* tải trực tiếp mã nguồn dựng sẵn (pre-compiled) từ GitHub Raylib Releases và tự cấu hình biến môi trường hệ thống.

**3. Biên dịch dự án:**
Mở Command Prompt, PowerShell hoặc terminal của VS Code tại thư mục chứa game và chạy lệnh sau:
```bash
mkdir build
cd build

# Khởi tạo cấu hình CMake (thêm CMAKE_TOOLCHAIN_FILE nếu bạn cài raylib qua vcpkg)
cmake -DCMAKE_TOOLCHAIN_FILE=[đường_dẫn_tới_vcpkg]/scripts/buildsystems/vcpkg.cmake ..

# Biên dịch (CMake sẽ tự động gọi trình biên dịch MSVC hoặc GCC tùy hệ thống)
cmake --build .
```

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

## 🛠️ Cấu trúc mã nguồn (Mô hình Hướng đối tượng - OOP)
- `main.cpp`: Khởi chạy chương trình và gọi thực thi lớp Game.
- `game.h / .cpp`: Lớp `Game` trung tâm quản lý vòng lặp, cập nhật logic và kết xuất đồ họa.
- `tank.h / .cpp`: Lớp `Tank` chứa Box2D body, nhận diện phím bấm di chuyển và bắn đạn.
- `bullet.h / .cpp`: Lớp `Bullet` mô phỏng đường đạn bay và giới hạn thời gian tồn tại.
- `gamemap.h / .cpp`: Lớp `GameMap` tự động tạo tường tĩnh b2_staticBody từ mảng ký tự.
- `portal.h / .cpp`: Lớp `Portal` tính toán khoảng cách để dịch chuyển xe tăng/đạn qua lại.
- `ui.h / ui.cpp`: Lớp tĩnh `UI` dùng để hiển thị menu cài đặt, điểm số và nhận nhập liệu.
- `constants.h`: Chứa các định nghĩa thông số (độ phân giải, tỷ lệ scale pixel - mét).

---

## ⚠️ Các lỗi thường gặp & Cách khắc phục

1. **Lỗi `Could not find a package configuration file provided by "raylib"` khi chạy lệnh `cmake`:**
   - *Nguyên nhân:* Hệ thống đang vô tình gọi bản CMake mặc định của Windows thay vì bản CMake trong môi trường MSYS2.
   - *Khắc phục:* Đảm bảo bạn đang sử dụng ứng dụng terminal **MSYS2 MinGW 64-bit**. Hãy gõ thử lệnh `which cmake` vào terminal, nếu kết quả hiện ra không phải là `/mingw64/bin/cmake`, bạn hãy xem lại Bước 2 và cài đặt đủ gói CMake của MinGW.

2. **Lỗi `gcc: command not found` hoặc `mingw32-make: command not found`:**
   - *Khắc phục:* Môi trường MSYS2 chưa được cài đặt bộ biên dịch. Chạy lại lệnh `pacman -S mingw-w64-x86_64-toolchain` và nhấn phím Y để cài đặt.

*Nếu bạn cần thêm sự trợ giúp, vui lòng tham khảo tại [Google Gemini](https://gemini.google.com/).*