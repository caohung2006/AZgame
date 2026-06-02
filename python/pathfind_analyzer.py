#AI generated

import os
import subprocess
import time
import pandas as pd
import sys
import argparse
import signal

_REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
_EXE_PATH = os.path.join(_REPO_ROOT, "build", "AZgameBridge.exe") # Adjust path to your exe
_LOG_PATH = os.path.join(_REPO_ROOT, "run", "telemetry.csv")

class BatchAnalyzer:
    def __init__(self, algos, heuristics, maps_count):
        self.algos = algos
        self.heuristics = heuristics
        self.maps_count = maps_count

    def run_benchmark(self):
        if os.path.exists(_LOG_PATH): os.remove(_LOG_PATH)

        for algo in self.algos:
            for h in (self.heuristics if algo == "astar" else ["uninformed"]):
                print(f"🚀 Benchmarking: {algo} | {h}")

                # 1. Start the C++ Game (Fresh Start = First Map in Manifest)
                cpp_proc = subprocess.Popen([_EXE_PATH], cwd=_REPO_ROOT)
                time.sleep(2) # Give C++ time to open window and create run/ folder

                # 2. Start the Python Controller
                env_vars = os.environ.copy()
                env_vars["AZ_ALGO"] = algo
                env_vars["AZ_HEURISTIC"] = h
                env_vars["AZ_LOG"] = _LOG_PATH
                
                ctrl_proc = subprocess.Popen(
                    [sys.executable, os.path.join(_REPO_ROOT, "python", "pathfind_controller.py")],
                    env=env_vars
                )

                # 3. Wait for the run to complete the requested number of maps
                # We calculate time: approx 15 seconds per map
                total_wait = self.maps_count * 15 
                print(f"   > Waiting {total_wait}s for {self.maps_count} maps...")
                
                try:
                    ctrl_proc.wait(timeout=total_wait)
                except subprocess.TimeoutExpired:
                    print(f"   > Time up for {algo}. Cleaning up...")
                
                # 4. Kill both so the next algo starts with Map #1 again
                ctrl_proc.terminate()
                cpp_proc.terminate()
                time.sleep(1) # Cool down

        self.generate_report()


    def generate_report(self):
        if not os.path.exists(_LOG_PATH):
            print("No telemetry data found.")
            return

        cols = ["timestamp", "algo", "heuristic", "success", "time_ms", "path_len"]
        df = pd.read_csv(_LOG_PATH, names=cols)

        # Aggregated Stats
        df["time_ms"] = pd.to_numeric(df["time_ms"])
        df["path_len"] = pd.to_numeric(df["path_len"])
        df["success"] = df["success"].astype(bool)

        summary = df.groupby(["algo", "heuristic"]).agg(
            avg_ms=('time_ms', 'mean'),
            median_ms=('time_ms', 'median'),
            std_ms=('time_ms', 'std'),
            max_ms=('time_ms', 'max'),
            avg_path_len=('path_len', 'mean'),
            success_rate=('success', 'mean'),
            sample_count=('timestamp', 'count')
        ).reset_index()
        
        summary.to_csv(os.path.join(_REPO_ROOT, "run", "summary_report.csv"))
        print("\n" + "="*80)
        print(f"{'ALGORITHM':<15} | {'HEURISTIC':<12} | {'AVG MS':<8} | {'SUCCESS':<8} | {'SAMPLES':<5}")
        print("-" * 80)
        for _, row in summary.iterrows():
            print(f"{row['algo']:<15} | {row['heuristic']:<12} | {row['avg_ms']:<8.3f} | {row['success_rate']*100:<7.1f}% | {int(row['sample_count']):<5}")
        print("="*80)
        print(f"Full analytics saved to: run/summary_report.csv")

if __name__ == "__main__":
    import sys
    parser = argparse.ArgumentParser()
    parser.add_argument("--algos", nargs="+", default=["astar", "dijkstra", "bfs"])
    parser.add_argument("--heuristics", nargs="+", default=["manhattan", "euclidean"])
    parser.add_argument("--maps", type=int, default=5, help="Number of maps in your manifest")
    args = parser.parse_args()

    # NOTE: You must have the C++ Game running already, or launch it here
    shell = BatchAnalyzer(args.algos, args.heuristics, args.maps)
    shell.run_benchmark()