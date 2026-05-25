import os
import sys
import numpy as np
import ctypes
from stable_baselines3 import PPO

# Nhập các thành phần cần thiết từ mã huấn luyện
from train_ai import PHASES
from gymnasium_wrapper import AZTankEnv

class HumanKeyboardBot:
    """
    Một "Bot" giả lập do con người điều khiển sử dụng ctypes (không cần cài thêm thư viện).
    Nó sẽ đọc phím bấm từ bàn phím và chuyển thành hành động cho Player 1.
    """
    def __init__(self):
        # Tạo không gian quan sát giả để không bị lỗi khi Gymnasium kiểm tra
        self.observation_space = type('MockSpace', (), {'shape': (45,)})()
        
    def is_key_pressed(self, vk_code):
        # ctypes.windll.user32.GetAsyncKeyState trả về giá trị khác 0 nếu phím đang được giữ
        return (ctypes.windll.user32.GetAsyncKeyState(vk_code) & 0x8000) != 0

    def get_action_from_env(self, env_instance):
        """
        Đọc phím bấm và trả về action dạng [Move, Turn, Shoot]
        """
        # Virtual Key Codes
        VK_W = 0x57
        VK_S = 0x53
        VK_A = 0x41
        VK_D = 0x44
        VK_UP = 0x26
        VK_DOWN = 0x28
        VK_LEFT = 0x25
        VK_RIGHT = 0x27
        VK_SPACE = 0x20
        VK_RETURN = 0x0D

        # 1. Move (0: Đứng im, 1: Tiến, 2: Lùi)
        move = 0
        if self.is_key_pressed(VK_W) or self.is_key_pressed(VK_UP):
            move = 1
        elif self.is_key_pressed(VK_S) or self.is_key_pressed(VK_DOWN):
            move = 2
            
        # 2. Turn (0: Đứng im, 1: Trái, 2: Phải)
        turn = 0
        if self.is_key_pressed(VK_A) or self.is_key_pressed(VK_LEFT):
            turn = 1
        elif self.is_key_pressed(VK_D) or self.is_key_pressed(VK_RIGHT):
            turn = 2
            
        # 3. Shoot (0: Không bắn, 1: Bắn)
        shoot = 0
        if self.is_key_pressed(VK_SPACE) or self.is_key_pressed(VK_RETURN):
            shoot = 1
            
        return [move, turn, shoot]

def play_vs_ai(phase_id):
    model_path = f"models/ppo_tank_phase{phase_id}.zip"
    if not os.path.exists(model_path):
        print(f"[Lỗi] Không tìm thấy model tại {model_path}!")
        return
        
    print(f"\n🎮 Đang tải AI Phase {phase_id} từ {model_path}...")
    
    # Lấy cấu hình của phase
    cfg = PHASES.get(phase_id, {"map": True, "items": True, "mode": 0})
    
    # Tạo bot do con người điều khiển
    human_bot = HumanKeyboardBot()
    
    # Khởi tạo môi trường
    env = AZTankEnv(
        num_players=2,
        map_enabled=cfg["map"],
        items_enabled=cfg["items"],
        training_mode=cfg["mode"],
        opponent_model=human_bot, # Người chơi thay thế vị trí của opponent
        opponent_pool=None,       # Không dùng opponent pool nữa
        render_mode="human"       # Bật màn hình đồ họa Raylib
    )
    
    # Tải AI model
    model = PPO.load(model_path)
    
    obs, _ = env.reset()
    
    print("=" * 60)
    print("                BẠN ĐANG CHƠI VỚI AI")
    print("=" * 60)
    print(" 🕹️ Cách điều khiển (Player 1 - Xe tăng Địch):")
    print("   - W, A, S, D hoặc Mũi tên: Di chuyển")
    print("   - SPACE hoặc ENTER: Bắn")
    print("   - ESC: Thoát game")
    print("=" * 60)
    
    VK_ESCAPE = 0x1B

    try:
        while True:
            # Nếu người dùng nhấn ESC thì thoát
            if (ctypes.windll.user32.GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0:
                print("\n[INFO] Đã thoát game.")
                break
                
            # AI (Player 0) dự đoán hành động
            action, _ = model.predict(obs, deterministic=True)
            
            # Step môi trường (lúc này env sẽ tự gọi human_bot.get_action_from_env để lấy phím bấm của bạn)
            obs, reward, done, trunc, info = env.step(action)
            
            # Dừng nếu cửa sổ bị đóng
            if not info.get("window_open", True):
                print("\n[INFO] Đã đóng cửa sổ game.")
                break
                
            # Reset nếu ván chơi kết thúc
            if done or trunc:
                obs, _ = env.reset()
    except KeyboardInterrupt:
        print("\n[INFO] Dừng chương trình.")
    finally:
        env.close()

if __name__ == "__main__":
    import argparse
    parser = argparse.ArgumentParser()
    parser.add_argument("--phase", type=int, default=8, help="Phase của model AI muốn chơi cùng (mặc định: 8)")
    args = parser.parse_args()
    
    play_vs_ai(args.phase)
