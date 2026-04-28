"""
Script huấn luyện AI xe tăng trên Google Colab.
Phiên bản sửa đổi từ train_ai.py gốc:
  - Dùng gymnasium_wrapper_colab (Pure Python) thay vì C++
  - Bỏ render (Colab không có GUI)
  - Tích hợp Google Drive để lưu model
  - Tối ưu cho Colab runtime
"""

import os
import sys
import gc

from stable_baselines3 import PPO
from stable_baselines3.common.env_util import make_vec_env
from stable_baselines3.common.callbacks import CheckpointCallback, BaseCallback
from gymnasium_wrapper_colab import AZTankEnv


class ProgressCallback(BaseCallback):
    def __init__(self, print_freq=10_000, verbose=0):
        super().__init__(verbose)
        self.print_freq = print_freq

    def _on_step(self) -> bool:
        if self.n_calls % self.print_freq == 0:
            print(f"  [Bước {self.num_timesteps:>10,}] Cập nhật quá trình học...")
        return True


# Lộ trình 7 giai đoạn (Curriculum Learning)
PHASES = {
    1: {"map": False, "items": False, "mode": 0, "steps": 500_000,   "op_phase": None},
    2: {"map": True,  "items": False, "mode": 0, "steps": 1_000_000, "op_phase": None},
    3: {"map": False, "items": False, "mode": 0, "steps": 1_500_000, "op_phase": 2},
    4: {"map": True,  "items": False, "mode": 0, "steps": 2_000_000, "op_phase": 3},
    5: {"map": True,  "items": False, "mode": 0, "steps": 2_000_000, "op_phase": 4},
    6: {"map": True,  "items": True,  "mode": 0, "steps": 2_000_000, "op_phase": 5},
    7: {"map": True,  "items": True,  "mode": 0, "steps": 3_000_000, "op_phase": 6},
}

# Thư mục lưu model (thay đổi nếu dùng Google Drive)
MODEL_DIR = "models"
LOG_DIR = "logs"


def load_opponent(phase_config):
    if phase_config["op_phase"] is None:
        return None
    op_path = f"{MODEL_DIR}/ppo_tank_phase{phase_config['op_phase']}.zip"
    if os.path.exists(op_path):
        print(f"  [Info] Tải model đối thủ từ Phase {phase_config['op_phase']}...")
        return PPO.load(op_path)
    print(f"  [Cảnh báo] Không tìm thấy model đối thủ tại {op_path}. Đối thủ sẽ đứng im!")
    return None


def make_env(phase_id, opponent_model=None):
    cfg = PHASES[phase_id]
    return AZTankEnv(
        num_players=2,
        map_enabled=cfg["map"],
        items_enabled=cfg["items"],
        training_mode=cfg["mode"],
        opponent_model=opponent_model,
        render_mode=None  # Colab không có GUI
    )


def train_phase(phase_id, resume_model_path=None):
    cfg = PHASES[phase_id]
    os.makedirs(MODEL_DIR, exist_ok=True)
    os.makedirs(LOG_DIR, exist_ok=True)

    model_path = f"{MODEL_DIR}/ppo_tank_phase{phase_id}"

    print("=" * 60)
    print(f"  BẮT ĐẦU HUẤN LUYỆN - GIAI ĐOẠN {phase_id}")
    print(f"  Map: {cfg['map']} | Items: {cfg['items']} | Steps: {cfg['steps']:,}")
    print("=" * 60)

    opponent_model = load_opponent(cfg)

    # Colab thường có 2 vCPU → dùng 4 envs song song
    num_envs = 4
    env = make_vec_env(lambda: make_env(phase_id, opponent_model), n_envs=num_envs)

    if resume_model_path and os.path.exists(resume_model_path + ".zip"):
        print(f"  [Info] Kế thừa trí tuệ từ model: {resume_model_path}.zip")
        model = PPO.load(resume_model_path, env=env)
    elif phase_id > 1 and os.path.exists(f"{MODEL_DIR}/ppo_tank_phase{phase_id - 1}.zip"):
        prev_path = f"{MODEL_DIR}/ppo_tank_phase{phase_id - 1}"
        print(f"  [Info] Kế thừa trí tuệ từ Phase {phase_id - 1}")
        model = PPO.load(prev_path, env=env)
    else:
        print("  [Info] Khởi tạo Model hoàn toàn mới!")
        model = PPO(
            policy="MlpPolicy",
            env=env,
            learning_rate=3e-4,
            n_steps=4096,
            batch_size=128,
            n_epochs=10,
            gamma=0.99,
            ent_coef=0.01,
            verbose=1,
            tensorboard_log=f"./{LOG_DIR}/curriculum/",
            device="auto"  # Tự động chọn GPU nếu có
        )

    checkpoint_cb = CheckpointCallback(
        save_freq=200_000, 
        save_path=f"./{MODEL_DIR}/checkpoints/",
        name_prefix=f"ppo_phase{phase_id}"
    )
    progress_cb = ProgressCallback(print_freq=50_000)

    try:
        model.learn(
            total_timesteps=cfg["steps"],
            callback=[checkpoint_cb, progress_cb],
            reset_num_timesteps=False
        )
        model.save(model_path)
        print(f"\n  [Hoàn Thành] Giai đoạn {phase_id} lưu tại: {model_path}.zip\n")

        env.close()
        del model, opponent_model, env
        gc.collect()

        try:
            import torch
            if torch.cuda.is_available():
                torch.cuda.empty_cache()
        except ImportError:
            pass

    except KeyboardInterrupt:
        model.save(model_path)
        print(f"\n  Người dùng dừng sớm, vẫn lưu model tại: {model_path}.zip\n")


def pipeline():
    print("\n🚀 Bắt đầu Curriculum Learning trên Colab 🚀\n")
    for phase_id in sorted(PHASES.keys()):
        expected_path = f"{MODEL_DIR}/ppo_tank_phase{phase_id}.zip"
        if os.path.exists(expected_path):
            print(f"  ⏩ Đã có model Phase {phase_id}, tự bỏ qua...")
            continue
        train_phase(phase_id)
    print("\n✅ TẤT CẢ CÁC GIAI ĐOẠN ĐÃ HOÀN THÀNH!")


if __name__ == "__main__":
    # Mặc định: chạy full pipeline
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument("--phase", type=int, choices=range(1, 8), help="Chỉ chạy 1 phase")
    parser.add_argument("--resume", action="store_true", help="Tiếp tục từ file save")
    args = parser.parse_args()

    if args.phase:
        resume_path = f"{MODEL_DIR}/ppo_tank_phase{args.phase}" if args.resume else None
        train_phase(args.phase, resume_model_path=resume_path)
    else:
        pipeline()
