import sys
import os
import numpy as np
import gymnasium as gym
from gymnasium import spaces

# -------------------------------------------------------
# Thêm thư mục build vào đường dẫn để Python tìm thấy file .pyd
# Lưu ý: file này trong /train_AI/ nên phải lùi ra 1 cấp ('..') để thấy /build/
build_dir = os.path.join(os.path.dirname(__file__), '..', 'build')
if os.path.exists(os.path.join(build_dir, 'Debug')):
    sys.path.append(os.path.join(build_dir, 'Debug'))
elif os.path.exists(os.path.join(build_dir, 'Release')):
    sys.path.append(os.path.join(build_dir, 'Release'))
else:
    sys.path.append(build_dir)

# Thêm thư mục MSYS2 chứa các DLL (libstdc++, libgcc) vào đường dẫn để có thể load file .pyd
msys2_bin = "C:/msys64/ucrt64/bin"
if hasattr(os, "add_dll_directory") and os.path.exists(msys2_bin):
    os.add_dll_directory(msys2_bin)

import azgame_env


class AZTankEnv(gym.Env):
    """
    Bọc (wrap) RLEnv của C++ theo chuẩn Gymnasium để
    tương thích với Stable Baselines3.

    State (13 floats):
        [0 -> 7]: Khoảng cách radar đến tường (8 hướng) (0-1)
        [8]: Cos góc tới địch (-1 đến 1)
        [9]: Sin góc tới địch (-1 đến 1)
        [10]: Khoảng cách đường chim bay đến địch (0-1)
        [11]: Số lượng đạn đang bay (tỷ lệ 0-1)
        [12]: Máu hiện tại (0-1)

    Action (5 hành động rời rạc):
        0: Tiến
        1: Lùi
        2: Quay trái
        3: Quay phải
        4: Bắn
    """

    metadata = {"render_modes": ["human"]}

    def __init__(self, num_players=2, map_enabled=False, items_enabled=False, training_mode=0, opponent_model=None, render_mode=None):
        super().__init__()

        self.opponent_model = opponent_model
        self.render_mode = render_mode

        # Khởi tạo môi trường game C++
        self._env = azgame_env.RLEnv(
            num_players=num_players,
            map_enabled=map_enabled,
            items_enabled=items_enabled,
            training_mode=training_mode
        )

        # Định nghĩa không gian hành động: 5 hành động rời rạc (0 → 4)
        self.action_space = spaces.Discrete(5)

        # Định nghĩa không gian quan sát: 13 con số thực trong khoảng [-1, 1]
        self.observation_space = spaces.Box(
            low=-1.0,
            high=1.0,
            shape=(13,),
            dtype=np.float32
        )

    def reset(self, seed=None, options=None):
        """Bắt đầu ván chơi mới, trả về trạng thái ban đầu."""
        super().reset(seed=seed)
        state = self._env.reset()
        obs = np.array(state, dtype=np.float32)
        info = {}
        return obs, info

    def step(self, action):
        """
        Thực hiện 1 hành động, trả về kết quả theo chuẩn Gymnasium.
        Returns: (observation, reward, terminated, truncated, info)
        """
        action_p1 = -1
        if self.opponent_model is not None:
             # Thu thập State của Enemy (Player 1)
             state_p1 = self._env.get_state(1)
             obs_p1 = np.array(state_p1, dtype=np.float32)
             # Cho model PPO cũ dự đoán đòn đánh
             opp_action, _ = self.opponent_model.predict(obs_p1, deterministic=True)
             action_p1 = int(opp_action)

        state, reward, done = self._env.step(int(action), action_p1)
        obs = np.array(state, dtype=np.float32)

        info = {}
        if self.render_mode == "human":
             is_open = self._env.render()
             info["window_open"] = is_open

        # Gymnasium phân biệt 2 loại kết thúc:
        # terminated = kết thúc tự nhiên (bị chết, bắn hạ hết địch)
        # truncated  = hết thời gian (maxSteps)
        terminated = done
        truncated = False

        return obs, float(reward), terminated, truncated, info

    def render(self):
        if self.render_mode == "human":
             return self._env.render()
        return True

    def close(self):
        pass
