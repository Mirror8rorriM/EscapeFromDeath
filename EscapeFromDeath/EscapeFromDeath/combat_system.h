#pragma once

#include <map>
#include <random>
#include <set>
#include <string>
#include <vector>

#include "hero.h"
#include "models.h"

namespace efd {

class CombatSystem {
 public:
  explicit CombatSystem(std::vector<CombatPhrase> phrases,
                        std::map<std::string, std::string> texts = {});

  bool fight(Hero& hero, EnemyData enemyTemplate);

 private:
  const CombatPhrase& randomPhrase();
  int calculateIncomingDamage(int baseDamage, bool correctWord, double seconds,
                              double heroMultiplier,
                              double enemyTimeScale) const;
  std::string text(const std::string& key) const;

  std::vector<CombatPhrase> phrases_;
  std::map<std::string, std::string> texts_;
  std::mt19937 rng_;
};

}  // namespace efd
