"""
Xem trực quan game AZTank bằng Pygame.
Đồ họa trung thành với renderer.cpp gốc (C++/Raylib):
  - Tank: xích + thân 3 lớp + tháp pháo tròn + nòng súng
  - Đạn: viên tròn có glow
  - Tường: shadow + viền tối + lõi sáng
  - Hiệu ứng nổ particle khi xe tăng chết

Hỗ trợ load model PPO đã train để xem AI chơi.

Cách dùng:
  python play_game.py                                        # Người chơi vs Random
  python play_game.py --model models/ppo_tank_phase1.zip     # AI vs Random  
  python play_game.py --model models/ppo_tank_phase7.zip --p1-model models/ppo_tank_phase5.zip  # AI vs AI

Điều khiển (khi không dùng --model):
  Player 0 (Xanh): W/A/S/D + Space bắn
  Player 1 (Đỏ):   Mũi tên + Enter bắn
  R: Reset ván  |  ESC: Thoát
  M: Bật/Tắt mode AI random cho Player 1
"""
import sys
import os
import math
import random
import argparse
import time as time_module
import numpy as np

try:
    import pygame
except ImportError:
    print("Can cai pygame: pip install pygame")
    sys.exit(1)

from pure_python_env import (
    RLEnv, SCREEN_WIDTH, SCREEN_HEIGHT, SCALE,
    WORLD_W, WORLD_H, TANK_HALF_W, TANK_HALF_H, PI
)

# ============================================================
# THIẾT LẬP
# ============================================================
WINDOW_W = SCREEN_WIDTH
WINDOW_H = SCREEN_HEIGHT
FPS = 60
RAD2DEG = 180.0 / PI

BG_COLOR = (20, 20, 25)

# ============================================================
# BẢNG MÀU XE TĂNG (giống renderer.cpp palette)
# ============================================================
# Mỗi player: (body, dark, light, track, barrel)
TANK_PALETTES = [
    # Player 0: Xanh lá
    {"body": (60,160,60), "dark": (35,100,35), "light": (100,210,100), "track": (30,70,30), "barrel": (85,85,85)},
    # Player 1: Xanh dương
    {"body": (50,90,190), "dark": (30,55,140), "light": (90,140,235), "track": (25,40,100), "barrel": (85,85,85)},
    # Player 2: Đỏ
    {"body": (190,50,50), "dark": (135,30,30), "light": (235,90,90), "track": (95,25,25), "barrel": (85,85,85)},
    # Player 3: Vàng
    {"body": (210,180,50), "dark": (155,135,25), "light": (245,220,85), "track": (115,95,20), "barrel": (85,85,85)},
]


# ============================================================
# HÀM PHỤ TRỢ VẼ
# ============================================================
def b2p(bx, by):
    """Box2D coords → Pixel coords (lật trục Y)."""
    return bx * SCALE, WINDOW_H - by * SCALE


def rotate_point(cx, cy, px, py, angle_rad):
    """Xoay điểm (px,py) quanh tâm (cx,cy) theo góc angle_rad."""
    cos_a = math.cos(angle_rad)
    sin_a = math.sin(angle_rad)
    dx = px - cx
    dy = py - cy
    return cx + dx * cos_a - dy * sin_a, cy + dx * sin_a + dy * cos_a


def draw_rotated_rect(surface, color, cx, cy, w, h, origin_x, origin_y, angle_deg):
    """
    Vẽ hình chữ nhật xoay (giống DrawRectanglePro của Raylib).
    origin_x, origin_y: điểm neo (pivot) tính từ góc trên-trái của rect.
    angle_deg: góc xoay (độ).
    """
    angle_rad = math.radians(angle_deg)
    # 4 góc trước khi xoay (tính từ origin)
    corners = [
        (-origin_x, -origin_y),
        (w - origin_x, -origin_y),
        (w - origin_x, h - origin_y),
        (-origin_x, h - origin_y),
    ]
    cos_a = math.cos(angle_rad)
    sin_a = math.sin(angle_rad)
    rotated = []
    for dx, dy in corners:
        rx = cx + dx * cos_a - dy * sin_a
        ry = cy + dx * sin_a + dy * cos_a
        rotated.append((rx, ry))
    pygame.draw.polygon(surface, color, rotated)


def draw_circle_alpha(surface, color, alpha, cx, cy, radius):
    """Vẽ hình tròn bán trong suốt."""
    if alpha <= 0 or radius <= 0:
        return
    s = pygame.Surface((int(radius*2+2), int(radius*2+2)), pygame.SRCALPHA)
    a = max(0, min(255, int(alpha * 255)))
    pygame.draw.circle(s, (*color, a), (int(radius+1), int(radius+1)), int(radius))
    surface.blit(s, (int(cx - radius - 1), int(cy - radius - 1)))


# ============================================================
# PARTICLE SYSTEM (giống renderer.cpp)
# ============================================================
class Particle:
    __slots__ = ('x', 'y', 'vx', 'vy', 'life', 'max_life', 'size', 'start_size', 'r', 'g', 'b')
    def __init__(self, x, y, vx, vy, life, size, r, g, b):
        self.x = x; self.y = y
        self.vx = vx; self.vy = vy
        self.life = life; self.max_life = life
        self.size = size; self.start_size = size
        self.r = r; self.g = g; self.b = b


particles = []

def spawn_explosion(sx, sy, player_index):
    """Tạo hiệu ứng nổ giống C++ (40 lửa + 15 khói + 8 mảnh)."""
    global particles
    # Lửa
    for _ in range(40):
        angle = random.random() * 2 * PI
        speed = 60 + random.random() * 200
        life = 0.3 + random.random() * 0.6
        size = 3 + random.random() * 7
        ct = random.randint(0, 3)
        if ct == 0: r, g, b = 255, 220, 60
        elif ct == 1: r, g, b = 255, 140, 20
        elif ct == 2: r, g, b = 240, 60, 20
        else: r, g, b = 255, 255, 200
        particles.append(Particle(
            sx + random.randint(-3, 3), sy + random.randint(-3, 3),
            math.cos(angle) * speed, math.sin(angle) * speed,
            life, size, r, g, b
        ))
    # Khói
    for _ in range(15):
        angle = random.random() * 2 * PI
        speed = 20 + random.random() * 60
        life = 0.6 + random.random() * 0.8
        size = 6 + random.random() * 10
        gray = 70 + random.randint(0, 30)
        particles.append(Particle(
            sx, sy,
            math.cos(angle) * speed, math.sin(angle) * speed - 15,
            life, size, gray, gray, gray
        ))
    # Mảnh vỡ màu xe
    debris_colors = [
        (60, 160, 60), (50, 90, 190), (190, 50, 50), (210, 180, 50)
    ]
    cr, cg, cb = debris_colors[player_index % 4]
    for _ in range(8):
        angle = random.random() * 2 * PI
        speed = 100 + random.random() * 150
        life = 0.5 + random.random() * 0.5
        size = 4 + random.random() * 4
        particles.append(Particle(
            sx, sy,
            math.cos(angle) * speed, math.sin(angle) * speed,
            life, size, cr, cg, cb
        ))


def update_particles(dt):
    """Animate particles (ma sát 0.96, co nhỏ dần)."""
    global particles
    alive = []
    for p in particles:
        p.life -= dt
        if p.life > 0:
            p.x += p.vx * dt
            p.y += p.vy * dt
            p.vx *= 0.96
            p.vy *= 0.96
            p.size = p.start_size * (p.life / p.max_life)
            alive.append(p)
    particles = alive


def draw_particles(screen):
    """Vẽ particles với alpha fade."""
    for p in particles:
        alpha = p.life / p.max_life
        if p.size > 0.5:
            draw_circle_alpha(screen, (p.r, p.g, p.b), alpha, p.x, p.y, p.size)
            # Glow cho lửa sáng
            if p.r > 200 and p.size > 3:
                draw_circle_alpha(screen, (p.r, p.g, p.b), alpha * 0.15, p.x, p.y, p.size * 1.5)


# ============================================================
# VẼ XE TĂNG (Đã được cách mạng hóa thành hình vuông)
# ============================================================
def draw_tank(screen, tank):
    """Vẽ xe tăng hình vuông 30x30: shadow → xích → thân → nòng → tháp pháo vuông."""
    x, y = b2p(tank.pos.x, tank.pos.y)
    rot = -tank.angle * RAD2DEG  # Raylib convention
    c = TANK_PALETTES[tank.player_index % 4]

    # --- Shadow (Hình vuông 30x30) ---
    draw_rotated_rect(screen, (5, 5, 8), x + 2, y + 2, 30, 30, 15, 15, rot)

    # --- Xích xe (Bên trái: 6x30, Bên phải: 6x30) ---
    draw_rotated_rect(screen, c["track"], x, y, 6, 30, 15, 15, rot)   # Trái
    draw_rotated_rect(screen, c["track"], x, y, 6, 30, -9, 15, rot)   # Phải

    # Chi tiết xích (4 vạch mỗi bên)
    track_line = (min(255, c["track"][0]+25), min(255, c["track"][1]+25), min(255, c["track"][2]+25))
    for i in range(5):
        y_off = -12 + i * 6
        draw_rotated_rect(screen, track_line, x, y, 6, 2, 15, 1 - y_off, rot)
        draw_rotated_rect(screen, track_line, x, y, 6, 2, -9, 1 - y_off, rot)

    # --- Thân xe (18x30 ở giữa) tạo thành tổng thể vuông 30x30 ---
    draw_rotated_rect(screen, c["dark"], x, y, 18, 30, 9, 15, rot)    # Viền tối
    draw_rotated_rect(screen, c["body"], x, y, 14, 26, 7, 13, rot)    # Thân chính
    draw_rotated_rect(screen, c["light"], x, y, 8, 14, 4, 7, rot)     # Highlight

    # --- Nòng súng ---
    draw_rotated_rect(screen, c["dark"], x, y, 8, 16, 4, 25, rot)     # Viền nòng
    draw_rotated_rect(screen, c["barrel"], x, y, 5, 15, 2.5, 24.5, rot)  # Nòng trong
    draw_rotated_rect(screen, (50, 50, 50), x, y, 4, 3, 2, 25, rot)   # Lỗ nòng

    # --- Tháp pháo vuông (Square Turret) ---
    draw_rotated_rect(screen, c["dark"], x, y, 16, 16, 8, 8, rot)
    draw_rotated_rect(screen, c["body"], x, y, 12, 12, 6, 6, rot)
    draw_rotated_rect(screen, c["light"], x, y, 6, 6, 3, 3, rot)


# ============================================================
# VẼ ĐẠN (giống renderer.cpp DrawBullet - đạn thường)
# ============================================================
def draw_bullet(screen, bullet):
    """Vẽ đạn thường: glow + thân tối + highlight."""
    x, y = b2p(bullet.pos.x, bullet.pos.y)
    ix, iy = int(x), int(y)
    # Glow
    draw_circle_alpha(screen, (80, 80, 80), 0.15, x, y, 6)
    # Thân
    pygame.draw.circle(screen, (40, 40, 40), (ix, iy), 4)
    # Highlight
    pygame.draw.circle(screen, (90, 90, 90), (ix, iy), 2)


# ============================================================
# VẼ TƯỜNG (giống renderer.cpp DrawMap)
# ============================================================
def draw_wall(screen, wall):
    """Vẽ tường: shadow → viền tối → lõi sáng."""
    cx, cy = b2p(wall.aabb.cx, wall.aabb.cy)
    w = wall.aabb.hw * 2 * SCALE
    h = wall.aabb.hh * 2 * SCALE
    ix, iy = int(cx - w/2), int(cy - h/2)
    iw, ih = int(w), int(h)

    # Shadow
    pygame.draw.rect(screen, (5, 5, 8), (ix + 2, iy + 2, iw, ih))
    # Viền tối
    pygame.draw.rect(screen, (65, 70, 80), (ix, iy, iw, ih))
    # Lõi sáng hơn (nhỏ hơn 1.5px mỗi bên)
    if iw > 3 and ih > 3:
        pygame.draw.rect(screen, (85, 90, 100), (ix + 1, iy + 1, iw - 2, ih - 2))


# ============================================================
# VẼ HUD
# ============================================================
def draw_hud(screen, font, env, p2_auto, p0_label, p1_label, game_time):
    """Vẽ HUD: điểm, bước, trạng thái."""
    # Nền bán trong suốt cho HUD
    hud_bg = pygame.Surface((280, 120), pygame.SRCALPHA)
    hud_bg.fill((0, 0, 0, 100))
    screen.blit(hud_bg, (5, 5))

    texts = [
        (f"P0[{p0_label}]: {env.player_scores[0]}  |  P1[{p1_label}]: {env.player_scores[1]}", (200, 200, 200)),
        (f"Step: {env.current_step}/{env.max_steps}", (160, 160, 160)),
        (f"Bullets: {len(env.bullets)}  |  FPS: {int(1.0/max(0.001, game_time))}", (160, 160, 160)),
        (f"P1 auto: {'ON' if p2_auto else 'OFF'} (M toggle)", (130, 130, 130)),
        ("R: Reset  |  ESC: Quit", (100, 100, 100)),
    ]
    for i, (text, color) in enumerate(texts):
        surf = font.render(text, True, color)
        screen.blit(surf, (12, 12 + i * 20))


# ============================================================
# BANG DIEM THANG (top-center)
# ============================================================
def draw_scoreboard(screen, font_big, font_med, env, p0_label, p1_label):
    """Ve bang diem thang lon o giua tren cung man hinh."""
    s0 = env.player_scores[0]
    s1 = env.player_scores[1]

    # Mau cua tung player
    P0_COLOR  = (100, 210, 100)   # Xanh la
    P1_COLOR  = (90,  140, 235)   # Xanh duong
    SEP_COLOR = (180, 180, 180)

    # Render cac phan
    lbl0 = font_med.render(f"P0 [{p0_label}]", True, P0_COLOR)
    lbl1 = font_med.render(f"P1 [{p1_label}]", True, P1_COLOR)
    sc0  = font_big.render(str(s0), True, P0_COLOR)
    sc1  = font_big.render(str(s1), True, P1_COLOR)
    sep  = font_big.render(":", True, SEP_COLOR)

    # Kich thuoc tong
    total_w = sc0.get_width() + sep.get_width() + sc1.get_width() + 40
    cx = WINDOW_W // 2
    cy = 30

    # Nen glassmorphism
    pad_x, pad_y = 24, 10
    bg_w = max(total_w + lbl0.get_width() + lbl1.get_width() + 80, 280)
    bg_h = sc0.get_height() + lbl0.get_height() + pad_y * 2 + 6
    bg = pygame.Surface((bg_w, bg_h), pygame.SRCALPHA)
    bg.fill((0, 0, 0, 130))
    pygame.draw.rect(bg, (255, 255, 255, 25), (0, 0, bg_w, bg_h), border_radius=12)
    pygame.draw.rect(bg, (255, 255, 255, 40), (0, 0, bg_w, 2))   # top highlight
    screen.blit(bg, (cx - bg_w // 2, cy - pad_y))

    # Vi tri cac phan tu
    sc0_x = cx - sep.get_width() // 2 - sc0.get_width() - 16
    sc1_x = cx + sep.get_width() // 2 + 16
    sep_x = cx - sep.get_width() // 2
    score_y = cy + lbl0.get_height() + 2

    # Label tren
    screen.blit(lbl0, (sc0_x + sc0.get_width() // 2 - lbl0.get_width() // 2, cy))
    screen.blit(lbl1, (sc1_x + sc1.get_width() // 2 - lbl1.get_width() // 2, cy))

    # So diem
    screen.blit(sc0,  (sc0_x, score_y))
    screen.blit(sep,  (sep_x, score_y))
    screen.blit(sc1,  (sc1_x, score_y))


# ============================================================
# LOAD MODEL
# ============================================================
def load_model(path):
    if path is None:
        return None
    if not os.path.exists(path):
        if os.path.exists(path + ".zip"):
            path = path + ".zip"
        else:
            print(f"  [Loi] Khong tim thay model: {path}")
            return None
    try:
        from stable_baselines3 import PPO
        model = PPO.load(path)
        print(f"  [OK] Da load model: {path}")
        return model
    except ImportError:
        print("  [Loi] Can cai: pip install stable-baselines3")
        return None


def get_model_action(model, env, player_idx):
    state = env.get_state(player_idx)
    obs = np.array(state, dtype=np.float32)
    
    # Tu dong khop kich thuoc observation (25 hoac 27)
    if hasattr(model, "observation_space"):
        expected_shape = model.observation_space.shape[0]
        if expected_shape == 25 and obs.shape[0] == 27:
            obs = obs[:25]  # Cat bo 2 feature A* (tuong thich model cu)
        elif expected_shape == 27 and obs.shape[0] == 25:
            obs = np.pad(obs, (0, 2), 'constant')  # Pad them 0 neu env khong map
            
    action, _ = model.predict(obs, deterministic=True)
    return int(action)


# ============================================================
# MAIN GAME LOOP
# ============================================================
def main():
    parser = argparse.ArgumentParser(description="AZTank Game Viewer")
    parser.add_argument("--model", type=str, default=None,
                        help="Model PPO cho Player 0 (xe xanh)")
    parser.add_argument("--p1-model", type=str, default=None,
                        help="Model PPO cho Player 1 (xe đỏ)")
    parser.add_argument("--no-map", action="store_true",
                        help="Tắt mê cung")
    args = parser.parse_args()

    map_enabled = not args.no_map

    p0_model = load_model(args.model)
    p1_model = load_model(args.p1_model)
    p0_label = "AI" if p0_model else "WASD"
    p1_label = "AI" if p1_model else "Random/Arrows"

    pygame.init()
    screen = pygame.display.set_mode((WINDOW_W, WINDOW_H))
    title = "AZTank"
    if p0_model: title += " — P0:AI"
    if p1_model: title += " — P1:AI"
    pygame.display.set_caption(title)
    clock = pygame.time.Clock()
    font = pygame.font.SysFont("Consolas", 14)
    font_big = pygame.font.SysFont("Consolas", 36, bold=True)
    font_med = pygame.font.SysFont("Consolas", 18, bold=True)

    env = RLEnv(num_players=2, map_enabled=map_enabled, items_enabled=False)
    env.reset()

    # Luu trang thai tank truoc do de detect death
    prev_tank_indices = set()
    p2_auto = True
    running = True
    frame_time = 1.0 / 60.0

    # Idle override + action smoothing
    IDLE_LIMIT  = 120   # 2 giay o 60 FPS
    ACTION_HOLD = 20    # giu moi action 20 frames (~0.33s) truoc khi doi
    idle_counter  = {0: 0, 1: 0}
    last_pos      = {}
    smooth_action = {0: (-1, 0), 1: (-1, 0)}  # {player: (action, frames_remaining)}

    while running:
        t_start = time_module.time()

        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                running = False
            elif event.type == pygame.KEYDOWN:
                if event.key == pygame.K_ESCAPE:
                    running = False
                elif event.key == pygame.K_r:
                    env.reset()
                    particles.clear()
                    prev_tank_indices = {t.player_index for t in env.tanks if not t.is_destroyed}
                elif event.key == pygame.K_m:
                    p2_auto = not p2_auto

        keys = pygame.key.get_pressed()

        # Lưu tank sống trước step
        alive_before = {t.player_index for t in env.tanks if not t.is_destroyed}

        # ---- Player 0 ----
        if p0_model:
            raw0 = get_model_action(p0_model, env, 0)
            cur0, hold0 = smooth_action[0]
            if hold0 <= 0:
                # Het thoi gian giu -> nhan action moi tu model
                smooth_action[0] = (raw0, ACTION_HOLD)
            else:
                # Dang trong thoi gian giu -> bo qua action model moi
                smooth_action[0] = (cur0, hold0 - 1)
            action0 = smooth_action[0][0]
            # Idle override cho P0
            t0 = next((t for t in env.tanks if t.player_index == 0 and not t.is_destroyed), None)
            if t0:
                prev = last_pos.get(0, (t0.pos.x, t0.pos.y))
                if abs(t0.pos.x - prev[0]) + abs(t0.pos.y - prev[1]) < 1e-4:
                    idle_counter[0] += 1
                else:
                    idle_counter[0] = 0
                last_pos[0] = (t0.pos.x, t0.pos.y)
                if idle_counter[0] > IDLE_LIMIT:
                    action0 = random.randint(0, 11)
                    smooth_action[0] = (action0, ACTION_HOLD)
        else:
            action0 = -1
            if keys[pygame.K_w]:     action0 = 0
            elif keys[pygame.K_a]:   action0 = 3
            elif keys[pygame.K_s]:   action0 = 6
            elif keys[pygame.K_d]:   action0 = 9
            if keys[pygame.K_SPACE]: action0 = 12

        # ---- Player 1 ----
        if p1_model:
            raw1 = get_model_action(p1_model, env, 1)
            cur1, hold1 = smooth_action[1]
            if hold1 <= 0:
                smooth_action[1] = (raw1, ACTION_HOLD)
            else:
                smooth_action[1] = (cur1, hold1 - 1)
            action1 = smooth_action[1][0]
            # Idle override cho P1
            t1 = next((t for t in env.tanks if t.player_index == 1 and not t.is_destroyed), None)
            if t1:
                prev = last_pos.get(1, (t1.pos.x, t1.pos.y))
                if abs(t1.pos.x - prev[0]) + abs(t1.pos.y - prev[1]) < 1e-4:
                    idle_counter[1] += 1
                else:
                    idle_counter[1] = 0
                last_pos[1] = (t1.pos.x, t1.pos.y)
                if idle_counter[1] > IDLE_LIMIT:
                    action1 = random.randint(0, 11)
                    smooth_action[1] = (action1, ACTION_HOLD)
        elif p2_auto:
            action1 = random.randint(0, 12)
        else:
            action1 = -1
            if keys[pygame.K_UP]:      action1 = 0
            elif keys[pygame.K_LEFT]:  action1 = 3
            elif keys[pygame.K_DOWN]:  action1 = 6
            elif keys[pygame.K_RIGHT]: action1 = 9
            if keys[pygame.K_RETURN]:  action1 = 12

        # ---- Step game ----
        state, reward, done = env.step(action0, action1)

        # Detect deaths → spawn explosion
        alive_after = {t.player_index for t in env.tanks if not t.is_destroyed}
        for pi in alive_before - alive_after:
            # Tìm vị trí cuối cùng (ước lượng từ trung tâm)
            sx, sy = WINDOW_W / 2, WINDOW_H / 2
            # Thử tìm trong danh sách tank (có thể đã bị xóa)
            for t in env.tanks:
                if t.player_index == pi:
                    sx, sy = b2p(t.pos.x, t.pos.y)
                    break
            spawn_explosion(sx, sy, pi)

        # Update particles
        update_particles(1.0 / 60.0)

        if done:
            idle_counter  = {0: 0, 1: 0}
            last_pos      = {}
            smooth_action = {0: (-1, 0), 1: (-1, 0)}
            # Đợi một chút để xem hiệu ứng nổ
            for _ in range(30):
                update_particles(1.0 / 60.0)
                screen.fill(BG_COLOR)
                for wall in env.walls:
                    draw_wall(screen, wall)
                for tank in env.tanks:
                    if not tank.is_destroyed:
                        draw_tank(screen, tank)
                draw_particles(screen)
                draw_hud(screen, font, env, p2_auto, p0_label, p1_label, frame_time)
                pygame.display.flip()
                clock.tick(FPS)

            env.reset()
            particles.clear()
            alive_before = {t.player_index for t in env.tanks if not t.is_destroyed}

        # ---- VẼ ----
        screen.fill(BG_COLOR)

        # Vẽ tường
        for wall in env.walls:
            draw_wall(screen, wall)

        # Vẽ đạn
        for bullet in env.bullets:
            if not bullet.is_dead():
                draw_bullet(screen, bullet)

        # Vẽ xe tăng
        for tank in env.tanks:
            if not tank.is_destroyed:
                draw_tank(screen, tank)

        # Vẽ particles (hiệu ứng nổ)
        draw_particles(screen)

        # HUD goc trai
        draw_hud(screen, font, env, p2_auto, p0_label, p1_label, frame_time)

        # Bang diem thang (top-center)
        draw_scoreboard(screen, font_big, font_med, env, p0_label, p1_label)

        pygame.display.flip()
        clock.tick(FPS)
        frame_time = max(0.001, time_module.time() - t_start)

    pygame.quit()


if __name__ == "__main__":
    main()
