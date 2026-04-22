from __future__ import annotations

from dataclasses import dataclass
import math
import heapq
from typing import Dict, Iterable, List, Optional, Sequence, Tuple


GridPos = Tuple[int, int]
ActionList = List[int]


@dataclass
class PathResult:
    path: List[GridPos]
    reached_goal: bool


class AStarPlanner:
    """A* planner scaffold for AZgame.

    Fill in the algorithm here and keep the environment wrapper focused on
    stepping the game. This file is intentionally separate so you can iterate
    on pathfinding without touching the pybind layer.
    """

    def __init__(
        self,
        grid: Sequence[Sequence[int]],
        world_origin: Tuple[float, float] = (0.0, 0.0),
        cell_size: Tuple[float, float] = (1.0, 1.0),
    ) -> None:
        self.grid = [list(row) for row in grid]
        self.height = len(self.grid)
        self.width = len(self.grid[0]) if self.height else 0
        self.world_origin_x = float(world_origin[0])
        self.world_origin_y = float(world_origin[1])
        self.cell_width = float(cell_size[0])
        self.cell_height = float(cell_size[1])

    @classmethod
    def from_wall_map(cls, wall_grid: Sequence[Sequence[int]]) -> "AStarPlanner":
        return cls(wall_grid)

    @classmethod
    def from_world_geometry(
        cls,
        world_width: float,
        world_height: float,
        wall_aabbs: Sequence[Tuple[float, float, float, float]],
        grid_width: int = 96,
        grid_height: int = 72,
        inflation_radius: float = 0.0,
        world_origin: Tuple[float, float] = (0.0, 0.0),
    ) -> "AStarPlanner":
        """Build an occupancy grid from world-space wall AABBs.

        Grid encoding:
        - 0: walkable
        - 1: blocked
        """
        if world_width <= 0 or world_height <= 0:
            raise ValueError("world_width and world_height must be positive")
        if grid_width <= 0 or grid_height <= 0:
            raise ValueError("grid_width and grid_height must be positive")
        if inflation_radius < 0:
            raise ValueError("inflation_radius must be >= 0")

        cell_width = world_width / float(grid_width)
        cell_height = world_height / float(grid_height)

        grid = [[0 for _ in range(grid_width)] for _ in range(grid_height)]

        origin_x, origin_y = world_origin

        for min_x, min_y, max_x, max_y in wall_aabbs:
            inf_min_x = min_x - inflation_radius
            inf_min_y = min_y - inflation_radius
            inf_max_x = max_x + inflation_radius
            inf_max_y = max_y + inflation_radius

            col_start = int(math.floor((inf_min_x - origin_x) / cell_width))
            col_end = int(math.floor((inf_max_x - origin_x) / cell_width))
            row_start = int(math.floor((inf_min_y - origin_y) / cell_height))
            row_end = int(math.floor((inf_max_y - origin_y) / cell_height))

            col_start = max(0, min(grid_width - 1, col_start))
            col_end = max(0, min(grid_width - 1, col_end))
            row_start = max(0, min(grid_height - 1, row_start))
            row_end = max(0, min(grid_height - 1, row_end))

            for row in range(row_start, row_end + 1):
                for col in range(col_start, col_end + 1):
                    grid[row][col] = 1

        return cls(
            grid,
            world_origin=(origin_x, origin_y),
            cell_size=(cell_width, cell_height),
        )

    def world_to_grid(self, x: float, y: float) -> GridPos:
        """Convert world-space coordinates to (row, col) grid index."""
        col = int(math.floor((x - self.world_origin_x) / self.cell_width))
        row = int(math.floor((y - self.world_origin_y) / self.cell_height))
        row = max(0, min(self.height - 1, row))
        col = max(0, min(self.width - 1, col))
        return (row, col)

    def is_walkable(self, cell: GridPos) -> bool:
        row, col = cell
        if row < 0 or row >= self.height or col < 0 or col >= self.width:
            return False
        return self.grid[row][col] == 0

    def grid_to_world(self, cell: GridPos) -> Tuple[float, float]:
        """Return world-space center point of a (row, col) cell."""
        row, col = cell
        x = self.world_origin_x + (col + 0.5) * self.cell_width
        y = self.world_origin_y + (row + 0.5) * self.cell_height
        return (x, y)

    def heuristic_manhattan(self, a: GridPos, b: GridPos) -> float:
        return abs(a[0] - b[0]) + abs(a[1] - b[1])
    
    def heuristic_euclidean(self, a: GridPos, b: GridPos) -> float:
        return ((a[0] - b[0]) ** 2 + (a[1] - b[1]) ** 2) ** 0.5
    
    def heuristic_chebyshev(self, a: GridPos, b: GridPos) -> float:
        return max(abs(a[0] - b[0]), abs(a[1] - b[1])) 

    def neighbors(self, node: GridPos) -> Iterable[(GridPos, float)]:
        for dx in [-1, 0, 1]:
            for dy in [-1, 0, 1]:
                if dx == 0 and dy == 0:
                    continue
                if abs(dx) + abs(dy) == 2:
                    cost = 1.4142135623731 
                    if self.grid[node[0] + dx][node[1]] == 1 or self.grid[node[0]][node[1] + dy] == 1:
                        continue
                else:
                    cost = 1.0
                
                x, y = node[0] + dx, node[1] + dy
                if 0 <= x < self.height and 0 <= y < self.width:
                    if self.grid[x][y] == 0:  
                        yield ((x, y), cost)  

    def find_path(self, start: GridPos, goal: GridPos) -> PathResult:
        if not self.is_walkable(start) or not self.is_walkable(goal):
            return PathResult(path=[], reached_goal=False)

        frontier = []
        heapq.heappush(frontier, (0, start))
        came_from: Dict[GridPos, Optional[GridPos]] = {start: None}
        cost_so_far: Dict[GridPos, float] = {start: 0}
        while frontier:
            current = heapq.heappop(frontier)[1]
            if current == goal:
                break
            for next_node, cost in self.neighbors(current):
                new_cost = cost_so_far[current] + cost
                if next_node not in cost_so_far or new_cost < cost_so_far[next_node]:
                    cost_so_far[next_node] = new_cost
                    priority = new_cost + self.heuristic_manhattan(next_node, goal)
                    heapq.heappush(frontier, (priority, next_node))
                    came_from[next_node] = current

        if goal not in came_from:
            return PathResult(path=[], reached_goal=False)

        path = []
        curr = goal
        while curr is not None:
            path.append(curr)
            curr = came_from[curr]
        path.reverse()
        return PathResult(path=path, reached_goal=True)

    def path_to_actions(self, path: Sequence[GridPos]) -> ActionList:
        actions = []
        for i in range(1, len(path)):
            dx = path[i][0] - path[i-1][0]
            dy = path[i][1] - path[i-1][1]
            if dx == -1 and dy == 0:
                actions.append(0)  
            elif dx == 1 and dy == 0:
                actions.append(1)  
            elif dx == 0 and dy == -1:
                actions.append(2)  
            elif dx == 0 and dy == 1:
                actions.append(3)  
            elif dx == -1 and dy == -1:
                actions.append(4)  
            elif dx == -1 and dy == 1:
                actions.append(5)  
            elif dx == 1 and dy == -1:
                actions.append(6)  
            elif dx == 1 and dy == 1:
                actions.append(7)  
        return actions

    def plan(self, start: GridPos, goal: GridPos) -> PathResult:
        return self.find_path(start, goal)
