#pragma once
class Creatures {
 private:
  int hp;
  int damage;

 public:
  Creatures(int health, int damage);
  int getHp() const;
};
