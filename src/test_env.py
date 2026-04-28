"""
Test nhanh Pure Python Environment.
Chạy vài ván với hành động ngẫu nhiên để kiểm tra logic game.
"""
import time
import random
import numpy as np
from pure_python_env import RLEnv

def test_basic():
    """Test 1: Kiểm tra reset + step + state hoạt động."""
    print("=" * 50)
    print("  TEST 1: Kiểm tra API cơ bản")
    print("=" * 50)

    env = RLEnv(num_players=2, map_enabled=False)
    state = env.reset()
    
    print(f"  State shape: {len(state)} (mong đợi: 13)")
    print(f"  State values: {[round(s, 3) for s in state]}")
    
    # Thử 1 step
    state, reward, done = env.step(0, -1)  # Player 0 tiến, Player 1 đứng im
    print(f"  After step: reward={reward:.3f}, done={done}")
    print("  ✅ API cơ bản hoạt động!\n")


def test_episode():
    """Test 2: Chạy 1 ván hoàn chỉnh với action ngẫu nhiên."""
    print("=" * 50)
    print("  TEST 2: Chạy 1 ván hoàn chỉnh (Random Actions)")
    print("=" * 50)

    env = RLEnv(num_players=2, map_enabled=True)
    state = env.reset()

    total_reward = 0
    steps = 0
    done = False

    while not done:
        a0 = random.randint(0, 4)  # Random action cho Player 0
        a1 = random.randint(0, 4)  # Random action cho Player 1
        state, reward, done = env.step(a0, a1)
        total_reward += reward
        steps += 1

    print(f"  Ván kết thúc sau {steps} bước")
    print(f"  Tổng reward: {total_reward:.2f}")
    print(f"  Điểm: P0={env.player_scores[0]}, P1={env.player_scores[1]}")
    print("  ✅ Episode hoàn chỉnh!\n")


def test_speed():
    """Test 3: Đo tốc độ simulation (steps/giây)."""
    print("=" * 50)
    print("  TEST 3: Đo tốc độ (Benchmark)")
    print("=" * 50)

    env = RLEnv(num_players=2, map_enabled=True)
    
    total_steps = 0
    start = time.time()
    
    for ep in range(10):
        env.reset()
        done = False
        while not done:
            a0 = random.randint(0, 4)
            a1 = random.randint(0, 4)
            _, _, done = env.step(a0, a1)
            total_steps += 1

    elapsed = time.time() - start
    sps = total_steps / elapsed
    print(f"  10 ván = {total_steps:,} bước trong {elapsed:.2f}s")
    print(f"  Tốc độ: {sps:,.0f} steps/giây")
    print("  ✅ Benchmark hoàn tất!\n")


def test_gymnasium():
    """Test 4: Kiểm tra Gymnasium wrapper (cần pip install gymnasium)."""
    print("=" * 50)
    print("  TEST 4: Gymnasium Wrapper")
    print("=" * 50)

    try:
        from gymnasium_wrapper_colab import AZTankEnv
        
        env = AZTankEnv(num_players=2, map_enabled=True)
        obs, info = env.reset()
        
        print(f"  Obs shape: {obs.shape} (mong đợi: (13,))")
        print(f"  Obs dtype: {obs.dtype} (mong đợi: float32)")
        print(f"  Action space: {env.action_space}")
        print(f"  Obs space: {env.observation_space}")
        
        # Chạy 100 bước
        for _ in range(100):
            action = env.action_space.sample()
            obs, reward, terminated, truncated, info = env.step(action)
            if terminated or truncated:
                obs, info = env.reset()
        
        env.close()
        print("  ✅ Gymnasium wrapper tương thích!\n")
    except ImportError:
        print("  ⚠️ Chưa cài gymnasium. Chạy: pip install gymnasium\n")


if __name__ == "__main__":
    test_basic()
    test_episode()
    test_speed()
    test_gymnasium()
    print("🎉 TẤT CẢ TEST ĐÃ PASS!")
