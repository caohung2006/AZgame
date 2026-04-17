"""
Script huấn luyện AI xe tăng bằng thuật toán PPO (Proximal Policy Optimization).

Cách dùng:
    python train_ai.py            # Bắt đầu huấn luyện mới
    python train_ai.py --resume   # Tiếp tục từ model đã lưu
"""

import os
import argparse
import sys

# Đảm bảo in tiếng Việt ra terminal Windows không bị lỗi
if sys.stdout.encoding.lower() != 'utf-8':
    sys.stdout.reconfigure(encoding='utf-8')

from stable_baselines3 import PPO
from stable_baselines3.common.env_util import make_vec_env
from stable_baselines3.common.callbacks import (
    EvalCallback,
    CheckpointCallback,
    BaseCallback
)
from gymnasium_wrapper import AZTankEnv


# -------------------------------------------------------
# Callback in tiến độ ra màn hình mỗi N bước
# -------------------------------------------------------
class ProgressCallback(BaseCallback):
    def __init__(self, print_freq=10_000, verbose=0):
        super().__init__(verbose)
        self.print_freq = print_freq

    def _on_step(self) -> bool:
        if self.n_calls % self.print_freq == 0:
            print(f"  [Bước {self.num_timesteps:>10,}] "
                  f"Trung bình phần thưởng: {self.locals.get('rewards', [0])}")
        return True  # Trả về False nếu muốn dừng huấn luyện sớm


# -------------------------------------------------------
# Hàm tạo môi trường (Curriculum Learning)
# Level 1: Phòng trống (không map, không vật phẩm) -> AI học cơ bản
# Level 2: Có bản đồ (thêm vật cản)
# Level 3: Có vật phẩm (đầy đủ)
# -------------------------------------------------------
def make_env(curriculum_level=1):
    map_enabled   = curriculum_level >= 2
    items_enabled = curriculum_level >= 3
    return AZTankEnv(
        num_players=2,
        map_enabled=map_enabled,
        items_enabled=items_enabled
    )


def train(resume=False, curriculum_level=1):
    # Tạo thư mục lưu model và log
    os.makedirs("models", exist_ok=True)
    os.makedirs("logs", exist_ok=True)

    model_path = f"models/ppo_tank_level{curriculum_level}"

    print("=" * 50)
    print(f"  Bắt đầu huấn luyện - Curriculum Level {curriculum_level}")
    print(f"  Map: {'Có' if curriculum_level >= 2 else 'Không'} | "
          f"Vật phẩm: {'Có' if curriculum_level >= 3 else 'Không'}")
    print("=" * 50)

    # Tạo môi trường. num_envs=4 nghĩa là chạy 4 phiên game song song để học nhanh hơn
    env = make_vec_env(lambda: make_env(curriculum_level), n_envs=4)

    if resume and os.path.exists(model_path + ".zip"):
        # Tải model đã lưu
        print(f"  Đang tải model từ: {model_path}.zip")
        model = PPO.load(model_path, env=env)
    else:
        # Tạo model PPO mới với mạng neural 2 lớp ẩn (64 neurons mỗi lớp)
        model = PPO(
            policy="MlpPolicy",             # Mạng neural nhiều lớp (Multi-Layer Perceptron)
            env=env,
            learning_rate=3e-4,             # Tốc độ học: không quá nhanh, không quá chậm
            n_steps=2048,                   # Số bước trước khi cập nhật model
            batch_size=64,                  # Số mẫu mỗi lần học
            n_epochs=10,                    # Số lần học lại trên mỗi tập dữ liệu
            gamma=0.99,                     # Hệ số chiết khấu tương lai (0=chỉ xem trước mắt, 1=nhìn xa)
            ent_coef=0.01,                  # Khuyến khích thử nghiệm (Exploration)
            verbose=1,                      # In log ra màn hình
            tensorboard_log="./logs/",      # Ghi log để xem bằng TensorBoard
            policy_kwargs=dict(
                net_arch=[64, 64]           # Kiến trúc mạng: 2 lớp ẩn, mỗi lớp 64 neurons
            )
        )

    # Callback: Lưu model tự động mỗi 50,000 bước
    checkpoint_cb = CheckpointCallback(
        save_freq=50_000,
        save_path="./models/checkpoints/",
        name_prefix=f"ppo_tank_level{curriculum_level}"
    )

    # Callback: In tiến độ mỗi 10,000 bước
    progress_cb = ProgressCallback(print_freq=10_000)

    print("\n  Bắt đầu huấn luyện... (Nhấn Ctrl+C để dừng và lưu lại)\n")

    try:
        model.learn(
            total_timesteps=500_000,        # Tổng số bước huấn luyện
            callback=[checkpoint_cb, progress_cb],
            reset_num_timesteps=not resume  # Nếu resume thì tiếp tục đếm bước
        )
    except KeyboardInterrupt:
        print("\n  Người dùng dừng huấn luyện sớm.")

    # Lưu model sau khi xong
    model.save(model_path)
    print(f"\n  Model đã được lưu vào: {model_path}.zip")
    print("  Dùng lệnh 'python train_ai.py --resume' để tiếp tục huấn luyện.\n")


def test_model(curriculum_level=1):
    """Chạy thử AI đã được huấn luyện để xem nó chơi."""
    model_path = f"models/ppo_tank_level{curriculum_level}.zip"
    if not os.path.exists(model_path):
        print(f"  Không tìm thấy model tại {model_path}. Hãy train trước!")
        return

    env = make_env(curriculum_level)
    model = PPO.load(model_path)

    print(f"\n  Đang chạy thử AI (Level {curriculum_level})...\n")
    obs, _ = env.reset()
    total_reward = 0
    n_steps = 0

    for _ in range(2000):
        action, _ = model.predict(obs, deterministic=True)
        obs, reward, terminated, truncated, _ = env.step(action)
        total_reward += reward
        n_steps += 1
        if terminated or truncated:
            print(f"  Ván kết thúc sau {n_steps} bước | Tổng thưởng: {total_reward:.2f}")
            obs, _ = env.reset()
            total_reward = 0
            n_steps = 0


# -------------------------------------------------------
# Entry point
# -------------------------------------------------------
if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Huấn luyện AI xe tăng với PPO")
    parser.add_argument("--resume", action="store_true",
                        help="Tiếp tục huấn luyện từ model đã lưu")
    parser.add_argument("--test", action="store_true",
                        help="Chạy thử AI đã huấn luyện (không train)")
    parser.add_argument("--level", type=int, default=1, choices=[1, 2, 3],
                        help="Curriculum level (1=đơn giản, 2=có map, 3=đầy đủ)")
    args = parser.parse_args()

    if args.test:
        test_model(curriculum_level=args.level)
    else:
        train(resume=args.resume, curriculum_level=args.level)
