#include "../neat/Population.h"
#include "AZRandom.h"
#include "Curriculum.h"
#include "Fitness.h"
#include "Observation.h"
#include "RuleEnemy.h"
#include "game.h"
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <memory>
#include <omp.h>
#include <string>
#include <vector>

// ─────────────────────────────────────────────────────────────────────────────
//  Constants
// ─────────────────────────────────────────────────────────────────────────────
constexpr float DT = 1.0f / 60.0f;
constexpr int NUM_INPUTS = 36;
constexpr int NUM_OUTPUTS = 6;
constexpr int ASTAR_REFRESH_INTERVAL = 3;

// ─────────────────────────────────────────────────────────────────────────────
//  Helpers
// ─────────────────────────────────────────────────────────────────────────────
static TankActions OutputToActions(const std::vector<float> &out) {
  TankActions a;
  a.forward = out[0] > 0.5f && out[0] > out[1];
  a.backward = out[1] > 0.5f && out[1] > out[0];
  a.turnLeft = out[2] > 0.5f && out[2] > out[3];
  a.turnRight = out[3] > 0.5f && out[3] > out[2];
  a.shoot = out[4] > 0.5f;
  a.shield = out[5] > 0.5f;
  return a;
}

// ─────────────────────────────────────────────────────────────────────────────
//  Episode runner
// ─────────────────────────────────────────────────────────────────────────────
static float RunEpisode(Network &agentNet, const PhaseConfig &cfg, int seed,
                        Network *enemyNet = nullptr) {
  AZ::SRand(seed);
  Game game;
  game.numPlayers = 2;
  game.mapMode = cfg.mapMode;
  game.itemsEnabled = cfg.itemsEnabled;
  game.portalsEnabled = cfg.portalsEnabled;
  game.shieldsEnabled = cfg.shieldsEnabled;
  game.bulletLifespan = cfg.bulletLifespan;
  game.maxBullets = cfg.maxBullets;

  game.ResetMatch();

  float fitness = 0.0f;
  float distToEnemy = 100.0f;
  if (game.tanks.size() >= 2) {
    b2Vec2 p1 = game.tanks[0]->body->GetPosition();
    b2Vec2 p2 = game.tanks[1]->body->GetPosition();
    distToEnemy = (p2 - p1).Length();
  }

  int astarCounter = 0;
  b2Vec2 astarWaypoint = {0, 0};
  std::vector<float> agentObs(36, 0.0f);
  std::vector<float> enemyObs(36, 0.0f);
  std::vector<float> agentOut, enemyOut;
  int scoresBefore[4] = {0, 0, 0, 0};
  int actualSteps = cfg.maxSteps;
  bool agentDidShoot = false;

  // ─── LỰA CHỌN ĐỐI THỦ (League Training Logic) ───
  EnemyType currentEnemy = cfg.enemyType;
  if (cfg.leagueRate > 0.0f) {
    if ((AZ::Rand() % 100) < (int)(cfg.leagueRate * 100)) {
      if (cfg.phase == Phase::PHASE4)
        currentEnemy = EnemyType::RULE_V2; // Trộn V2 để AI ôn bài "nghiệp dư"
      else if (cfg.phase == Phase::PHASE5)
        currentEnemy = EnemyType::RULE_V3; // Trộn V3 để AI giữ chuẩn Sniper
      else
        currentEnemy = EnemyType::STATIONARY; // P1,2,3: Trộn bao cát đứng yên
    }
  }

  for (int step = 0; step < cfg.maxSteps; step++) {
    if (astarCounter <= 0 && game.tanks.size() >= 2) {
      astarWaypoint =
          game.map.GetNextWaypoint(game.tanks[0]->body->GetPosition(),
                                   game.tanks[1]->body->GetPosition());
      astarCounter = ASTAR_REFRESH_INTERVAL;
    }
    astarCounter--;

    GetObservation(game, 0, astarWaypoint, agentObs);
    agentOut.clear();
    agentNet.Activate(agentObs, agentOut);

    std::vector<TankActions> actions(game.numPlayers);
    actions[0] = OutputToActions(agentOut);
    if (actions[0].shoot)
      agentDidShoot = true;

    switch (currentEnemy) {
    case EnemyType::STATIONARY:
      actions[1] = {};
      break;
    case EnemyType::RANDOM: {
      TankActions ra;
      ra.forward = (AZ::Rand() % 3 == 0);
      ra.turnLeft = (AZ::Rand() % 4 == 0);
      ra.turnRight = (AZ::Rand() % 4 == 0);
      // [VÁ LỖI TỰ SÁT]: Random bot xả đạn quá nhiều (cứ 8 frame 1 viên)
      // Giảm tần suất xuống cỡ 1 viên / 1.5 giây để tránh nó tự chết quá nhanh
      ra.shoot = (AZ::Rand() % 90 == 0);
      // [VÁ LỖI KHIÊN]: Thi thoảng bật khiên để agent phải mệt mỏi hơn
      ra.shield = (AZ::Rand() % 150 == 0);
      actions[1] = ra;
      break;
    }
    case EnemyType::RULE_V1:
      actions[1] = GetRuleEnemyAction(game, 1, RuleVariant::V1);
      break;
    case EnemyType::RULE_V2:
      actions[1] = GetRuleEnemyAction(game, 1, RuleVariant::V2);
      break;
    case EnemyType::RULE_V3:
      actions[1] = GetRuleEnemyAction(game, 1, RuleVariant::V3);
      break;
    case EnemyType::SELF_PLAY:
      if (enemyNet && game.tanks.size() >= 2) {
        b2Vec2 enemyWp =
            game.map.GetNextWaypoint(game.tanks[1]->body->GetPosition(),
                                     game.tanks[0]->body->GetPosition());
        GetObservation(game, 1, enemyWp, enemyObs);
        enemyOut.clear();
        enemyNet->Activate(enemyObs, enemyOut);
        actions[1] = OutputToActions(enemyOut);
      } else {
        actions[1] = {};
      }
      break;
    }

    game.Update(actions, DT);

    if (game.needsRestart) {
      actualSteps = step + 1;
      break;
    }

    bool canSee = false;
    if (game.tanks.size() >= 2) {
      canSee = CheckLineOfSight(game, game.tanks[0]->body->GetPosition(),
                                game.tanks[1]->body->GetPosition());
    }
    float prev = distToEnemy;
    fitness += ComputeStepReward(game, 0, canSee, astarWaypoint, prev,
                                 distToEnemy, actions[0], agentObs, cfg.phase);
  }

  fitness += ComputeEndBonus(game, 0, scoresBefore, actualSteps, cfg.maxSteps,
                             agentDidShoot, cfg.phase);
  return fitness;
}

// ─────────────────────────────────────────────────────────────────────────────
static float EvaluateGenome(Genome &genome, const PhaseConfig &cfg,
                            const std::vector<int> &seeds,
                            const Genome *frozenEnemy = nullptr) {
  Network agentNet = Network::FromGenome(genome);
  std::unique_ptr<Network> enemyNet;
  if (frozenEnemy && cfg.enemyType == EnemyType::SELF_PLAY) {
    enemyNet = std::make_unique<Network>(Network::FromGenome(*frozenEnemy));
  }
  float total = 0.0f;
  for (int seed : seeds) {
    total += RunEpisode(agentNet, cfg, seed, enemyNet.get());
  }
  float avg = total / (float)seeds.size();
  avg -= 0.005f * (float)genome.conns.size();
  return avg;
}

// ─────────────────────────────────────────────────────────────────────────────
static bool RunPhase(Population &pop, const PhaseConfig &cfg,
                     const Genome *frozenEnemy, const std::string &outDir) {
  printf(
      "\n╔══════════════════════════════════════════════════════════════╗\n");
  printf("║  %-60s║\n", cfg.name.c_str());
  printf("║  Generations: %-5d  Steps/ep: %-6d  Seeds: %-5d         ║\n",
         cfg.maxGenerations, cfg.maxSteps, cfg.kSeeds);
  printf("╚══════════════════════════════════════════════════════════════╝\n");

  std::string logPath = outDir + "/" + cfg.name + "_log.csv";
  FILE *logFile = fopen(logPath.c_str(), "w");
  if (logFile)
    fprintf(logFile, "gen,best,avg,species\n");

  int streak = 0;
  bool promoted = false;
  for (int gen = 0; gen < cfg.maxGenerations; gen++) {
    std::vector<int> seeds;
    seeds.reserve(cfg.kSeeds);
    for (int k = 0; k < cfg.kSeeds; k++)
      seeds.push_back(42000 + gen * 100 + k);

    pop.EvaluateAll([&](Genome &g) -> float {
      return EvaluateGenome(g, cfg, seeds, frozenEnemy);
    });

    float best = -1e30f, avg = 0.0f;
    for (auto &g : pop.genomes) {
      best = std::max(best, g.fitness);
      avg += g.fitness;
    }
    avg /= (float)pop.genomes.size();
    pop.Evolve();
    pop.PrintStats();

    if (logFile) {
      fprintf(logFile, "%d,%.4f,%.4f,%d\n", gen + 1, best, avg,
              (int)pop.species.size());
      fflush(logFile);
    }
    if ((gen + 1) % 10 == 0) {
      std::string ckpt =
          outDir + "/" + cfg.name + "_gen" + std::to_string(gen + 1) + ".bin";
      if (pop.SaveBest(ckpt))
        printf("      [Checkpoint: %s]\n", ckpt.c_str());
    }
    if (cfg.promotionThreshold > 0.0f && best >= cfg.promotionThreshold) {
      // Ép train ít nhất 15% số Generation tối đa để đảm bảo sự ổn định
      int minGen = std::max(10, (int)(cfg.maxGenerations * 0.15f));
      if (gen >= minGen) {
        streak++;
        printf("  -> [Streak: %d/%d] Best fitness %.1f >= threshold!\n", streak,
               cfg.streakRequired, best);
        if (streak >= cfg.streakRequired) {
          printf("\n  ✅ Promotion threshold %.1f maintained for %d gens! Next phase.\n",
                 cfg.promotionThreshold, cfg.streakRequired);
          promoted = true;
          break;
        }
      } else {
        printf("  -> Best: %.1f >= threshold. Tiếp tục ép train (Gen: %d/%d)\n",
               best, gen, minGen);
      }
    } else {
      if (streak > 0)
        printf("  -> Streak broken!\n");
      streak = 0;
    }
  }
  pop.SaveBest(outDir + "/" + cfg.name + "_final.bin");
  if (logFile)
    fclose(logFile);
  return promoted;

// ─────────────────────────────────────────────────────────────────────────────
int main(int argc, char *argv[]) {
  srand((unsigned int)time(NULL));
  namespace fs = std::filesystem;
  if (!fs::exists("agents"))
    fs::create_directory("agents");

  int numThreads = 8;
  if (argc >= 2) {
    numThreads = std::atoi(argv[1]);
    if (numThreads <= 0)
      numThreads = 1;
  }
  omp_set_num_threads(numThreads);

  printf("\n  AZgame NEAT Trainer v2.1 (Smart Resume)\n\n");
  printf("  Max threads (OpenMP): %d\n", omp_get_max_threads());
  printf("  Population : %d\n", NeatCfg::POP_SIZE);
  printf("  Threads    : %d\n", numThreads);
  printf("  Output dir : agents/\n\n");

  Population pop(NUM_INPUTS, NUM_OUTPUTS);
  Genome loadedG;
  Genome *lastBest = nullptr;
  int startPhase = 1;

  if (argc >= 3) {
    std::string ckptPath = argv[2];
    loadedG = Genome::Load(ckptPath);
    if (!loadedG.nodes.empty()) {
      int maxN = 0, maxC = 0;
      for (auto &n : loadedG.nodes)
        maxN = std::max(maxN, n.id);
      for (auto &c : loadedG.conns)
        maxC = std::max(maxC, c.innovation);
      InnovationTracker::Get().UpdateCounters(maxN, maxC);

      for (auto &gen : pop.genomes)
        gen = loadedG;
      lastBest = new Genome(loadedG);
      printf("  [LOADED] Continuing from: %s (MaxID: %d)\n", ckptPath.c_str(),
             maxN);

      std::string pathLower = ckptPath;
      for (auto &c : pathLower)
        c = (char)std::tolower(c);

      if (pathLower.find("phase2") != std::string::npos)
        startPhase = 2;
      else if (pathLower.find("phase3") != std::string::npos)
        startPhase = 3;
      else if (pathLower.find("phase4") != std::string::npos)
        startPhase = 4;
      else if (pathLower.find("phase5") != std::string::npos)
        startPhase = 5;

      if (startPhase > 1)
        printf("  [AUTO-SKIP] Jumping to Phase %d\n", startPhase);
    } else {
      printf("\n  [FATAL ERROR] Could not load checkpoint: %s\n\n",
             ckptPath.c_str());
      exit(1);
    }
  }

  for (int p = startPhase; p <= 5; p++) {
    PhaseConfig cfg = GetPhaseConfig(static_cast<Phase>(p));
    bool graduated = RunPhase(pop, cfg, lastBest, "agents");

    if (lastBest)
      delete lastBest;
    lastBest = new Genome();

    float bFit = -1e30f;
    int bIdx = 0;
    for (int i = 0; i < (int)pop.genomes.size(); i++) {
      if (pop.genomes[i].fitness > bFit) {
        bFit = pop.genomes[i].fitness;
        bIdx = i;
      }
    }
    *lastBest = pop.genomes[bIdx];
    printf("\n      >>> Phase %d Completed. Best Fitness: %.2f\n\n", p, bFit);

    if (!graduated) {
      printf("\n  ❌ Phase %d: KHÔNG ĐẠT STREAK sau %d generations!\n", p, cfg.maxGenerations);
      printf("  => Training DỪNG LẠI. Genome tốt nhất đã lưu tại: agents/%s_final.bin\n", cfg.name.c_str());
      printf("  => Bạn có thể:\n");
      printf("     1. Chạy lại: ./aztrain.exe 4 agents/%s_final.bin\n", cfg.name.c_str());
      printf("     2. Điều chỉnh Curriculum.h rồi train lại\n");
      break;
    }
  }

  if (lastBest)
    delete lastBest;
  return 0;
}