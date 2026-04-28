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

# Lộ trình 7 giai đoạn tối ưu hóa cho 1HP + 45 States + Fat RayCast + Bot
PHASES = {
    # 1. Cơ bản: Đánh với Bot Level 1 (Bia tập bắn)
    1: {"map": False, "items": False, "mode": 0, "steps": 400_000,   "bot_level": 1, "op_phase": None},
    
    # 2. Né đạn: Bãi trống, bị bắn (mode=2, khóa súng) với Bot Level 2 (Đuổi theo bắn)
    2: {"map": False, "items": False, "mode": 2, "steps": 800_000, "bot_level": 2, "op_phase": None},
    
    # 3. Mê cung: Đánh với Bot Level 2 (Đuổi theo qua mê cung)
    3: {"map": True,  "items": False, "mode": 0, "steps": 1_000_000, "bot_level": 2, "op_phase": None},
    
    # 4. Đấu tay đôi: Đánh với Bot Level 3 (Veteran - Biết né đạn, bãi trống)
    4: {"map": False, "items": False, "mode": 0, "steps": 1_500_000, "bot_level": 3, "op_phase": None},
    
    # 5. Tác chiến đô thị: Đánh với Bot Level 4 (Boss - Kiting trong mê cung)
    5: {"map": True,  "items": False, "mode": 0, "steps": 2_000_000, "bot_level": 4, "op_phase": None},
    
    # 6. Self-Play: Đánh với các Model cũ để tránh Rock-Paper-Scissors
    6: {"map": True,  "items": False, "mode": 0, "steps": 2_000_000, "bot_level": None, "op_phase": 5},
    
    # 7. Full Game: Items + Self-Play
    7: {"map": True,  "items": True,  "mode": 0, "steps": 3_000_000, "bot_level": None, "op_phase": 6},
}

import random
import glob

class OpponentPool:
    """
    Opponent Pool cho Self-Play nâng cao.
    Thay vì chỉ đánh với 1 model cố định (frozen), pool lưu nhiều phiên bản cũ
    và chọn ngẫu nhiên đối thủ → tránh bẫy Rock-Paper-Scissors cycling.
    """
    def __init__(self, phase_config):
        self.models = []
        op_phase = phase_config.get("op_phase")
        if op_phase is None:
            return

        # 1. Thêm model phase chính thức (nếu có)
        op_path = f"models/ppo_tank_phase{op_phase}.zip"
        if os.path.exists(op_path):
            self.models.append(PPO.load(op_path))
            print(f"  [Pool] Đã thêm model Phase {op_phase} vào pool")

        # 2. Quét thêm các checkpoint cũ (tạo diversity)
        checkpoint_pattern = f"models/checkpoints/ppo_phase{op_phase}_*.zip"
        checkpoints = sorted(glob.glob(checkpoint_pattern))
        # Chỉ lấy tối đa 5 checkpoint gần nhất để tiết kiệm RAM
        for cp in checkpoints[-5:]:
            try:
                self.models.append(PPO.load(cp))
                print(f"  [Pool] Đã thêm checkpoint: {os.path.basename(cp)}")
            except Exception:
                pass

        if not self.models:
            print(f"  [Cảnh báo] Không tìm thấy model đối thủ Phase {op_phase}. Đối thủ sẽ đứng im!")

    def sample(self):
        """Chọn ngẫu nhiên 1 đối thủ từ pool"""
        if not self.models:
            return None
        return random.choice(self.models)

    def __len__(self):
        return len(self.models)

from bot import RuleBasedBot

def make_env(phase_id, opponent_pool=None, render_mode=None):
    cfg = PHASES[phase_id]
    
    # Ưu tiên dùng Hardcoded Bot nếu cấu hình yêu cầu
    if cfg.get("bot_level") is not None:
        opponent_model = RuleBasedBot(level=cfg["bot_level"])
    else:
        # Nếu không có bot_level, dùng Self-Play từ Opponent Pool
        opponent_model = opponent_pool.sample() if opponent_pool else None

    return AZTankEnv(
        num_players=2, # Cần 2 vì còn có địch để bắn
        map_enabled=cfg["map"],
        items_enabled=cfg["items"],
        training_mode=cfg["mode"],
        opponent_model=opponent_model,
        opponent_pool=opponent_pool,  # Truyền pool để swap đối thủ mỗi episode
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

    # 1. Tải Pool Đối thủ (nếu có) — Self-Play nâng cao
    opponent_pool = OpponentPool(cfg)
    if len(opponent_pool) > 0:
        print(f"  [Pool] Tổng cộng {len(opponent_pool)} đối thủ trong pool")

    # 2. Khởi tạo môi trường
    # Nếu đang bật xem trực tiếp (render) thì phải ép n_envs=1 để khỏi bay nhiều cửa sổ
    num_envs = 1 if render else 4
    render_mode = "human" if render else None
    env = make_vec_env(lambda: make_env(phase_id, opponent_pool, render_mode), n_envs=num_envs)

    # 3. PPO Hyperparameters tùy theo giai đoạn
    #    Phase 1-3: Khám phá nhiều (ent_coef cao, batch lớn hơn)
    #    Phase 4-7: Khai thác kiến thức (ent_coef thấp, batch nhỏ hơn)
    is_early_phase = (phase_id <= 3)
    ppo_params = {
        "n_steps": 4096 if is_early_phase else 2048,
        "batch_size": 128 if is_early_phase else 64,
        "ent_coef": 0.05 if is_early_phase else 0.01,
    }
    print(f"  [PPO] n_steps={ppo_params['n_steps']} | batch_size={ppo_params['batch_size']} | ent_coef={ppo_params['ent_coef']}")

    # 4. Khởi tạo Model AI (Tiếp tục từ phase trước, hoặc resume file)
    # MLP Policy nhỏ thường nhanh hơn trên CPU do không mất công copy data qua VRAM.
    # Tuy nhiên, nếu user muốn ép dùng GPU có thể đổi thành 'auto' hoặc 'cuda'.
    training_device = "cpu" 
    
    if resume_model_path and os.path.exists(resume_model_path + ".zip"):
        print(f"  [Info] Kế thừa trí tuệ từ model: {resume_model_path}.zip")
        model = PPO.load(resume_model_path, env=env, device=training_device)
    elif phase_id > 1 and os.path.exists(f"models/ppo_tank_phase{phase_id - 1}.zip"):
        prev_path = f"models/ppo_tank_phase{phase_id - 1}"
        print(f"  [Info] Kế thừa trí tuệ từ Phase {phase_id - 1}")
        model = PPO.load(prev_path, env=env, device=training_device)
    else:
        print("  [Info] Khởi tạo Model hoàn toàn mới!")
        model = PPO(
            policy="MlpPolicy",
            env=env,
            learning_rate=3e-4,
            n_steps=ppo_params["n_steps"],
            batch_size=ppo_params["batch_size"],
            n_epochs=10,
            gamma=0.99,
            ent_coef=ppo_params["ent_coef"],
            verbose=1,
            device=training_device,
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
        del opponent_pool
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
    opponent_pool = OpponentPool(cfg)
    env = make_env(phase_id, opponent_pool, render_mode="human")
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
    parser.add_argument("--phase", type=int, choices=range(1, 8), help="Chỉ định chạy 1 GĐ cụ thể")
    parser.add_argument("--render", action="store_true", help="Mở cửa sổ Raylib xem (TRAIN RẤT CHẬM)")
    parser.add_argument("--test", type=int, choices=range(1, 8), help="Xem AI múa ở Phase X (sau khi train)")
    parser.add_argument("--resume", action="store_true", help="Tiếp tục học từ file save đang dở")
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
