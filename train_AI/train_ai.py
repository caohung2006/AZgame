"""
Script huấn luyện AI xe tăng (Curriculum Learning 7 Giai đoạn)
Hỗ trợ Self-Play (Lấy AI cũ làm đối thủ cho AI mới tập né).
"""

import os
import argparse
import sys

# Đảm bảo in tiếng Việt ra terminal Windows không bị lỗi
if sys.stdout.encoding.lower() != 'utf-8':
    sys.stdout.reconfigure(encoding='utf-8')

from stable_baselines3 import PPO
from stable_baselines3.common.env_util import make_vec_env
from stable_baselines3.common.callbacks import CheckpointCallback, BaseCallback
from gymnasium_wrapper import AZTankEnv

class ProgressCallback(BaseCallback):
    def __init__(self, print_freq=10_000, verbose=0):
        super().__init__(verbose)
        self.print_freq = print_freq

    def _on_step(self) -> bool:
        if self.n_calls % self.print_freq == 0:
            print(f"  [Bước {self.num_timesteps:>10,}] "
                  f"Cập nhật quá trình học...")
        return True

# Lộ trình 7 giai đoạn tối ưu hóa (Khắc phục Catastrophic Forgetting)
PHASES = {
    # 1. Tập đi bãi đất trống & bắn thẳng (Địch đứng im)
    1: {"map": False, "items": False, "mode": 0, "steps": 500_000,   "op_phase": None},
    
    # 2. Xử lý vật cản: Tìm đường qua tường & bắn địch đứng im
    2: {"map": True,  "items": False, "mode": 0, "steps": 1_000_000, "op_phase": None},
    
    # 3. Solo đấu súng bãi trống: Bắt đầu cho địch (Phase 2) phản công. Học né đạn & bắn mục tiêu di động
    3: {"map": False, "items": False, "mode": 0, "steps": 1_500_000, "op_phase": 2}, 
    
    # 4. Tác chiến đô thị: Mang kỹ năng đối kháng ở Phase 3 vào môi trường có tường (hit-and-run)
    4: {"map": True,  "items": False, "mode": 0, "steps": 2_000_000, "op_phase": 3},
    
    # 5. Self-Play Vòng 1: Đánh với chính mình để leo rank
    5: {"map": True,  "items": False, "mode": 0, "steps": 2_000_000, "op_phase": 4},
    
    # 6. Mở khóa nhặt đồ & dịch chuyển: Cấp độ tư duy chiến thuật
    6: {"map": True,  "items": True,  "mode": 0, "steps": 2_000_000, "op_phase": 5},
    
    # 7. Self-Play Vòng 2 (Mastery): Trận chiến sinh tử thực sự
    7: {"map": True,  "items": True,  "mode": 0, "steps": 3_000_000, "op_phase": 6},
}

def load_opponent(phase_config):
    if phase_config["op_phase"] is None:
        return None
    op_path = f"models/ppo_tank_phase{phase_config['op_phase']}.zip"
    if os.path.exists(op_path):
        print(f"  [Info] Tải model đối thủ từ Phase {phase_config['op_phase']}...")
        return PPO.load(op_path)
    print(f"  [Cảnh báo] Không tìm thấy model đối thủ tại {op_path}. Đối thủ sẽ đứng im!")
    return None

def make_env(phase_id, opponent_model=None, render_mode=None):
    cfg = PHASES[phase_id]
    return AZTankEnv(
        num_players=2, # Cần 2 vì còn có địch để bắn
        map_enabled=cfg["map"],
        items_enabled=cfg["items"],
        training_mode=cfg["mode"],
        opponent_model=opponent_model,
        render_mode=render_mode
    )

def train_phase(phase_id, resume_model_path=None, render=False):
    cfg = PHASES[phase_id]
    os.makedirs("models", exist_ok=True)
    os.makedirs("logs", exist_ok=True)
    
    model_path = f"models/ppo_tank_phase{phase_id}"
    
    print("=" * 60)
    print(f"  BẮT ĐẦU HUẤN LUYỆN - GIAI ĐOẠN {phase_id}")
    print(f"  Map: {cfg['map']} | Items: {cfg['items']} | Mode: {cfg['mode']} | Target Steps: {cfg['steps']:,}")
    print("=" * 60)

    # 1. Tải Model Đối thủ (nếu có)
    opponent_model = load_opponent(cfg)

    # 2. Khởi tạo môi trường
    # Nếu đang bật xem trực tiếp (render) thì phải ép n_envs=1 để khỏi bay nhiều cửa sổ
    num_envs = 1 if render else 4
    render_mode = "human" if render else None
    env = make_vec_env(lambda: make_env(phase_id, opponent_model, render_mode), n_envs=num_envs)

    # 3. Khởi tạo Model AI (Tiếp tục từ phase trước, hoặc resume file)
    if resume_model_path and os.path.exists(resume_model_path + ".zip"):
        print(f"  [Info] Kế thừa trí tuệ từ model: {resume_model_path}.zip")
        model = PPO.load(resume_model_path, env=env)
    elif phase_id > 1 and os.path.exists(f"models/ppo_tank_phase{phase_id - 1}.zip"):
        prev_path = f"models/ppo_tank_phase{phase_id - 1}"
        print(f"  [Info] Kế thừa trí tuệ từ Phase {phase_id - 1}")
        model = PPO.load(prev_path, env=env)
    else:
        print("  [Info] Khởi tạo Model hoàn toàn mới!")
        model = PPO(
            policy="MlpPolicy",
            env=env,
            learning_rate=3e-4,
            n_steps=2048,
            batch_size=64,
            n_epochs=10,
            gamma=0.99,
            ent_coef=0.01,
            verbose=1,
            tensorboard_log="./logs/curriculum/"
        )

    # Callback
    checkpoint_cb = CheckpointCallback(save_freq=50_000, save_path="./models/checkpoints/", name_prefix=f"ppo_phase{phase_id}")
    progress_cb = ProgressCallback(print_freq=20_000)

    if render:
        print("\n  [Chú ý] Chế độ biểu diễn ĐANG BẬT. Tốc độ train sẽ bị dìm xuống mức thấp nhất (bằng tốc độ mắt nhìn)...")

    try:
        model.learn(total_timesteps=cfg["steps"], callback=[checkpoint_cb, progress_cb], reset_num_timesteps=False)
        model.save(model_path)
        print(f"\n  [Hoàn Thành] Giai đoạn {phase_id} lưu tại: {model_path}.zip\n")
        
        # --- QUAN TRỌNG: DỌN RÁC BỘ NHỚ ---
        # Tránh việc RAM và VRAM (Card đồ họa) bị ngốn ngày càng gắt khi lưu file mới
        env.close()
        del model
        del opponent_model
        del env
        import gc
        gc.collect()
        import torch
        if torch.cuda.is_available():
            torch.cuda.empty_cache()
            
    except KeyboardInterrupt:
        model.save(model_path)
        print(f"\n  Người dùng dừng sớm, vẫn lưu model tại: {model_path}.zip\n")
        sys.exit(0)

def pipeline(render=False):
    print("\n🚀 Bắt đầu Curriculum Learning 🚀\n")
    for phase_id in sorted(PHASES.keys()):
        expected_path = f"models/ppo_tank_phase{phase_id}.zip"
        if os.path.exists(expected_path):
             print(f"  ⏩ Đã có model Phase {phase_id}, tự bỏ qua...")
             continue
        train_phase(phase_id, render=render)
    print("\n✅ TẤT CẢ CÁC GIAI ĐOẠN ĐÃ HOÀN THÀNH!")

def test_model(phase_id):
    """Mở cửa sổ đồ họa xem thư giãn model đã train (không học)"""
    model_path = f"models/ppo_tank_phase{phase_id}.zip"
    if not os.path.exists(model_path):
        print(f"  [Lỗi] Không tìm thấy model tại {model_path}!")
        return
        
    print(f"\n🎮 Đang TEST Model Phase {phase_id}...")
    cfg = PHASES[phase_id]
    opponent_model = load_opponent(cfg)
    env = make_env(phase_id, opponent_model, render_mode="human")
    model = PPO.load(model_path)
    
    obs, _ = env.reset()
    try:
        while True:
            action, _ = model.predict(obs, deterministic=True)
            obs, reward, done, trunc, info = env.step(action)
            # Dừng vòng lặp nếu người dùng đóng cửa sổ đồ họa
            if not info.get("window_open", True):
                print("\n  [INFO] Đã đóng cửa sổ game.")
                break

            if done or trunc:
                obs, _ = env.reset()
    except KeyboardInterrupt:
        print("\n  [INFO] Dừng test model.")
    finally:
        env.close()

if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--pipeline", action="store_true", help="Chạy tự động từ GĐ 1 đến 7")
    # parser.add_argument("--phase", type=int, choices=range(1, 8), help="Chỉ định chạy 1 GĐ cụ thể")
    # parser.add_argument("--render", action="store_true", help="Mở cửa sổ Raylib xem (TRAIN RẤT CHẬM)")
    # parser.add_argument("--test", type=int, choices=range(1, 8), help="Xem AI múa ở Phase X (sau khi train)")
    # parser.add_argument("--resume", action="store_true", help="Tiếp tục học từ file save đang dở")
    args = parser.parse_args()

    if args.test:
        test_model(args.test)
    elif args.pipeline:
        pipeline(render=args.render)
    elif args.phase:
        # Nếu bật resume, truyền đường dẫn file hiện tại vào để cấy gene học tiếp
        resume_path = f"models/ppo_tank_phase{args.phase}" if args.resume else None
        train_phase(args.phase, resume_model_path=resume_path, render=args.render)
    else:
        print("Vui lòng chọn cách chạy:")
        print("  python train_ai.py --pipeline             (Chạy tốc độ rùa, không màn hình)")
        print("  python train_ai.py --pipeline --render    (Mở màn hình xem quá trình, CỰC CHẬM)")
        print("  python train_ai.py --phase 1 --resume     (Học tiếp file save nếu bị ngắt giữa chừng)")
        print("  python train_ai.py --test 7               (Xem AI diễn hài SAU KHI train xong)")
