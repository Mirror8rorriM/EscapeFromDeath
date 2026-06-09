#include "hero.h"

#include <algorithm>
#include <sstream>

namespace efd {

Hero::Hero() {
    healFull();
}

int Hero::hp() const {
    return hp_;
}

int Hero::strengthLevel() const {
    return strengthLevel_;
}

int Hero::agilityLevel() const {
    return agilityLevel_;
}

int Hero::enduranceLevel() const {
    return enduranceLevel_;
}

bool Hero::isAlive() const {
    return hp_ > 0;
}

int Hero::maxHp() const {
    switch (enduranceLevel_) {
        case 1: return 100;
        case 2: return 110;
        default: return 120;
    }
}

int Hero::attackDamage() const {
    switch (strengthLevel_) {
        case 1: return 20;
        case 2: return 25;
        default: return 30;
    }
}

double Hero::agilityTimeMultiplier() const {
    switch (agilityLevel_) {
        case 1: return 1.0;
        case 2: return 1.25;
        default: return 1.5;
    }
}

void Hero::receiveDamage(int damage) {
    hp_ = std::max(0, hp_ - std::max(0, damage));
}

void Hero::healFull() {
    hp_ = maxHp();
}

bool Hero::train(StatType stat) {
    int* target = nullptr;
    switch (stat) {
        case StatType::Strength: target = &strengthLevel_; break;
        case StatType::Agility: target = &agilityLevel_; break;
        case StatType::Endurance: target = &enduranceLevel_; break;
    }

    if (*target >= 3) return false;
    ++(*target);
    if (stat == StatType::Endurance) healFull();
    return true;
}

void Hero::setStats(int strength, int agility, int endurance, int hp) {
    strengthLevel_ = std::clamp(strength, 1, 3);
    agilityLevel_ = std::clamp(agility, 1, 3);
    enduranceLevel_ = std::clamp(endurance, 1, 3);
    hp_ = std::clamp(hp, 0, maxHp());
}

std::string Hero::statsLine() const {
    std::ostringstream out;
    out << "HP " << hp_ << "/" << maxHp()
        << " | Сила " << strengthLevel_
        << " | Ловкость " << agilityLevel_
        << " | Выносливость " << enduranceLevel_;
    return out.str();
}

} 
