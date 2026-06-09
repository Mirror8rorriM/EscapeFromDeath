#include "combat_system.h"

#include "utils.h"

#include <chrono>
#include <cmath>
#include <iostream>
#include <thread>

namespace efd {

CombatSystem::CombatSystem(std::vector<CombatPhrase> phrases)
    : phrases_(std::move(phrases)), rng_(std::random_device{}()) {}

const CombatPhrase& CombatSystem::randomPhrase() {
    std::uniform_int_distribution<std::size_t> dist(0, phrases_.size() - 1);
    return phrases_[dist(rng_)];
}

int CombatSystem::calculateIncomingDamage(int baseDamage, bool correctWord, double seconds, double heroMultiplier, double enemyTimeScale) const {
    if (!correctWord) return baseDamage;

    const double effectiveTime = (seconds * enemyTimeScale) / heroMultiplier;
    double damageFactor = 1.0;
    if (effectiveTime <= 1.0) damageFactor = 0.0;
    else if (effectiveTime <= 2.0) damageFactor = 0.4;
    else if (effectiveTime <= 3.0) damageFactor = 0.7;

    return static_cast<int>(std::ceil(baseDamage * damageFactor));
}

bool CombatSystem::fight(Hero& hero, EnemyData enemyTemplate) {
    if (phrases_.empty()) {
        std::cout << "Нет загруженных боевых фраз. Бой невозможен.\n";
        return false;
    }

    int enemyHp = enemyTemplate.hp;
    std::cout << "\nБой начался: " << enemyTemplate.name << "!\n";
    std::cout << "Вводите одно из слов, написанных КАПСОМ. Чем быстрее ввод, тем меньше урон.\n\n";

    while (hero.isAlive() && enemyHp > 0) {
        const CombatPhrase& phrase = randomPhrase();
        std::cout << enemyTemplate.name << ": " << phrase.attackText << "\n";
        std::cout << "Мне нужно ";
        for (std::size_t i = 0; i < phrase.reactions.size(); ++i) {
            if (i > 0) std::cout << ", либо ";
            std::cout << phrase.reactions[i].text;
        }
        std::cout << "\n> " << std::flush;

        const auto start = std::chrono::steady_clock::now();
        std::string answer;
        std::getline(std::cin, answer);
        const auto finish = std::chrono::steady_clock::now();
        const double seconds = std::chrono::duration<double>(finish - start).count();

        answer = toLowerAscii(trim(answer));
        bool correct = false;
        for (const auto& reaction : phrase.reactions) {
            if (toLowerAscii(reaction.word) == answer) {
                correct = true;
                break;
            }
        }

        const int incoming = calculateIncomingDamage(
            enemyTemplate.damage,
            correct,
            seconds,
            hero.agilityTimeMultiplier(),
            enemyTemplate.timeScale
        );

        if (correct && incoming == 0) {
            std::cout << "Идеально! Вы не получили урон.\n";
        } else if (correct) {
            std::cout << "Вы успели среагировать частично. Получено урона: " << incoming << ".\n";
        } else {
            std::cout << "Неверное действие. Удар прошёл полностью. Получено урона: " << incoming << ".\n";
        }
        hero.receiveDamage(incoming);
        if (!hero.isAlive()) break;

        int outgoing = hero.attackDamage();
        if (correct && seconds <= 1.0) {
            outgoing = static_cast<int>(std::ceil(outgoing * 1.5));
            std::cout << "Контратака получилась идеальной! ";
        }
        enemyHp = std::max(0, enemyHp - outgoing);
        std::cout << "Вы ударили противника на " << outgoing << " урона.\n";
        std::cout << "Ваше HP: " << hero.hp() << " | HP врага: " << enemyHp << "\n\n";
    }

    if (hero.isAlive()) {
        std::cout << "Вы победили в бою.\n\n";
        return true;
    }

    std::cout << "Вы проиграли бой и очнулись в лазарете через два дня.\n\n";
    return false;
}

} 
