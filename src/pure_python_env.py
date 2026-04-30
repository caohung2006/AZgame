"""
Pure Python Environment cho AZTank Game.
Thay thế hoàn toàn module C++ (azgame_env) để chạy được trên Google Colab.
Tái tạo trung thành logic game từ C++: vật lý xe tăng, đạn nảy tường,
radar 8 hướng, mê cung Recursive Backtracker, reward shaping.
"""
import math
import random
import heapq
import numpy as np

# ============================================================
# HẰNG SỐ (từ Constants.h)
# ============================================================
SCALE = 30.0                    # Hệ số quy đổi pixel → đơn vị Box2D (1m = 30px)
SCREEN_WIDTH = 1024              # Chiều rộng cửa sổ game (pixel)
SCREEN_HEIGHT = 768              # Chiều cao cửa sổ game (pixel)
PI = math.pi                     # Số pi
DT = 1.0 / 60.0                 # Bước thời gian cố định, giả lập 60 FPS (~0.0167s)

# Maze config (từ map.cpp)
MAZE_ROWS = 6                   # Số hàng ô mê cung
MAZE_COLS = 8                   # Số cột ô mê cung
CELL_W = 90.0                   # Chiều rộng mỗi ô mê cung (pixel)
CELL_H = 90.0                   # Chiều cao mỗi ô mê cung (pixel)
WALL_THICKNESS = 6.0            # Độ dày tường mê cung (pixel)
OFFSET_X = (SCREEN_WIDTH - (MAZE_COLS * CELL_W)) / 2.0   # Offset X để canh giữa mê cung
OFFSET_Y = (SCREEN_HEIGHT - (MAZE_ROWS * CELL_H)) / 2.0 - 50.0  # Offset Y để canh giữa mê cung

# Tank physics (từ tank.cpp)
MOVE_SPEED = 3.0                # Tốc độ di chuyển xe tăng (m/s trong Box2D)
TURN_SPEED = 3.0                # Tốc độ xoay xe tăng (rad/s)
TANK_HALF_W = 15.0 / SCALE     # Nửa chiều rộng bounding box thân xe (m) - Square 30x30
TANK_HALF_H = 15.0 / SCALE     # Nửa chiều cao bounding box thân xe (m) - Square 30x30
SHOOT_COOLDOWN = 0.15           # Thời gian chờ giữa 2 lần bắn (giây)
MAX_BULLETS_PER_PLAYER = 1     # Số đạn tối đa đồng thời mỗi người chơi (mỗi lần chỉ bắn 1 viên)
BULLET_SPEED = 6.0              # Tốc độ đạn (m/s trong Box2D)
BULLET_LIFETIME = 7.0           # Thời gian sống của đạn trước khi tự hủy (giây)
BULLET_RADIUS = 3.0 / SCALE    # Bán kính va chạm của đạn (m)
BARREL_LENGTH = 30.0 / SCALE   # Chiều dài nòng súng, xác định điểm spawn đạn (m)

# World bounds (Box2D coords)
WORLD_W = SCREEN_WIDTH / SCALE  # Chiều rộng thế giới vật lý (m) ≈ 34.13
WORLD_H = SCREEN_HEIGHT / SCALE # Chiều cao thế giới vật lý (m) ≈ 25.6


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
                 'shoot_cooldown', 'hw', 'hh', 'ammo')

    def __init__(self, player_index):
        self.pos = Vec2(WORLD_W / 2, WORLD_H / 2)
        self.angle = 0.0
        self.player_index = player_index
        self.is_destroyed = False
        self.shoot_cooldown = 0.0
        self.hw = TANK_HALF_W
        self.hh = TANK_HALF_H + 10.0 / SCALE  # hull + barrel (barrel extends to 25px = 15 + 10)
        self.ammo = 5

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

    def _overlaps_any_tank(self, other_tanks):
        """Kiểm tra xe có đè lên xe tăng khác không."""
        my_aabb = self.get_aabb()
        for t in other_tanks:
            if t is self or t.is_destroyed:
                continue
            if my_aabb.overlaps(t.get_aabb()):
                return True
        return False

    def handle_movement(self, action, dt, walls, other_tanks):
        """Di chuyển theo action (0 -> 11 (12 hướng, hướng thứ i xoay 1 góc i * pi/6 rồi di chuyển theo hướng đó)) với va chạm tường axis-separated.
        Trả về True nếu bị chặn bởi tường (dùng cho reward)."""

        """
        Di chuyển theo hướng các góc 0, pi/6,  
                                        pi/3, 
                                        pi/2, 
                                        2pi/3 , 
                                        5pi/6, 
                                        pi, 
                                        7pi/6, 
                                        4pi/3, 
                                        3pi/2, 
                                        5pi/3, 
                                        11pi/6    
        """
        # Xoay
        self.angle = action * PI / 6 

        # Tính vận tốc
        fwd = self.forward_dir()
        vx, vy = fwd.x * MOVE_SPEED * dt, fwd.y * MOVE_SPEED * dt

        blocked = False

        # Di chuyển trục X, revert nếu va chạm
        self.pos.x += vx
        if (walls and self._overlaps_any_wall(walls)) or self._overlaps_any_tank(other_tanks):
            self.pos.x -= vx
            blocked = True

        # Di chuyển trục Y, revert nếu va chạm
        self.pos.y += vy
        if (walls and self._overlaps_any_wall(walls)) or self._overlaps_any_tank(other_tanks):
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
# A* PATHFINDING TRÊN GRID
# ============================================================
# Kích thước mỗi ô grid (đơn vị Box2D)
# Chọn ~1/3 chiều rộng ô mê cung để đủ mịn nhưng không quá chậm
A_STAR_CELL = (CELL_W / 3.0) / SCALE   # ≈ 1.0 m


def _build_grid(walls):
    """
    Xây dựng occupancy grid từ danh sách tường.
    Ô (r, c) = True nếu bị tường chặn.
    Grid gốc tại góc dưới-trái (Box2D 0,0).
    """
    cols = max(1, int(math.ceil(WORLD_W / A_STAR_CELL)))
    rows = max(1, int(math.ceil(WORLD_H / A_STAR_CELL)))
    blocked = [[False] * cols for _ in range(rows)]

    margin = A_STAR_CELL * 0.4   # padding nhỏ quanh tường cho xe không kẹt
    for w in walls:
        a = w.aabb
        x0 = a.cx - a.hw - margin
        x1 = a.cx + a.hw + margin
        y0 = a.cy - a.hh - margin
        y1 = a.cy + a.hh + margin
        c0 = max(0, int(x0 / A_STAR_CELL))
        c1 = min(cols - 1, int(x1 / A_STAR_CELL))
        r0 = max(0, int(y0 / A_STAR_CELL))
        r1 = min(rows - 1, int(y1 / A_STAR_CELL))
        for r in range(r0, r1 + 1):
            for c in range(c0, c1 + 1):
                blocked[r][c] = True

    return blocked, rows, cols


def _pos_to_cell(x, y):
    """Chuyển tọa độ Box2D → (row, col) trong grid."""
    return int(y / A_STAR_CELL), int(x / A_STAR_CELL)


def _cell_to_pos(r, c):
    """Chuyển (row, col) → tọa độ Box2D tâm ô."""
    return (c + 0.5) * A_STAR_CELL, (r + 0.5) * A_STAR_CELL


def astar_path(start_pos, goal_pos, walls):
    """
    Tìm đường A* từ start_pos đến goal_pos (Box2D Vec2).
    Trả về list[(x, y)] các waypoint (tâm ô), hoặc [] nếu không tìm được.
    Heuristic: khoảng cách Euclidean.
    """
    blocked, rows, cols = _build_grid(walls)

    sr, sc = _pos_to_cell(start_pos.x, start_pos.y)
    gr, gc = _pos_to_cell(goal_pos.x, goal_pos.y)

    # Clamp
    sr = max(0, min(rows - 1, sr))
    sc = max(0, min(cols - 1, sc))
    gr = max(0, min(rows - 1, gr))
    gc = max(0, min(cols - 1, gc))

    # Nếu đích bị tường chặn, tìm ô gần nhất không bị chặn
    if blocked[gr][gc]:
        found = False
        for dr in range(-2, 3):
            for dc in range(-2, 3):
                nr, nc = gr + dr, gc + dc
                if 0 <= nr < rows and 0 <= nc < cols and not blocked[nr][nc]:
                    gr, gc = nr, nc
                    found = True
                    break
            if found:
                break

    if (sr, sc) == (gr, gc):
        return []   # Đã ở đích

    # 8-chiều di chuyển
    DIRS = [(-1, 0, 1.0), (1, 0, 1.0), (0, -1, 1.0), (0, 1, 1.0),
            (-1, -1, 1.414), (-1, 1, 1.414), (1, -1, 1.414), (1, 1, 1.414)]

    open_heap = []   # (f, g, r, c)
    g_cost = {(sr, sc): 0.0}
    parent = {(sr, sc): None}

    def h(r, c):
        return math.sqrt((r - gr)**2 + (c - gc)**2)

    heapq.heappush(open_heap, (h(sr, sc), 0.0, sr, sc))

    while open_heap:
        f, g, r, c = heapq.heappop(open_heap)

        if (r, c) == (gr, gc):
            # Tái tạo đường đi
            path = []
            cur = (gr, gc)
            while cur is not None:
                px, py = _cell_to_pos(cur[0], cur[1])
                path.append((px, py))
                cur = parent[cur]
            path.reverse()
            return path   # list[(x, y)]

        # Bỏ qua nếu đã có đường tốt hơn
        if g > g_cost.get((r, c), float('inf')):
            continue

        for dr, dc, cost in DIRS:
            nr, nc = r + dr, c + dc
            if not (0 <= nr < rows and 0 <= nc < cols):
                continue
            if blocked[nr][nc]:
                continue
            # Fix corner cutting: di chuyển chéo phải kiểm tra cả 2 ô kề bên
            # Tránh tank đi xuyên qua góc tường (vật lý không cho phép)
            if dr != 0 and dc != 0:
                if blocked[r + dr][c] or blocked[r][c + dc]:
                    continue
            ng = g + cost
            if ng < g_cost.get((nr, nc), float('inf')):
                g_cost[(nr, nc)] = ng
                parent[(nr, nc)] = (r, c)
                heapq.heappush(open_heap, (ng + h(nr, nc), ng, nr, nc))

    return []   # Không tìm được đường


def astar_path_length(start_pos, goal_pos, walls):
    """
    Trả về tổng độ dài A* path (đơn vị Box2D * A_STAR_CELL).
    Dùng cho reward shaping thay đường chim bay.
    Trả về -1.0 nếu không tìm được đường.
    """
    path = astar_path(start_pos, goal_pos, walls)
    if not path:
        return -1.0
    total = 0.0
    for i in range(1, len(path)):
        dx = path[i][0] - path[i-1][0]
        dy = path[i][1] - path[i-1][1]
        total += math.sqrt(dx*dx + dy*dy)
    return total


def get_astar_waypoint_dir(my_pos, enemy_pos, walls):
    """
    Trả về (dx, dy) đã normalize hướng từ tank tới waypoint A* tiếp theo.
    Nếu không tìm được đường → trả về hướng thẳng tới enemy.
    Dùng để thêm vào state vector.
    """
    path = astar_path(my_pos, enemy_pos, walls)

    if len(path) >= 2:
        # Bỏ qua waypoint đầu (chính là vị trí hiện tại)
        # Lấy waypoint thứ 2 hoặc xa hơn (lookahead=2) để mượt hơn
        lookahead = min(2, len(path) - 1)
        wx, wy = path[lookahead]
    elif len(path) == 1:
        wx, wy = path[0]
    else:
        # Không tìm được đường → hướng thẳng
        wx, wy = enemy_pos.x, enemy_pos.y

    dx = wx - my_pos.x
    dy = wy - my_pos.y
    length = math.sqrt(dx*dx + dy*dy)
    if length > 1e-6:
        dx /= length
        dy /= length
    return dx, dy


# ============================================================
# MÔI TRƯỜNG RL (thay thế RLEnv C++)
# ============================================================
class RLEnv:
    """
    Drop-in replacement cho azgame_env.RLEnv (C++).
    Cùng API: reset(), step(action0, action1), get_state(playerIdx), render().
    """

    def __init__(self, num_players=2, map_enabled=False, items_enabled=False,
                 training_mode=0, use_astar=None):
        self.num_players = num_players
        self.map_enabled = map_enabled
        self.items_enabled = items_enabled
        self.training_mode = training_mode
        self.max_steps = 10000
        self.current_step = 0
        self.last_scores = [0] * 2
        self.last_distance = 1000.0
        self.needs_restart = True

        # A* chỉ có ý nghĩa khi có tường (map_enabled=True)
        # use_astar=None → tự động theo map_enabled
        if use_astar is None:
            self.use_astar = map_enabled
        else:
            self.use_astar = use_astar

        self.tanks = []
        self.bullets = []
        self.walls = []
        self.player_scores = [0] * 2
        self.idle_steps = 0          # So buoc agent dung yen lien tiep
        self._last_pos = {}          # {player_idx: (x, y)} - theo doi vi tri de phat idle

    @property
    def obs_size(self):
        """Kích thước observation vector (25 hoặc 27 tùy use_astar)."""
        return 27 if self.use_astar else 25

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
        self.idle_steps = 0
        self._last_pos = {}
        if hasattr(self, 'last_dist_error'):
            del self.last_dist_error
        return self.get_state(0)


    def step(self, action0, action1 = -1) : 
        """
        do a action, return tuple(state, reward, done)
        """
        p0_hit_wall = False

        for t in self.tanks : 
            if t.is_destroyed : 
                continue

            act = action0 if t.player_index == 0 else action1 
            if act < 0: 
                act = -1
            hit = False
            # Chỉ gọi hàm di chuyển nếu action từ 0 đến 11 (12 góc). 12 là bắn.
            if 0 <= act < 12: 
                hit = t.handle_movement(act, DT, self.walls, self.tanks) 
            if t.player_index == 0: 
                p0_hit_wall = hit
            
            # fire 
            can_shoot = (act == 12 and t.shoot_cooldown <= 0.0 and self.training_mode != 2 and t.ammo > 0) 
            if can_shoot: 
                my_bullet = sum(1 for b in self.bullets if b.owner == t.player_index and not b.is_dead()) 
                if my_bullet < MAX_BULLETS_PER_PLAYER: 
                    fwd = t.forward_dir() 
                    spawn_pos = t.pos + fwd * BARREL_LENGTH
                    vel = fwd * BULLET_SPEED
                    self.bullets.append(SimpleBullet(spawn_pos, vel, t.player_index))
                    t.shoot_cooldown = SHOOT_COOLDOWN 
                    t.ammo -= 1
            if t.shoot_cooldown > 0.0: 
                t.shoot_cooldown -= DT 

        for b in self.bullets: 
            if not b.is_dead(): 
                b.update(DT, self.walls) 

        for t in self.tanks: 
            if t.is_destroyed: 
                continue 
            t_aabb = t.get_aabb() 
            for b in self.bullets: 
                if b.is_dead(): 
                    continue
                b_aabb = b.get_aabb() 
                if t_aabb.overlaps(b_aabb): 
                    b.time = 0
                    t.is_destroyed = True 
                    break         

        self.bullets = [b for b in self.bullets if not b.is_dead()]

        alive = [t for t in self.tanks if not t.is_destroyed]
        if self.num_players > 1 and len(alive) <= 1: 
            if len(alive) == 1: 
                self.player_scores[alive[0].player_index] += 1 
            self.needs_restart = True 
        self.current_step += 1

        # ----- calculate rewards ------ # 
        reward = -0.01  # Time penalty
        
        my_tank = None 
        enemy_tank = None
        for t in self.tanks: 
            if t.player_index == 0: 
                my_tank = t 
            elif not t.is_destroyed:
                enemy_tank = t
        
        p0_alive = my_tank is not None and not my_tank.is_destroyed

        # --- Idle penalty ---
        if p0_alive:
            IDLE_THRESHOLD = 1e-4
            last = self._last_pos.get(0, (my_tank.pos.x, my_tank.pos.y))
            moved = (abs(my_tank.pos.x - last[0]) +
                     abs(my_tank.pos.y - last[1])) > IDLE_THRESHOLD
            if moved:
                self.idle_steps = 0
                reward += 0.005
            else:
                self.idle_steps += 1
                if self.idle_steps > 30:
                    reward -= 0.02 * min(self.idle_steps / 30, 3.0)
            self._last_pos[0] = (my_tank.pos.x, my_tank.pos.y)

        if p0_alive:
            if p0_hit_wall: 
                reward -= 0.05 
        
        score_diff = self.player_scores[0] - self.last_scores[0]
        if score_diff > 0: 
            reward += 100.0 * score_diff
            self.last_scores[0] = self.player_scores[0] 
        
        if not p0_alive: 
            reward -= 100.0 

        # Potential-based Distance Shaping & Aiming Reward (dùng A* path distance)
        if p0_alive and enemy_tank and not enemy_tank.is_destroyed:
            # Dùng A* path length nếu có tường, fallback về đường chim bay
            astar_len = astar_path_length(my_tank.pos, enemy_tank.pos, self.walls)
            if astar_len < 0:
                current_dist = self._raw_distance(0)      # fallback
            else:
                current_dist = astar_len * SCALE          # đổi về pixel để so sánh

            optimal_dist = 200.0
            dist_error = abs(current_dist - optimal_dist)

            if hasattr(self, 'last_dist_error'):
                reward += (self.last_dist_error - dist_error) * 0.05
            self.last_dist_error = dist_error
            self.last_distance = current_dist

            to_enemy = enemy_tank.pos - my_tank.pos
            abs_angle = math.atan2(-to_enemy.x, to_enemy.y)
            rel_angle = abs_angle - my_tank.angle
            rel_angle = (rel_angle + PI) % (2 * PI) - PI
            
            if abs(rel_angle) < PI / 6:
                reward += 0.01

            if action0 == 12 and my_tank.shoot_cooldown == SHOOT_COOLDOWN:
                if abs(rel_angle) < PI / 6:
                    reward += 0.1
        
        # Check if both tanks are out of bullets
        all_out_of_bullets = all(
            t.ammo == 0 and sum(1 for b in self.bullets if b.owner == t.player_index and not b.is_dead()) == 0
            for t in self.tanks if not t.is_destroyed
        )

        if all_out_of_bullets and len(alive) == 2:
            reward = 0.0  # No points for a draw
            self.needs_restart = True

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
        """
        Trích xuất đặc trưng:
          [0-7]  : Wall Radar 8 hướng
          [8-10] : Góc (cos, sin) + khoảng cách tới enemy (đường chim bay)
          [11]   : Số đạn đang bay
          [12]   : HP
          [13-24]: Bullet Radar (3 viên gần nhất)
          [25-26]: A* waypoint direction (dx, dy) — chỉ khi use_astar=True
        Trả về 25 floats (use_astar=False) hoặc 27 floats (use_astar=True).
        """
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

            # 1. Wall Radar 8 hướng (SCAN_RADIUS = 250.0)
            SCAN_RADIUS = 250.0 / SCALE
            for i in range(8):
                angle = my_angle + i * (PI / 4.0)
                direction = Vec2(-math.sin(angle), math.cos(angle))
                frac = raycast_walls(my_pos, direction, SCAN_RADIUS, self.walls)
                state.append(frac)

            # 2-3. Góc + khoảng cách tới enemy (đường chim bay)
            if enemy_tank and not enemy_tank.is_destroyed:
                to_enemy = enemy_tank.pos - my_pos
                abs_angle = math.atan2(-to_enemy.x, to_enemy.y)
                rel_angle = abs_angle - my_angle
                state.append(math.cos(rel_angle))
                state.append(math.sin(rel_angle))
                state.append(min(1.0, to_enemy.length() / (1000.0/SCALE)))
            else:
                state.extend([1.0, 0.0, 1.0])

            # 4. Số đạn còn lại của bản thân
            state.append(my_tank.ammo / 5.0)

            # 5. HP
            state.append(1.0)

            # 6. Bullet Radar: Tìm 3 viên đạn gần nhất
            nearby_bullets = []
            for b in self.bullets:
                if b.is_dead(): continue
                dist = (b.pos - my_pos).length()
                if dist <= SCAN_RADIUS:
                    nearby_bullets.append((dist, b))

            nearby_bullets.sort(key=lambda x: x[0])

            for i in range(3):
                if i < len(nearby_bullets):
                    dist, b = nearby_bullets[i]
                    dx = (b.pos.x - my_pos.x) / SCAN_RADIUS
                    dy = (b.pos.y - my_pos.y) / SCAN_RADIUS
                    vx = b.vel.x / BULLET_SPEED
                    vy = b.vel.y / BULLET_SPEED
                    state.extend([dx, dy, vx, vy])
                else:
                    state.extend([1.0, 1.0, 0.0, 0.0])

            # 7. A* Waypoint Direction [25-26] — chỉ thêm khi use_astar=True
            if self.use_astar:
                if enemy_tank and not enemy_tank.is_destroyed:
                    wp_dx, wp_dy = get_astar_waypoint_dir(my_pos, enemy_tank.pos, self.walls)
                    state.append(wp_dx)
                    state.append(wp_dy)
                else:
                    state.extend([0.0, 0.0])

            return state   # 25 floats (no A*) hoặc 27 floats (with A*)
        else:
            return [0.0] * self.obs_size

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
