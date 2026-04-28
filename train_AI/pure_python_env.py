"""
Pure Python Environment cho AZTank Game.
Thay thế hoàn toàn module C++ (azgame_env) để chạy được trên Google Colab.
Tái tạo trung thành logic game từ C++: vật lý xe tăng, đạn nảy tường,
radar 8 hướng, mê cung Recursive Backtracker, reward shaping.
"""
import math
import random
import numpy as np

# ============================================================
# HẰNG SỐ (từ Constants.h)
# ============================================================
SCALE = 30.0
SCREEN_WIDTH = 1024
SCREEN_HEIGHT = 768
PI = math.pi
DT = 1.0 / 60.0  # Fixed timestep

# Maze config (từ map.cpp)
MAZE_ROWS = 6
MAZE_COLS = 8
CELL_W = 90.0
CELL_H = 90.0
WALL_THICKNESS = 6.0
OFFSET_X = (SCREEN_WIDTH - (MAZE_COLS * CELL_W)) / 2.0
OFFSET_Y = (SCREEN_HEIGHT - (MAZE_ROWS * CELL_H)) / 2.0 - 50.0

# Tank physics (từ tank.cpp)
MOVE_SPEED = 3.0  # Box2D m/s
TURN_SPEED = 3.0  # rad/s
TANK_HALF_W = 14.0 / SCALE
TANK_HALF_H = 14.0 / SCALE
SHOOT_COOLDOWN = 0.15
MAX_BULLETS_PER_PLAYER = 5
BULLET_SPEED = 6.0
BULLET_LIFETIME = 7.0
BULLET_RADIUS = 3.0 / SCALE
BARREL_LENGTH = 30.0 / SCALE

# World bounds (Box2D coords)
WORLD_W = SCREEN_WIDTH / SCALE
WORLD_H = SCREEN_HEIGHT / SCALE


# ============================================================
# VẬT THỂ GAME
# ============================================================
class Vec2:
    """Vector 2D đơn giản thay thế b2Vec2."""
    __slots__ = ('x', 'y')
    def __init__(self, x=0.0, y=0.0):
        self.x = float(x)
        self.y = float(y)
    def length(self):
        return math.sqrt(self.x*self.x + self.y*self.y)
    def length_sq(self):
        return self.x*self.x + self.y*self.y
    def __sub__(self, o):
        return Vec2(self.x - o.x, self.y - o.y)
    def __add__(self, o):
        return Vec2(self.x + o.x, self.y + o.y)
    def __mul__(self, s):
        return Vec2(self.x * s, self.y * s)
    def __rmul__(self, s):
        return Vec2(self.x * s, self.y * s)
    def normalize(self):
        l = self.length()
        if l > 1e-6:
            self.x /= l
            self.y /= l
        return self


class AABB:
    """Axis-Aligned Bounding Box cho va chạm."""
    __slots__ = ('cx', 'cy', 'hw', 'hh')
    def __init__(self, cx, cy, hw, hh):
        self.cx = cx; self.cy = cy; self.hw = hw; self.hh = hh

    def overlaps(self, other):
        return (abs(self.cx - other.cx) < self.hw + other.hw and
                abs(self.cy - other.cy) < self.hh + other.hh)


class Wall:
    """Tường tĩnh (thay thế b2_staticBody)."""
    __slots__ = ('aabb',)
    def __init__(self, cx, cy, hw, hh):
        self.aabb = AABB(cx, cy, hw, hh)


class SimpleBullet:
    """Đạn đơn giản (chỉ đạn thường, không special weapons cho training cơ bản)."""
    __slots__ = ('pos', 'vel', 'time', 'owner', 'radius')
    def __init__(self, pos, vel, owner):
        self.pos = Vec2(pos.x, pos.y)
        self.vel = Vec2(vel.x, vel.y)
        self.time = BULLET_LIFETIME
        self.owner = owner
        self.radius = BULLET_RADIUS

    def update(self, dt, walls):
        """Di chuyển đạn + nảy tường (restitution = 1.0)."""
        self.time -= dt
        if self.time <= 0:
            return

        new_x = self.pos.x + self.vel.x * dt
        new_y = self.pos.y + self.vel.y * dt

        # Nảy tường vật lý (walls)
        for w in walls:
            a = w.aabb
            if (abs(new_x - a.cx) < self.radius + a.hw and
                abs(new_y - a.cy) < self.radius + a.hh):
                # Xác định hướng va chạm
                dx = new_x - a.cx
                dy = new_y - a.cy
                overlap_x = self.radius + a.hw - abs(dx)
                overlap_y = self.radius + a.hh - abs(dy)
                if overlap_x < overlap_y:
                    self.vel.x = -self.vel.x
                    new_x = a.cx + (a.hw + self.radius) * (1 if dx > 0 else -1)
                else:
                    self.vel.y = -self.vel.y
                    new_y = a.cy + (a.hh + self.radius) * (1 if dy > 0 else -1)

        # Nảy viền màn hình
        if new_x < self.radius:
            self.vel.x = abs(self.vel.x); new_x = self.radius
        elif new_x > WORLD_W - self.radius:
            self.vel.x = -abs(self.vel.x); new_x = WORLD_W - self.radius
        if new_y < self.radius:
            self.vel.y = abs(self.vel.y); new_y = self.radius
        elif new_y > WORLD_H - self.radius:
            self.vel.y = -abs(self.vel.y); new_y = WORLD_H - self.radius

        self.pos.x = new_x
        self.pos.y = new_y

    def is_dead(self):
        return self.time <= 0

    def get_aabb(self):
        return AABB(self.pos.x, self.pos.y, self.radius, self.radius)


class SimpleTank:
    """Xe tăng đơn giản (thay thế Tank + b2Body)."""
    __slots__ = ('pos', 'angle', 'player_index', 'is_destroyed',
                 'shoot_cooldown', 'hw', 'hh')

    def __init__(self, player_index):
        self.pos = Vec2(WORLD_W / 2, WORLD_H / 2)
        self.angle = 0.0
        self.player_index = player_index
        self.is_destroyed = False
        self.shoot_cooldown = 0.0
        self.hw = TANK_HALF_W
        self.hh = TANK_HALF_H + 7.0 / SCALE  # hull + barrel

    def forward_dir(self):
        """Hướng mũi xe (-sin, cos) giống C++."""
        return Vec2(-math.sin(self.angle), math.cos(self.angle))

    def get_aabb(self):
        # Dùng bounding box xoay gần đúng (hơi lớn hơn thực tế)
        r = max(self.hw, self.hh) * 1.2
        return AABB(self.pos.x, self.pos.y, r, r)

    def _overlaps_any_wall(self, walls):
        """Kiểm tra xe có chồng lên bất kỳ tường nào không (chỉ check, không đẩy)."""
        r = max(self.hw, self.hh) * 1.2
        for w in walls:
            if (abs(self.pos.x - w.aabb.cx) < r + w.aabb.hw and
                abs(self.pos.y - w.aabb.cy) < r + w.aabb.hh):
                return True
        return False

    def handle_movement(self, action, dt, walls):
        """Di chuyển theo action (0-4) với va chạm tường axis-separated.
        Trả về True nếu bị chặn bởi tường (dùng cho reward)."""
        # Xoay
        angular_vel = 0.0
        if action == 2:  # Turn left
            angular_vel = TURN_SPEED
        elif action == 3:  # Turn right
            angular_vel = -TURN_SPEED
        self.angle += angular_vel * dt

        # Tính vận tốc
        fwd = self.forward_dir()
        vx, vy = 0.0, 0.0
        if action == 0:  # Forward
            vx, vy = fwd.x * MOVE_SPEED * dt, fwd.y * MOVE_SPEED * dt
        elif action == 1:  # Backward
            vx, vy = -fwd.x * MOVE_SPEED * dt, -fwd.y * MOVE_SPEED * dt

        blocked = False

        # Di chuyển trục X, revert nếu va chạm
        self.pos.x += vx
        if walls and self._overlaps_any_wall(walls):
            self.pos.x -= vx
            blocked = True

        # Di chuyển trục Y, revert nếu va chạm
        self.pos.y += vy
        if walls and self._overlaps_any_wall(walls):
            self.pos.y -= vy
            blocked = True

        # Giữ trong world bounds
        r = max(self.hw, self.hh)
        self.pos.x = max(r, min(WORLD_W - r, self.pos.x))
        self.pos.y = max(r, min(WORLD_H - r, self.pos.y))

        return blocked


# ============================================================
# MAZE GENERATOR (từ map.cpp)
# ============================================================
def build_maze():
    """Sinh mê cung bằng Recursive Backtracker + shortcut. Trả về list[Wall]."""
    h_walls = [[True]*MAZE_COLS for _ in range(MAZE_ROWS+1)]
    v_walls = [[True]*(MAZE_COLS+1) for _ in range(MAZE_ROWS)]
    visited = [[False]*MAZE_COLS for _ in range(MAZE_ROWS)]

    stack = []
    sr, sc = random.randint(0, MAZE_ROWS-1), random.randint(0, MAZE_COLS-1)
    visited[sr][sc] = True
    stack.append((sr, sc))

    while stack:
        r, c = stack[-1]
        neighbors = []
        if r > 0 and not visited[r-1][c]: neighbors.append(0)
        if c < MAZE_COLS-1 and not visited[r][c+1]: neighbors.append(1)
        if r < MAZE_ROWS-1 and not visited[r+1][c]: neighbors.append(2)
        if c > 0 and not visited[r][c-1]: neighbors.append(3)

        if neighbors:
            d = random.choice(neighbors)
            nr, nc = r, c
            if d == 0: nr = r-1; h_walls[r][c] = False
            elif d == 1: nc = c+1; v_walls[r][c+1] = False
            elif d == 2: nr = r+1; h_walls[r+1][c] = False
            elif d == 3: nc = c-1; v_walls[r][c] = False
            visited[nr][nc] = True
            stack.append((nr, nc))
        else:
            stack.pop()

    # Shortcut
    extra = 6 + random.randint(0, 3)
    while extra > 0:
        if random.randint(0, 1) == 0:
            r = 1 + random.randint(0, MAZE_ROWS-2)
            c = random.randint(0, MAZE_COLS-1)
            if h_walls[r][c]: h_walls[r][c] = False; extra -= 1
        else:
            r = random.randint(0, MAZE_ROWS-1)
            c = 1 + random.randint(0, MAZE_COLS-2)
            if v_walls[r][c]: v_walls[r][c] = False; extra -= 1

    # Tạo Wall objects (Box2D coords)
    walls = []
    wt2 = (WALL_THICKNESS / 2.0) / SCALE
    cw2 = (CELL_W / 2.0) / SCALE
    ch2 = (CELL_H / 2.0) / SCALE
    cwf = (CELL_W + WALL_THICKNESS) / 2.0 / SCALE
    chf = (CELL_H + WALL_THICKNESS) / 2.0 / SCALE

    for r in range(MAZE_ROWS + 1):
        for c in range(MAZE_COLS):
            if h_walls[r][c]:
                px = (OFFSET_X + c * CELL_W + CELL_W / 2.0) / SCALE
                py = (SCREEN_HEIGHT - (OFFSET_Y + r * CELL_H)) / SCALE
                walls.append(Wall(px, py, cwf, wt2))

    for r in range(MAZE_ROWS):
        for c in range(MAZE_COLS + 1):
            if v_walls[r][c]:
                px = (OFFSET_X + c * CELL_W) / SCALE
                py = (SCREEN_HEIGHT - (OFFSET_Y + r * CELL_H + CELL_H / 2.0)) / SCALE
                walls.append(Wall(px, py, wt2, chf))

    return walls


def get_random_cell_center():
    """Trả về tọa độ Box2D tâm ô ngẫu nhiên (từ map.cpp)."""
    row = random.randint(0, MAZE_ROWS - 1)
    col = random.randint(0, MAZE_COLS - 1)
    x = OFFSET_X + col * CELL_W + CELL_W / 2.0
    y = OFFSET_Y + row * CELL_H + CELL_H / 2.0
    return Vec2(x / SCALE, (SCREEN_HEIGHT - y) / SCALE)


# ============================================================
# RAYCAST ĐƠN GIẢN (thay thế b2RayCast cho Radar)
# ============================================================
def raycast_walls(origin, direction, max_dist, walls):
    """Tìm khoảng cách gần nhất tới tường theo hướng direction. Trả về fraction [0,1]."""
    closest = 1.0
    end = origin + direction * max_dist

    for w in walls:
        a = w.aabb
        # AABB ray intersection
        t = _ray_aabb(origin, end, a)
        if t is not None and t < closest:
            closest = t

    # Viền màn hình (implicit walls)
    for bx, by, bw, bh in [
        (WORLD_W/2, 0, WORLD_W/2, 0.01),        # bottom
        (WORLD_W/2, WORLD_H, WORLD_W/2, 0.01),   # top
        (0, WORLD_H/2, 0.01, WORLD_H/2),          # left
        (WORLD_W, WORLD_H/2, 0.01, WORLD_H/2),   # right
    ]:
        t = _ray_aabb(origin, end, AABB(bx, by, bw, bh))
        if t is not None and t < closest:
            closest = t

    return closest


def _ray_aabb(p1, p2, aabb):
    """Ray-AABB intersection, trả về fraction hoặc None."""
    dx = p2.x - p1.x
    dy = p2.y - p1.y

    tmin = 0.0
    tmax = 1.0

    if abs(dx) > 1e-8:
        tx1 = (aabb.cx - aabb.hw - p1.x) / dx
        tx2 = (aabb.cx + aabb.hw - p1.x) / dx
        tmin = max(tmin, min(tx1, tx2))
        tmax = min(tmax, max(tx1, tx2))
    elif abs(p1.x - aabb.cx) > aabb.hw:
        return None

    if abs(dy) > 1e-8:
        ty1 = (aabb.cy - aabb.hh - p1.y) / dy
        ty2 = (aabb.cy + aabb.hh - p1.y) / dy
        tmin = max(tmin, min(ty1, ty2))
        tmax = min(tmax, max(ty1, ty2))
    elif abs(p1.y - aabb.cy) > aabb.hh:
        return None

    if tmin <= tmax and tmin >= 0:
        return tmin
    return None


# ============================================================
# MÔI TRƯỜNG RL (thay thế RLEnv C++)
# ============================================================
class RLEnv:
    """
    Drop-in replacement cho azgame_env.RLEnv (C++).
    Cùng API: reset(), step(action0, action1), get_state(playerIdx), render().
    """

    def __init__(self, num_players=2, map_enabled=False, items_enabled=False, training_mode=0):
        self.num_players = num_players
        self.map_enabled = map_enabled
        self.items_enabled = items_enabled
        self.training_mode = training_mode
        self.max_steps = 5000
        self.current_step = 0
        self.last_scores = [0] * 4
        self.last_distance = 1000.0
        self.needs_restart = True

        self.tanks = []
        self.bullets = []
        self.walls = []
        self.player_scores = [0] * 4

    def reset(self):
        """Khởi tạo ván mới, trả về state player 0."""
        self.walls = []
        self.bullets = []
        self.tanks = []
        self.current_step = 0
        self.needs_restart = False

        if self.map_enabled:
            self.walls = build_maze()

        # Spawn tanks tại vị trí đủ xa
        spawn_cells = []
        attempts = 0
        while len(spawn_cells) < self.num_players and attempts < 1000:
            p = get_random_cell_center()
            ok = True
            for sp in spawn_cells:
                if (p - sp).length_sq() < 1.0:
                    ok = False; break
            if ok:
                spawn_cells.append(p)
            attempts += 1

        for i in range(self.num_players):
            t = SimpleTank(i)
            if i < len(spawn_cells):
                t.pos = Vec2(spawn_cells[i].x, spawn_cells[i].y)
            t.angle = random.choice([0, PI/2, PI, 3*PI/2])
            self.tanks.append(t)

        self.last_scores = list(self.player_scores)
        self.last_distance = self._raw_distance(0)
        return self.get_state(0)

    def step(self, action0, action1=-1):
        """Thực hiện 1 bước, trả về (state, reward, done) dạng tuple."""
        # Lưu kết quả va chạm tường cho reward (tránh gọi collides_wall 2 lần)
        p0_hit_wall = False

        # Cập nhật tanks
        for t in self.tanks:
            if t.is_destroyed:
                continue
            act = action0 if t.player_index == 0 else action1
            if act < 0:
                act = -1  # Không hành động

            hit = False
            if act >= 0:
                hit = t.handle_movement(act, DT, self.walls)
            if t.player_index == 0:
                p0_hit_wall = hit

            # Bắn
            can_shoot = (act == 4 and t.shoot_cooldown <= 0 and self.training_mode != 2)
            if can_shoot:
                my_bullets = sum(1 for b in self.bullets if b.owner == t.player_index and not b.is_dead())
                if my_bullets < MAX_BULLETS_PER_PLAYER:
                    fwd = t.forward_dir()
                    spawn_pos = t.pos + fwd * BARREL_LENGTH
                    vel = fwd * BULLET_SPEED
                    self.bullets.append(SimpleBullet(spawn_pos, vel, t.player_index))
                    t.shoot_cooldown = SHOOT_COOLDOWN

            if t.shoot_cooldown > 0:
                t.shoot_cooldown -= DT

        # Cập nhật đạn
        for b in self.bullets:
            if not b.is_dead():
                b.update(DT, self.walls)

        # Kiểm tra va chạm đạn-tank
        for t in self.tanks:
            if t.is_destroyed:
                continue
            t_aabb = t.get_aabb()
            for b in self.bullets:
                if b.is_dead():
                    continue
                b_aabb = b.get_aabb()
                if t_aabb.overlaps(b_aabb):
                    b.time = 0  # Xóa đạn
                    t.is_destroyed = True
                    break

        # Dọn đạn chết
        self.bullets = [b for b in self.bullets if not b.is_dead()]

        # Kiểm tra điều kiện thắng
        alive = [t for t in self.tanks if not t.is_destroyed]
        if self.num_players > 1 and len(alive) <= 1:
            if len(alive) == 1:
                self.player_scores[alive[0].player_index] += 1
            self.needs_restart = True

        self.current_step += 1

        # ---- TÍNH REWARD (giống rl_env_wrapper.cpp) ----
        reward = -0.01  # Time penalty

        my_tank = None
        for t in self.tanks:
            if t.player_index == 0:
                my_tank = t; break

        # Phạt đâm tường (dùng kết quả đã tính ở trên, KHÔNG gọi lại collides_wall)
        if my_tank and not my_tank.is_destroyed:
            if p0_hit_wall:
                reward -= 0.1

        # Thưởng kill
        score_diff = self.player_scores[0] - self.last_scores[0]
        if score_diff > 0:
            reward += 100.0 * score_diff
            self.last_scores[0] = self.player_scores[0]

        p0_alive = my_tank is not None and not my_tank.is_destroyed
        if not p0_alive:
            reward -= 100.0

        # Shaping: khoảng cách
        current_dist = self._raw_distance(0)
        if p0_alive and current_dist < 1000.0:
            if current_dist > 350.0:
                reward -= 0.02
            elif current_dist < 80.0:
                reward -= 0.05
        self.last_distance = current_dist

        is_timeout = self.current_step >= self.max_steps
        done = self.needs_restart or is_timeout or (not p0_alive)

        if is_timeout and p0_alive:
            if self.training_mode == 2:
                reward += 100.0
            else:
                reward -= 50.0

        state = self.get_state(0)
        return (state, reward, done)

    def get_state(self, player_idx):
        """Trích xuất 13 đặc trưng (giống C++)."""
        my_tank = None
        enemy_tank = None
        for t in self.tanks:
            if t.player_index == player_idx:
                my_tank = t
            elif not t.is_destroyed:
                enemy_tank = t

        if my_tank and not my_tank.is_destroyed:
            state = []
            my_pos = my_tank.pos
            my_angle = my_tank.angle

            # 1. Radar 8 hướng
            ray_length = math.sqrt(SCREEN_WIDTH**2 + SCREEN_HEIGHT**2) / SCALE
            for i in range(8):
                angle = my_angle + i * (PI / 4.0)
                direction = Vec2(-math.sin(angle), math.cos(angle))
                frac = raycast_walls(my_pos, direction, ray_length, self.walls)
                state.append(frac)

            # 2-3. Góc + khoảng cách tới enemy
            if enemy_tank and not enemy_tank.is_destroyed:
                to_enemy = enemy_tank.pos - my_pos
                abs_angle = math.atan2(-to_enemy.x, to_enemy.y)
                rel_angle = abs_angle - my_angle
                state.append(math.cos(rel_angle))
                state.append(math.sin(rel_angle))
                state.append(min(1.0, to_enemy.length() / ray_length))
            else:
                state.extend([1.0, 0.0, 1.0])

            # 4. Số đạn đang bay
            my_bullets = sum(1 for b in self.bullets if b.owner == player_idx and not b.is_dead())
            state.append(min(1.0, my_bullets / 5.0))

            # 5. HP (1-hit kill → luôn 1.0 nếu sống)
            state.append(1.0)

            return state
        else:
            return [0.0] * 13

    def render(self):
        """Stub - Colab không có cửa sổ đồ họa."""
        return True

    def _raw_distance(self, player_idx):
        my_tank = None
        enemy_tank = None
        for t in self.tanks:
            if t.player_index == player_idx:
                my_tank = t
            elif not t.is_destroyed:
                enemy_tank = t
        if my_tank and enemy_tank:
            diff = my_tank.pos - enemy_tank.pos
            return diff.length() * SCALE
        return 1000.0
