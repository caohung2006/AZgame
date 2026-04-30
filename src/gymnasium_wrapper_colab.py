"""
Gymnasium Wrapper cho Google Colab.
Thay thế gymnasium_wrapper.py (dùng module C++) bằng pure_python_env.
API hoàn toàn giống bản gốc → train_ai.py không cần sửa nhiều.
"""
import numpy as np
import gymnasium as gym
from gymnasium import spaces
from pure_python_env import RLEnv


class AZTankEnv(gym.Env):
    """
    Bọc (wrap) RLEnv Python theo chuẩn Gymnasium.
    Tương thích 100% với Stable Baselines3.

    State (25 hoặc 27 floats tùy use_astar):
        [0 -> 7] : Khoảng cách radar đến tường (8 hướng) (0-1)
        [8]      : Cos góc tới địch (-1 đến 1)
        [9]      : Sin góc tới địch (-1 đến 1)
        [10]     : Khoảng cách đường chim bay đến địch (0-1)
        [11]     : Số lượng đạn đang bay của bản thân (tỷ lệ 0-1)
        [12]     : Máu hiện tại (0-1)
        [13->24] : Tọa độ tương đối (dx, dy) và Vận tốc (vx, vy) của 3 viên đạn gần nhất
        [25]     : A* waypoint dx (chỉ khi use_astar=True, tức map_enabled=True)
        [26]     : A* waypoint dy (chỉ khi use_astar=True, tức map_enabled=True)

    Action (13 hành động rời rạc):
        0-11: Di chuyển theo hướng góc `action * pi / 6`
        12: Bắn đạn
    """

    metadata = {"render_modes": ["human"]}

    def __init__(self, num_players=2, map_enabled=False, items_enabled=False,
                 training_mode=0, opponent_model=None, render_mode=None):
        super().__init__()

        self.opponent_model = opponent_model
        self.render_mode = render_mode

        self._env = RLEnv(
            num_players=num_players,
            map_enabled=map_enabled,
            items_enabled=items_enabled,
            training_mode=training_mode
            # use_astar tự động = map_enabled (xem RLEnv.__init__)
        )

        self.action_space = spaces.Discrete(13)
        self.observation_space = spaces.Box(
            low=-1.0, high=1.0,
            shape=(self._env.obs_size,),   # 25 khi no-map, 27 khi có map
            dtype=np.float32
        )

    def reset(self, seed=None, options=None):
        super().reset(seed=seed)
        state = self._env.reset()
        obs = np.array(state, dtype=np.float32)
        return obs, {}

    def step(self, action):
        action_p1 = -1
        if self.opponent_model is not None:
            state_p1 = self._env.get_state(1)
            obs_p1 = np.array(state_p1, dtype=np.float32)
            opp_action, _ = self.opponent_model.predict(obs_p1, deterministic=True)
            action_p1 = int(opp_action)

        state, reward, done = self._env.step(int(action), action_p1)
        obs = np.array(state, dtype=np.float32)
        return obs, float(reward), done, False, {}

    def render(self):
        return True

    def close(self):
        pass
