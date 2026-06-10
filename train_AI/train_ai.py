"""
Script huấn luyện AI xe tăng (Curriculum Learning 10 Giai đoạn)
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
from stable_baselines3.common.vec_env import SubprocVecEnv
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


class GraduationCallback(BaseCallback):
    """
    Performance Gate: Kiểm tra reward trung bình mỗi check_freq bước.
    - Nếu reward >= grad_reward liên tục patience lần → TỐT NGHIỆP SỚM (đỡ kểo dài)
    - Nếu hết max steps mà chưa đạt → vẫn qua nhưng cảnh báo
    """
    def __init__(self, grad_reward, min_steps=100_000, check_freq=50_000, patience=3):
        super().__init__()
        self.grad_reward = grad_reward
        self.min_steps = min_steps      # Tối thiểu phải train bao nhiêu bước
        self.check_freq = check_freq    # Kiểm tra mỗi bao nhiêu bước
        self.patience = patience        # Số lần liên tục đạt ngưỡng mới tốt nghiệp
        self.consecutive_pass = 0
        self.graduated = False
        self.last_mean_reward = None

    def _on_step(self) -> bool:
        if self.num_timesteps < self.min_steps:
            return True  # Chưa đủ steps tối thiểu

        if self.n_calls % self.check_freq == 0:
            # Lấy ep_rew_mean từ SB3 logger
            if len(self.model.ep_info_buffer) > 0:
                mean_reward = sum(ep['r'] for ep in self.model.ep_info_buffer) / len(self.model.ep_info_buffer)
                self.last_mean_reward = mean_reward

                if mean_reward >= self.grad_reward:
                    self.consecutive_pass += 1
                    print(f"  \u2705 [Gate] Reward={mean_reward:.2f} >= {self.grad_reward:.1f} "
                          f"({self.consecutive_pass}/{self.patience} lần liên tục)")
                    if self.consecutive_pass >= self.patience:
                        print(f"  \U0001f393 [Gate] TỐT NGHIỆP SỚM! Reward ổn định tại {mean_reward:.2f}")
                        self.graduated = True
                        return False  # Dừng training sớm
                else:
                    self.consecutive_pass = 0
                    print(f"  \u23f3 [Gate] Reward={mean_reward:.2f} < {self.grad_reward:.1f} "
                          f"(chưa đạt, tiếp tục...)")

        return True

# LỘ TRÌNH 10 GIAI ĐOẠN HUẤN LUYỆN (ANTI-BOUNCE SNIPER v2 — 7 Bot Levels)
# Level 1: Đứng yên | Level 2: Chỉ chạy | Level 3: Bắn thụ động | Level 4: Bắn thẳng chủ động
# Level 5: Nảy 1 lần | Level 6: Nảy 2 lần | Level 7: Full sniper (4 bounces)
PHASES = {
    # CHƯƠNG 1: NỀN TẢNG (Bãi trống)
    # grad_reward: ngưỡng reward trung bình cần đạt để "tốt nghiệp" sớm
    1:  {"map": False, "items": False, "mode": 0, "bot_level": [1], "steps": 300_000,   "grad_reward": 5.0},
    2:  {"map": False, "items": False, "mode": 0, "bot_level": [2], "steps": 500_000,   "grad_reward": 3.0},
    3:  {"map": False, "items": False, "mode": 0, "bot_level": [3], "steps": 800_000,   "grad_reward": 1.0},
    4:  {"map": False, "items": False, "mode": 0, "bot_level": [4], "steps": 1_000_000, "grad_reward": 0.5},

    # CHƯƠNG 2: MÊ CUNG + BOUNCE
    # Phase 5: Bot yếu (chỉ chạy) → AI tập di chuyển mê cung + khám phá nảy tường
    # Phase 6+: Tăng dần bot mạnh hơn
    5:  {"map": True,  "items": False, "mode": 0, "bot_level": [2], "steps": 1_500_000, "grad_reward": 0.5},
    # Trui rèn Phase 6 (Bot bắn thẳng) đến khi cực kỳ thuần thục:
    6:  {"map": True,  "items": False, "mode": 0, "bot_level": [4], "steps": 10_000_000, "grad_reward": 100.0},
    7:  {"map": True,  "items": False, "mode": 0, "bot_level": [6], "steps": 2_500_000, "grad_reward": 0.0},
    8:  {"map": True,  "items": False, "mode": 0, "bot_level": [7], "steps": 3_000_000, "grad_reward": -2.0},

    # CHƯƠNG 3: NÂNG CAO
    9:  {"map": True,  "items": True,  "mode": 0, "bot_level": [7], "steps": 3_000_000, "grad_reward": -2.0},
    10: {"map": True,  "items": True,  "mode": 0, "op_phase": 9,    "steps": 6_000_000, "grad_reward": 0.0},
}

import random
import glob

class OpponentPool:
    """
    Opponent Pool cho Self-Play nâng cao.
    Thay vì chỉ đánh với 1 model cố định (frozen), pool lưu nhiều phiên bản cũ
    và chọn ngẫu nhiên đối thủ → tránh bẫy Rock-Paper-Scissors cycling.
    """
    def __init__(self, phase_config, current_phase=None):
        self.models = []
        self.phase_config = phase_config
        self.current_phase = current_phase
        self.refresh()

    def refresh(self):
        """Quét ổ đĩa để tải các model đối thủ mới nhất"""
        op_phase = self.phase_config.get("op_phase")
        if op_phase is None:
            return

        # Logic tải đối thủ:
        # Phase 9 (Asymmetric): Đấu với Phase 8 (frozen)
        # Phase 10 (Symmetric): Đấu với chính mình (Phase 10 checkpoints)
        phases_to_load = [op_phase]

        new_models = []
        for p in phases_to_load:
            # 1. Thêm model phase chính thức
            op_path = f"models/ppo_tank_phase{p}.zip"
            if os.path.exists(op_path):
                try:
                    new_models.append(PPO.load(op_path))
                    print(f"  [Pool] Đã nạp model chính Phase {p}")
                except Exception: pass

            # 2. Quét thêm các checkpoint (chỉ lấy 3 cái mới nhất mỗi phase để tránh tràn RAM)
            checkpoint_pattern = f"models/checkpoints/ppo_phase{p}_*.zip"
            checkpoints = sorted(glob.glob(checkpoint_pattern))
            for cp in checkpoints[-3:]:
                try:
                    new_models.append(PPO.load(cp))
                    print(f"  [Pool] Đã nạp checkpoint: {os.path.basename(cp)}")
                except Exception: pass
        
        if new_models:
            self.models = new_models
            print(f"  [Pool] Hiện có tổng cộng {len(self.models)} đối thủ.")
        elif not self.models:
            print(f"  [Cảnh báo] Pool trống rỗng! Đối thủ sẽ đứng im.")

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

    # Bot miễn nhiễm đạn tự bắn từ Phase 5 trở đi (mê cung + bounce)
    use_bot_immune = (phase_id >= 5)

    return AZTankEnv(
        num_players=2, # Cần 2 vì còn có địch để bắn
        map_enabled=cfg["map"],
        items_enabled=cfg["items"],
        training_mode=cfg["mode"],
        bot_self_immune=use_bot_immune,
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
    opponent_pool = OpponentPool(cfg, current_phase=phase_id)

    # 2. Khởi tạo môi trường
    # SubprocVecEnv: mỗi env chạy trong process riêng → tận dụng đa nhân CPU
    # i5-13500 (20 threads): 8 envs × 3 threads/env = tối ưu ~70% CPU
    num_envs = 1 if render else 8
    render_mode = "human" if render else None
    env = make_vec_env(lambda: make_env(phase_id, opponent_pool, render_mode),
                       n_envs=num_envs,
                       vec_env_cls=SubprocVecEnv if num_envs > 1 else None)

    # 3. Callback đặc biệt cho Self-Play: Cập nhật pool mỗi khi có checkpoint mới
    class SelfPlayCallback(BaseCallback):
        def __init__(self, pool, refresh_freq=100_000):
            super().__init__()
            self.pool = pool
            self.refresh_freq = refresh_freq
        def _on_step(self) -> bool:
            if self.n_calls % self.refresh_freq == 0:
                print("\n  [Self-Play] Đang làm mới Pool đối thủ từ các checkpoint mới nhất...")
                self.pool.refresh()
            return True
    
    self_play_cb = SelfPlayCallback(opponent_pool)

    # 3. PPO Hyperparameters tùy theo giai đoạn
    #    Phase 1-4: Khám phá nhiều (ent_coef cao, batch lớn hơn)
    #    Phase 5-10: Khai thác kiến thức (ent_coef thấp, batch nhỏ hơn)
    is_early_phase = (phase_id <= 4)
    ppo_params = {
        "n_steps": 4096 if is_early_phase else 2048,
        "batch_size": 128 if is_early_phase else 256,
        "ent_coef": 0.05 if is_early_phase else 0.01,
    }
    print(f"  [PPO] n_steps={ppo_params['n_steps']} | batch_size={ppo_params['batch_size']} | ent_coef={ppo_params['ent_coef']}")

    # 4. Khởi tạo Model AI (Tiếp tục từ phase trước, hoặc resume file)
    training_device = "cpu" 
     
    if resume_model_path and os.path.exists(resume_model_path + ".zip"):
        print(f"  [Info] Kế thừa trí tuệ từ model: {resume_model_path}.zip")
        model = PPO.load(resume_model_path, env=env, device=training_device,
                         custom_objects={
                             "n_steps": ppo_params["n_steps"],
                             "batch_size": ppo_params["batch_size"],
                             "ent_coef": ppo_params["ent_coef"],
                         })
    elif phase_id > 1 and os.path.exists(f"models/ppo_tank_phase{phase_id - 1}.zip"):
        prev_path = f"models/ppo_tank_phase{phase_id - 1}"
        print(f"  [Info] Kế thừa trí tuệ từ Phase {phase_id - 1}")
        model = PPO.load(prev_path, env=env, device=training_device,
                         custom_objects={
                             "n_steps": ppo_params["n_steps"],
                             "batch_size": ppo_params["batch_size"],
                             "ent_coef": ppo_params["ent_coef"],
                         })
    else:
        print("  [Info] Khởi tạo Model hoàn toàn mới!")
        model = PPO(
            policy="MlpPolicy",
            env=env,
            learning_rate=1e-4,
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

    # Graduation callback — kiểm tra reward để tốt nghiệp sớm
    grad_reward = cfg.get("grad_reward", 0.0)
    base_steps = cfg["steps"]
    max_steps = base_steps * 3  # Tối đa 3x steps nếu chưa đạt ngưỡng

    grad_cb = GraduationCallback(
        grad_reward=grad_reward,
        min_steps=max(100_000, base_steps // 5),
        check_freq=50_000,
        patience=3
    )

    try:
        # VÒNG LẶP TRAIN: tiếp tục cho đến khi đạt ngưỡng hoặc hết giới hạn
        total_trained = 0
        attempt = 1
        while total_trained < max_steps:
            remaining = min(base_steps, max_steps - total_trained)
            
            if attempt > 1:
                print(f"\n  🔄 [Retry #{attempt}] Reward chưa đạt! Thêm {remaining:,} steps (đã train: {total_trained:,}/{max_steps:,})...")
                # Reset graduation counter cho lần thử mới
                grad_cb.consecutive_pass = 0
                grad_cb.graduated = False

            model.learn(total_timesteps=remaining,
                        callback=[checkpoint_cb, progress_cb, self_play_cb, grad_cb],
                        reset_num_timesteps=False)
            total_trained += remaining
            model.save(model_path)

            # Kiểm tra đã tốt nghiệp chưa
            if grad_cb.graduated:
                print(f"\n  🎓 [Hoàn Thành] Phase {phase_id} TỐT NGHIỆP! "
                      f"(reward={grad_cb.last_mean_reward:.2f} >= {grad_reward}, "
                      f"sau {total_trained:,} steps)")
                break
            elif grad_cb.last_mean_reward is not None and grad_cb.last_mean_reward >= grad_reward:
                print(f"\n  ✅ [Hoàn Thành] Phase {phase_id} ĐẠT CHUẨN "
                      f"(reward={grad_cb.last_mean_reward:.2f}, sau {total_trained:,} steps)")
                break
            else:
                # Chưa đạt → thử thêm nếu còn quota
                if total_trained >= max_steps:
                    rew_str = f"{grad_cb.last_mean_reward:.2f}" if grad_cb.last_mean_reward is not None else "N/A"
                    print(f"\n  ⚠️ [Cảnh báo] Phase {phase_id} đã train {total_trained:,} steps (3x) "
                          f"nhưng reward={rew_str} < {grad_reward}")
                    print(f"     Chuyển sang phase tiếp theo — có thể cần điều chỉnh reward function.")
                    break
                attempt += 1
        
        print(f"  Lưu tại: {model_path}.zip\n")
        
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

def pipeline(start_phase=1, resume_start=False, render=False):
    print(f"\n🚀 Bắt đầu Curriculum Learning từ Phase {start_phase} 🚀\n")
    for phase_id in sorted(PHASES.keys()):
        if phase_id < start_phase:
            continue
            
        expected_path = f"models/ppo_tank_phase{phase_id}.zip"
        
        # Nếu là phase đầu tiên và người dùng muốn resume, ta luôn nạp model cũ
        if phase_id == start_phase and resume_start:
             print(f"  🔄 Đang nạp lại model Phase {phase_id} để học tiếp...")
             train_phase(phase_id, resume_model_path=f"models/ppo_tank_phase{phase_id}", render=render)
        # Nếu đã có model phase này rồi thì bỏ qua để tiết kiệm thời gian
        elif os.path.exists(expected_path):
             print(f"  ⏩ Đã có model Phase {phase_id}, tự bỏ qua...")
             continue
        else:
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
    parser.add_argument("--pipeline", action="store_true", help="Chạy tự động từ GĐ 1 đến 10")
    parser.add_argument("--phase", type=int, choices=range(1, 11), help="Chỉ định chạy 1 GĐ cụ thể")
    parser.add_argument("--render", action="store_true", help="Mở cửa sổ Raylib xem (TRAIN RẤT CHẬM)")
    parser.add_argument("--test", type=int, choices=range(1, 11), help="Xem AI múa ở Phase X (sau khi train)")
    parser.add_argument("--resume", action="store_true", help="Tiếp tục học từ file save đang dở")
    args = parser.parse_args()

    if args.test:
        test_model(args.test)
    elif args.pipeline:
        # Nếu có truyền --phase X khi đang chạy --pipeline, nó sẽ bắt đầu từ phase X
        start_ph = args.phase if args.phase else 1
        pipeline(start_phase=start_ph, resume_start=args.resume, render=args.render)
    elif args.phase:
        resume_path = f"models/ppo_tank_phase{args.phase}" if args.resume else None
        train_phase(args.phase, resume_model_path=resume_path, render=args.render)
    else:
        print("Vui lòng chọn cách chạy:")
        print("  python train_ai.py --pipeline             (Chạy từ đầu đến cuối)")
        print("  python train_ai.py --pipeline --phase 9   (Bắt đầu từ Phase 9, bỏ qua nếu đã có file)")
        print("  python train_ai.py --pipeline --phase 9 --resume  (Học TIẾP Phase 9 rồi tự động sang các Phase sau)")
        print("  python train_ai.py --phase 1 --resume     (Chỉ học tiếp Phase 1 rồi dừng)")
        print("  python train_ai.py --test 7               (Xem AI diễn hài SAU KHI train xong)")
