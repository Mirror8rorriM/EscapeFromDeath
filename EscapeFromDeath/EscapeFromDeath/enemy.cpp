#include "enemy.h"

namespace efd {

Enemy::Enemy() = default;

Enemy::Enemy(const EnemyData& data)
    : Creatures(data.name, data.hp, data.damage), timeScale_(data.timeScale) {}

double Enemy::timeScale() const {
    return timeScale_;
}

} // namespace efd
