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

    State (9 floats):
        [0] x xe mình      (0-1)
        [1] y xe mình      (0-1)
        [2] cos góc mình   (-1, 1)
        [3] sin góc mình   (-1, 1)
        [4] đạn mình       (0-1)
        [5] x xe địch      (0-1)
        [6] y xe địch      (0-1)
        [7] cos góc địch   (-1, 1)
        [8] sin góc địch   (-1, 1)

    Action (6 hành động rời rạc):
        0: Đứng im
        1: Tiến
        2: Lùi
        3: Quay trái
        4: Quay phải
        5: Bắn
    """

    metadata = {"render_modes": []}

    def __init__(self, num_players=2, map_enabled=False, items_enabled=False):
        super().__init__()

        # Khởi tạo môi trường game C++
        self._env = azgame_env.RLEnv(
            num_players=num_players,
            map_enabled=map_enabled,
            items_enabled=items_enabled
        )

        # Định nghĩa không gian hành động: 6 hành động rời rạc (0 → 5)
        self.action_space = spaces.Discrete(6)

        # Định nghĩa không gian quan sát: 9 con số thực trong khoảng [-1, 1]
        self.observation_space = spaces.Box(
            low=-1.0,
            high=1.0,
            shape=(9,),
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
        state, reward, done = self._env.step(int(action))
        obs = np.array(state, dtype=np.float32)

        # Gymnasium phân biệt 2 loại kết thúc:
        # terminated = kết thúc tự nhiên (bị chết, bắn hạ hết địch)
        # truncated  = hết thời gian (maxSteps)
        terminated = done
        truncated = False
        info = {}

        return obs, float(reward), terminated, truncated, info

    def close(self):
        pass
