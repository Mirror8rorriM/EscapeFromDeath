#pragma once

#include "hero.h"
#include "models.h"

#include <random>
#include <vector>
#include <set>

namespace efd {

class CombatSystem {
public:
    explicit CombatSystem(std::vector<CombatPhrase> phrases);

    bool fight(Hero& hero, EnemyData enemyTemplate);

private:
    const CombatPhrase& randomPhrase();
    int calculateIncomingDamage(int baseDamage, bool correctWord, double seconds, double heroMultiplier, double enemyTimeScale) const;

    std::vector<CombatPhrase> phrases_;
    std::mt19937 rng_;
};

} 
