#pragma once

#include <string>

namespace efd {

enum class StatType {
    Strength,
    Agility,
    Endurance
};

class Hero {
public:
    Hero();

    int hp() const;
    int strengthLevel() const;
    int agilityLevel() const;
    int enduranceLevel() const;

    int maxHp() const;
    int attackDamage() const;
    double agilityTimeMultiplier() const;

    void receiveDamage(int damage);
    void healFull();
    bool isAlive() const;

    bool train(StatType stat);
    void setStats(int strength, int agility, int endurance, int hp);

    std::string statsLine() const;

private:
    int hp_ = 100;
    int strengthLevel_ = 1;
    int agilityLevel_ = 1;
    int enduranceLevel_ = 1;
};

} 
