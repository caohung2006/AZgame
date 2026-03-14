cmake_minimum_required(VERSION 3.10)
project(AZgame)

# 1. Tắt các module không cần thiết của Box2D (như testbed, docs) để build nhanh hơn
set(BOX2D_BUILD_TESTBED OFF CACHE BOOL "" FORCE)
set(BOX2D_BUILD_UNIT_TESTS OFF CACHE BOOL "" FORCE)
set(BOX2D_BUILD_DOCS OFF CACHE BOOL "" FORCE)

# 2. Báo cho CMake biết phải build luôn cả mã nguồn trong thư mục box2d
add_subdirectory(box2d)

find_package(raylib CONFIG REQUIRED)


# 3. Khai báo file chạy chính của bạn
add_executable(AZgame main.cpp world.cpp gamemap.cpp render.cpp tank.cpp bullet.cpp)

# 4. Liên kết AZgame với thư viện box2d vừa được biên dịch từ thư mục con
target_link_libraries(AZgame PRIVATE box2d raylib)