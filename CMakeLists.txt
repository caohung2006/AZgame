cmake_minimum_required(VERSION 3.10)
project(AZgame)

# Xuất file compile_commands.json để cho clangd hiểu được cấu trúc dự án
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ── THIẾT LẬP TỐI ƯU HÓA HIỆU NĂNG ──
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O3 -march=native")
message(STATUS "Build type: ${CMAKE_BUILD_TYPE}")

# 1. Tắt các module không cần thiết của Box2D (như testbed, docs) để build nhanh hơn
set(BOX2D_BUILD_TESTBED OFF CACHE BOOL "" FORCE)
set(BOX2D_BUILD_UNIT_TESTS OFF CACHE BOOL "" FORCE)
set(BOX2D_BUILD_DOCS OFF CACHE BOOL "" FORCE)

# 2. Báo cho CMake biết phải build luôn cả mã nguồn trong thư mục box2d
add_subdirectory(box2d)

find_package(raylib CONFIG REQUIRED)

add_executable(AZgame main.cpp game.cpp map.cpp renderer.cpp tank.cpp bullet.cpp ui.cpp portal.cpp item.cpp)

# 4. Liên kết AZgame với thư viện box2d vừa được biên dịch từ thư mục con
target_link_libraries(AZgame PRIVATE box2d raylib)

# 5. Chỉ định thư mục chứa các file Header (.h)
target_include_directories(AZgame PRIVATE include neat train)

# ── AZtrain: headless training binary — NO Raylib dependency ─────────────────
add_executable(AZtrain
    train/train.cpp
    game.cpp map.cpp tank.cpp bullet.cpp portal.cpp item.cpp
)
target_link_libraries(AZtrain PRIVATE box2d)
target_include_directories(AZtrain PRIVATE include neat train)
target_compile_features(AZtrain PRIVATE cxx_std_17)   # std::filesystem

# ── OpenMP: Parallel genome evaluation (all cores) ──────────────────────────
find_package(OpenMP)
if(OpenMP_CXX_FOUND)
    target_link_libraries(AZtrain PRIVATE OpenMP::OpenMP_CXX)
    message(STATUS "⚡ OpenMP found — AZtrain will use parallel evaluation")
else()
    message(WARNING "⚠ OpenMP NOT found — AZtrain will run single-threaded")
endif()
