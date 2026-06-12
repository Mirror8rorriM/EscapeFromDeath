#include "combat_system.h"

#include <chrono>
#include <cmath>
#include <iostream>
#include <thread>

#include "console_encoding.h"
#include "utils.h"

namespace efd {

CombatSystem::CombatSystem(std::vector<CombatPhrase> phrases,
                           std::map<std::string, std::string> texts)
    : phrases_(std::move(phrases)),
      texts_(std::move(texts)),
      rng_(std::random_device{}()) {}

std::string CombatSystem::text(const std::string& key) const {
  auto it = texts_.find(key);
  if (it != texts_.end()) return it->second;
  return key;
}

const CombatPhrase& CombatSystem::randomPhrase() {
  std::uniform_int_distribution<std::size_t> dist(0, phrases_.size() - 1);
  return phrases_[dist(rng_)];
}

int CombatSystem::calculateIncomingDamage(int baseDamage, bool correctWord,
                                          double seconds, double heroMultiplier,
                                          double enemyTimeScale) const {
  if (!correctWord) return baseDamage;

  const double effectiveTime = (seconds * enemyTimeScale) / heroMultiplier;
  double damageFactor = 1.0;
  if (effectiveTime <= 2.0)
    damageFactor = 0.0;
  else if (effectiveTime <= 4.0)
    damageFactor = 0.4;
  else if (effectiveTime <= 6.0)
    damageFactor = 0.7;

  return static_cast<int>(std::ceil(baseDamage * damageFactor));
}

bool CombatSystem::fight(Hero& hero, EnemyData enemyTemplate) {
  if (phrases_.empty()) {
    std::cout << text("combat.no_phrases") << "\n";
    return false;
  }

  int enemyHp = enemyTemplate.hp;
  std::cout << "\n"
            << text("combat.started_prefix") << enemyTemplate.name
            << text("combat.started_suffix") << "\n";
  std::cout << text("combat.instructions") << "\n\n";

  while (hero.isAlive() && enemyHp > 0) {
    const CombatPhrase& phrase = randomPhrase();
    std::cout << enemyTemplate.name << ": " << phrase.attackText << "\n";
    std::cout << text("combat.reaction_prompt");
    for (std::size_t i = 0; i < phrase.reactions.size(); ++i) {
      if (i > 0) std::cout << text("combat.reaction_or");
      std::cout << phrase.reactions[i].text;
    }
    std::cout << "\n> " << std::flush;

    const auto start = std::chrono::steady_clock::now();
    std::string answer;
    if (!readLineUtf8(answer)) {
      std::cout << "\n" << text("combat.input_closed") << "\n";
      return false;
    }
    const auto finish = std::chrono::steady_clock::now();
    const double seconds =
        std::chrono::duration<double>(finish - start).count();

    answer = toLowerUtf8(trim(answer));
    bool correct = false;
    for (const auto& reaction : phrase.reactions) {
      if (toLowerUtf8(reaction.word) == answer) {
        correct = true;
        break;
      }
    }

    const int incoming = calculateIncomingDamage(
        enemyTemplate.damage, correct, seconds, hero.agilityTimeMultiplier(),
        enemyTemplate.timeScale);

    if (correct && incoming == 0) {
      std::cout << text("combat.perfect_defense") << "\n";
    } else if (correct) {
      std::cout << text("combat.partial_defense_prefix") << incoming
                << text("combat.sentence_suffix") << "\n";
    } else {
      std::cout << text("combat.wrong_action_prefix") << incoming
                << text("combat.sentence_suffix") << "\n";
    }
    hero.receiveDamage(incoming);
    if (!hero.isAlive()) break;

    int outgoing = hero.attackDamage();
    if (correct && seconds <= 1.0) {
      outgoing = static_cast<int>(std::ceil(outgoing * 1.5));
      std::cout << text("combat.perfect_counter_prefix");
    }
    enemyHp = std::max(0, enemyHp - outgoing);
    std::cout << text("combat.hero_hit_prefix") << outgoing
              << text("combat.hero_hit_suffix") << "\n";
    std::cout << text("combat.hp_prefix") << hero.hp()
              << text("combat.enemy_hp_prefix") << enemyHp << "\n\n";
  }

  if (hero.isAlive()) {
    std::cout << text("combat.win") << "\n\n";
    return true;
  }

  std::cout << text("combat.lose") << "\n\n";
  return false;
}

}  
