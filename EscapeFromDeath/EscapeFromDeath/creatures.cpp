#include "creatures.h"

#include <utility>

namespace efd {

Creatures::Creatures() : name_("Безымянный") {}

Creatures::~Creatures() = default;

const std::string& Creatures::name() const {
    return name_;
}

int Creatures::hp() const {
    return hp_;
}

int Creatures::maxHp() const {
    return maxHp_;
}

int Creatures::damage() const {
    return damage_;
}

bool Creatures::isAlive() const {
    return hp_ > 0;
}

Creatures::Creatures(std::string name, int hp, int damage)
    : name_(std::move(name)), hp_(std::max(1, hp)), maxHp_(std::max(1, hp)), damage_(std::max(1, damage)) {}

void Creatures::receiveDamage(int amount) {
    hp_ = std::max(0, hp_ - std::max(0, amount));
}

void Creatures::healFull() {
    hp_ = maxHp_;
}

} // namespace efd
