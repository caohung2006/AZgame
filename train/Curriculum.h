#pragma once
#include "map.h"
#include <string>

// ── Enemy behaviour during training ──────────────────────────────────────────
enum class EnemyType {
  STATIONARY,
  RANDOM,
  RULE_V1,
  RULE_V2,
  RULE_V3,
  SELF_PLAY
};

// ── Curriculum phase ID
// ───────────────────────────────────────────────────────
enum class Phase { PHASE1 = 1, PHASE2, PHASE3, PHASE4, PHASE5 };

struct PhaseConfig {
  Phase phase;
  MapMode mapMode;
  EnemyType enemyType;
  int maxSteps;
  int kSeeds;
  int maxGenerations;
  float promotionThreshold;
  bool itemsEnabled;
  bool portalsEnabled;
  bool shieldsEnabled;
  float bulletLifespan;
  int maxBullets;
  int streakRequired;
  float leagueRate;
  std::string name;
};

/**
 * Curriculum Learning Strategy - Final Tuned for Sniper AI
 */
inline PhaseConfig GetPhaseConfig(Phase phase) {
  switch (phase) {
  case Phase::PHASE1:
    return {
        Phase::PHASE1,         // Phase ID
        MapMode::SPARSE,       // Bản đồ trống, ít tường
        EnemyType::STATIONARY, // Đối thủ: Đứng yên
        1200,                  // Max steps mỗi ván
        5,                     // Số ván đấu mỗi cá thể (kSeeds)
        500,                   // Số thế hệ tối đa
        500.0f,        // Điểm ngưỡng thăng hạng (Hợp lý hơn cho mầm non)
        false,         // Vật phẩm (Items)
        false,         // Cổng dịch chuyển (Portals)
        false,         // Khiên (Shields)
        2.5f,          // Đạn tồn tại trong (giây)
        3,             // Giới hạn đạn trên sân
        10,            // Streak cần thiết (10 thế hệ là đủ chứng minh năng lực)
        0.0f,          // Tỉ lệ trộn (P1 không trộn)
        "Phase1_Basic" // Tên Phase
    };

  case Phase::PHASE2:
    return {Phase::PHASE2,
            MapMode::NORMAL,    // Mê cung Full
            EnemyType::RULE_V1, // Đối thủ: Wanderer (Chỉ đi, không bắn)
            1500,
            6,
            200,
            480.0f,
            false,
            false,
            false,
            2.5f,
            3,
            10,
            0.30f, // 30% trộn STATIONARY
            "Phase2_Wanderer"};

  case Phase::PHASE3:
    return {Phase::PHASE3,
            MapMode::NORMAL,
            EnemyType::RULE_V2, // Đối thủ: Fighter (Bắn nghiệp dư, không khiên)
            1500,
            8,
            250,
            500.0f,
            false,
            false,
            true, // Bắt đầu cho AI dùng Khiên
            3.5f,
            3,
            10,    // 15→10: V2 bắn trả gây variance cao, 10 gen là đủ
            0.40f, // 40% trộn STATIONARY để giữ gene
            "Phase3_Fighter"};

  case Phase::PHASE4:
    return {
        Phase::PHASE4,
        MapMode::NORMAL,
        EnemyType::RULE_V3, // Đối thủ: Sniper Boss (Ngắm chuẩn, có khiên)
        1500,
        10,
        300,
        500.0f, // 550→500: Sparse reward ±1000/ván, ngưỡng cao = không qua nổi
        false,
        false,
        true,
        7.0f,
        5,
        12,    // 20→12: Variance cực cao ở sparse reward
        0.35f, // 35% trộn RULE_V2 để AI ôn bài
        "Phase4_SniperBoss"};

  case Phase::PHASE5:
    return {
        Phase::PHASE5, MapMode::NORMAL,
        EnemyType::SELF_PLAY, // Đối thủ: Chính mình (Self-play)
        1500, 12, 500,
        350.0f, // 650→350: Self-play winrate tối ưu ~60%, 650 là bất khả thi
        false, false, true, 7.0f, 5,
        15,    // 30→15: Self-play variance rất cao, 30 streak = vĩnh viễn không
               // qua
        0.50f, // 50% trộn RULE_V3 để giữ kỷ luật Sniper
        "Phase5_Tournament"};

  default:
    return GetPhaseConfig(Phase::PHASE1);
  }
}
