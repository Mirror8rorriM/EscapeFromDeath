#pragma once

#include <string>

namespace efd {

class Creatures {
public:
    Creatures();
    Creatures(std::string name, int hp, int damage);
    virtual ~Creatures();

    const std::string& name() const;
    int hp() const;
    int maxHp() const;
    int damage() const;
    bool isAlive() const;

    void receiveDamage(int amount);
    void healFull();

protected:
    std::string name_;
    int hp_ = 1;
    int maxHp_ = 1;
    int damage_ = 1;
};

} 
