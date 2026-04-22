from __future__ import annotations

import math
import os
import tempfile
import time
from typing import Dict, List, Optional, Tuple

from astar_env import BridgeEnv, heading_error
from astar_planner import AStarPlanner


_REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
_WAYPOINTS_FILE = os.path.join(_REPO_ROOT, "bridge_waypoints.txt")
_LAST_WAYPOINTS_WRITE = 0.0
_STUCK_FRAMES = 10
_STUCK_MOVE_EPS2 = 0.001
_BACKOFF_FRAMES = 12
_TURN_ON_THRESHOLD = 0.18
_TURN_OFF_THRESHOLD = 0.12


def _tank(snapshot: Dict, player_index: int) -> Optional[Dict]:
    for t in snapshot.get("tanks", []):
        if int(t.get("player_index", -1)) == player_index:
            return t
    return None


def _build_planner(snapshot: Dict) -> AStarPlanner:
    scale = float(snapshot["scale"])
    walls = snapshot.get("walls", [])
    wall_aabbs = [(w["min_x"], w["min_y"], w["max_x"], w["max_y"]) for w in walls]
    return AStarPlanner.from_world_geometry(
        world_width=float(snapshot["screen_width"]) / scale,
        world_height=float(snapshot["screen_height"]) / scale,
        wall_aabbs=wall_aabbs,
        grid_width=96,
        grid_height=72,
        inflation_radius=0.55,
    )


def _plan_waypoints(planner: AStarPlanner, me: Dict, enemy: Dict) -> Optional[List[Tuple[float, float]]]:
    start = planner.world_to_grid(float(me["x"]), float(me["y"]))
    goal = planner.world_to_grid(float(enemy["x"]), float(enemy["y"]))
    result = planner.plan(start, goal)
    if not result.reached_goal or len(result.path) < 2:
        return None
    return [planner.grid_to_world(cell) for cell in result.path[1:]]


def _write_waypoints(waypoints: List[Tuple[float, float]], waypoint_idx: int, force: bool = False) -> None:
    global _LAST_WAYPOINTS_WRITE
    now = time.monotonic()
    if not force and (now - _LAST_WAYPOINTS_WRITE) < 0.05:
        return

    if waypoints:
        idx = min(max(waypoint_idx, 0), len(waypoints) - 1)
    else:
        idx = 0

    payload = [f"idx {idx}\n"]
    for x, y in waypoints:
        payload.append(f"{x:.4f} {y:.4f}\n")
    payload_str = "".join(payload)

    # Best effort only: never let debug drawing break the controller loop.
    try:
        fd, tmp = tempfile.mkstemp(prefix="bridge_waypoints.", suffix=".tmp", dir=_REPO_ROOT)
        with os.fdopen(fd, "w", encoding="utf-8") as f:
            f.write(payload_str)
        os.replace(tmp, _WAYPOINTS_FILE)
        _LAST_WAYPOINTS_WRITE = now
    except OSError:
        try:
            with open(_WAYPOINTS_FILE, "w", encoding="utf-8") as f:
                f.write(payload_str)
            _LAST_WAYPOINTS_WRITE = now
        except OSError:
            pass


def _remaining_waypoints_length(
    me_pos: Tuple[float, float],
    enemy_pos: Tuple[float, float],
    waypoints: List[Tuple[float, float]],
    waypoint_idx: int,
) -> float:
    total = 0.0
    prev = me_pos
    for x, y in waypoints[waypoint_idx:]:
        total += math.hypot(x - prev[0], y - prev[1])
        prev = (x, y)
    total += math.hypot(enemy_pos[0] - prev[0], enemy_pos[1] - prev[1])
    return total


def main() -> None:
    env = BridgeEnv()
    env.launch()

    planner: Optional[AStarPlanner] = None
    waypoints: List[Tuple[float, float]] = []
    last_good_waypoints: List[Tuple[float, float]] = []
    waypoint_idx = 0
    frame = 0
    last_pos: Optional[Tuple[float, float]] = None
    stuck_frames = 0
    backoff_frames = 0

    try:
        while True:
            snapshot = env.read_state(wait=True, timeout=2.0)
            if snapshot is None:
                _write_waypoints([], 0, force=True)
                env.set_action(0, 0)
                env.flush_actions()
                last_pos = None
                stuck_frames = 0
                backoff_frames = 0
                continue

            me = _tank(snapshot, 0)
            enemy = _tank(snapshot, 1)
            if me is None or enemy is None:
                _write_waypoints([], 0, force=True)
                env.set_action(0, 0)
                env.flush_actions()
                last_pos = None
                stuck_frames = 0
                backoff_frames = 0
                continue


            if planner is None or frame % 20 == 0 or waypoint_idx >= len(waypoints):
                planner = _build_planner(snapshot)
                planned_waypoints = _plan_waypoints(planner, me, enemy)
                if planned_waypoints is not None:
                    waypoints = planned_waypoints
                    last_good_waypoints = planned_waypoints
                    waypoint_idx = 0
                elif not waypoints:
                    waypoints = last_good_waypoints
                    waypoint_idx = min(waypoint_idx, max(len(waypoints) - 1, 0))

            if waypoint_idx < len(waypoints):
                tx, ty = waypoints[waypoint_idx]
                if (tx - float(me["x"])) ** 2 + (ty - float(me["y"])) ** 2 < 0.08:
                    waypoint_idx += 1

            if not waypoints:
                env.set_flags(0)
                env.flush_actions()
                frame += 1
                time.sleep(0.003)
                continue

            target = waypoints[min(waypoint_idx, len(waypoints) - 1)]
            err = heading_error(float(me["x"]), float(me["y"]), float(me["angle"]), target[0], target[1])
            dist2 = (float(enemy["x"]) - float(me["x"])) ** 2 + (float(enemy["y"]) - float(me["y"])) ** 2
            err_to_shoot = heading_error(float(me["x"]), float(me["y"]), float(me["angle"]), float(enemy["x"]), float(enemy["y"]))
            _write_waypoints(waypoints, waypoint_idx)

            should_shoot = abs(err_to_shoot) <= 0.16 and dist2 < 9.0
            if should_shoot:
                total_waypoint_len = _remaining_waypoints_length(
                    me_pos=(float(me["x"]), float(me["y"])),
                    enemy_pos=(float(enemy["x"]), float(enemy["y"])),
                    waypoints=waypoints,
                    waypoint_idx=waypoint_idx,
                )
                if total_waypoint_len > 1.2 * dist2:
                    should_shoot = False

            aligned = abs(err) <= 0.2
            turning_hard = abs(err) > 1.2
            moving_allowed = aligned or (abs(err) <= 0.3 and dist2 > 9.0)

            pos = (float(me["x"]), float(me["y"]))
            if last_pos is not None:
                dx = pos[0] - last_pos[0]
                dy = pos[1] - last_pos[1]
                moved_enough = (dx * dx + dy * dy) > _STUCK_MOVE_EPS2
            else:
                moved_enough = True

            if moving_allowed and not turning_hard and not moved_enough:
                stuck_frames += 1
            else:
                stuck_frames = 0
            last_pos = pos

            if stuck_frames >= _STUCK_FRAMES:
                backoff_frames = _BACKOFF_FRAMES
                stuck_frames = 0

            if backoff_frames > 0:
                env.set_flags(0, backward=True)
                env.flush_actions()
                backoff_frames -= 1
                frame += 1
                time.sleep(0.001)
                continue

            turn_left = err > _TURN_ON_THRESHOLD
            turn_right = err < -_TURN_ON_THRESHOLD
            if abs(err) < _TURN_OFF_THRESHOLD:
                turn_left = turn_right = False

            env.set_flags(
                0,
                forward=moving_allowed,
                turn_left=turn_left,
                turn_right=turn_right,
                shoot=should_shoot,
            )
            env.flush_actions()

            frame += 1
            time.sleep(0.001)
    except KeyboardInterrupt:
        pass
    finally:
        _write_waypoints([], 0, force=True)
        env.set_action(0, 0)
        env.flush_actions()
        env.close()


if __name__ == "__main__":
    main()
