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
        10,     // Số ván đấu mỗi cá thể (kSeeds) — 5→10 giảm variance map
        500,    // Số thế hệ tối đa
        500.0f, // Điểm ngưỡng thăng hạng (Hợp lý hơn cho mầm non)
        false,  // Vật phẩm (Items)
        false,  // Cổng dịch chuyển (Portals)
        false,  // Khiên (Shields)
        2.5f,   // Đạn tồn tại trong (giây)
        3,      // Giới hạn đạn trên sân
        10,     // Streak cần thiết (10 thế hệ là đủ chứng minh năng lực)
        0.0f,   // Tỉ lệ trộn (P1 không trộn)
        "Phase1_Basic" // Tên Phase
    };

  case Phase::PHASE2:
    return {
        Phase::PHASE2,
        MapMode::NORMAL,    // Mê cung Full
        EnemyType::RULE_V1, // Đối thủ: Wanderer (Chỉ đi, không bắn)
        1500,
        10,  // 6→10: NORMAL map variance cao, cần nhiều seed hơn
        350, // 200→350: nhảy SPARSE→NORMAL + STAT→Wanderer cần nhiều gen hơn
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
    return {
        Phase::PHASE3,
        MapMode::NORMAL,
        EnemyType::RULE_V2, // Đối thủ: Fighter (Bắn nghiệp dư, không khiên)
        1500,
        10, // 8→10: Fighter bắn trả = variance cao, cần nhiều seed
        350,    // 250→350: thêm enemy bắn trả + shield = 2 kỹ năng mới
        480.0f, // Engagement mode không có move reward → step total thấp hơn
        false,
        false,
        true, // Bắt đầu cho AI dùng Khiên
        3.5f,
        3,
        10,    // 15→10: V2 bắn trả gây variance cao, 10 gen là đủ
        0.25f, // 40→25%: trộn V1 (không phải STAT) để ôn mê cung
        "Phase3_Fighter"};

  case Phase::PHASE4:
    return {Phase::PHASE4, MapMode::NORMAL,
            EnemyType::RULE_V3, // Đối thủ: Sniper Boss (Ngắm chuẩn, có khiên)
            1500, 10, 300,
            400.0f, // V3 cực mạnh, cần 70% winrate để đạt 400 (500 cần 80% =
                    // quá khó)
            false, false, true, 7.0f, 5,
            10,    // Sparse reward variance cao, 10 gen là đủ
            0.35f, // 35% trộn RULE_V2 để AI ôn bài
            "Phase4_SniperBoss"};

  case Phase::PHASE5:
    return {Phase::PHASE5, MapMode::NORMAL,
            EnemyType::SELF_PLAY, // Đối thủ: Chính mình (Self-play)
            1500, 12, 500,
            300.0f, // Self-play 50/50 chỉ cho ~365, threshold 300 = an toàn cho
                    // streak
            false, false, true, 7.0f, 5,
            10,    // Self-play variance cực cao, 10 gen là đủ
            0.50f, // 50% trộn RULE_V3 để giữ kỷ luật Sniper
            "Phase5_Tournament"};

  default:
    return GetPhaseConfig(Phase::PHASE1);
  }
}
