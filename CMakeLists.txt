cmake_minimum_required(VERSION 3.10)
project(AZgame)

# 1. Tắt các module không cần thiết của Box2D (như testbed, docs) để build nhanh hơn
set(BOX2D_BUILD_TESTBED OFF CACHE BOOL "" FORCE)
set(BOX2D_BUILD_UNIT_TESTS OFF CACHE BOOL "" FORCE)
set(BOX2D_BUILD_DOCS OFF CACHE BOOL "" FORCE)

# 2. Báo cho CMake biết phải build luôn cả mã nguồn trong thư mục box2d
add_subdirectory(box2d)

find_package(raylib CONFIG REQUIRED)

add_executable(AZgame main.cpp game.cpp map.cpp render.cpp tank.cpp bullet.cpp ui.cpp portal.cpp)

# 4. Liên kết AZgame với thư viện box2d vừa được biên dịch từ thư mục con
target_link_libraries(AZgame PRIVATE box2d raylib)

# 5. Chỉ định thư mục chứa các file Header (.h)
target_include_directories(AZgame PRIVATE include)

# ============================================================
# 6. Bot executable — AI-controlled tank (không sửa AZgame gốc)
# ============================================================
add_executable(AZgame_bot bot_main.cpp bot_game.cpp bot_tank.cpp tank.cpp bullet.cpp map.cpp ui.cpp portal.cpp)
target_link_libraries(AZgame_bot PRIVATE box2d raylib)
target_include_directories(AZgame_bot PRIVATE include)

# ============================================================
# 7. Dodge test executable — ShooterBot (bất tử) vs BotTank
# ============================================================
add_executable(AZgame_test test_main.cpp test_game.cpp bot_tank.cpp shooter_bot.cpp tank.cpp bullet.cpp map.cpp)
target_link_libraries(AZgame_test PRIVATE box2d raylib)
target_include_directories(AZgame_test PRIVATE include)