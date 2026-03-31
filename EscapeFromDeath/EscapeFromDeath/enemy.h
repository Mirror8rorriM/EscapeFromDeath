#pragma once
#include <iostream>
#include <map>

#include "creatures.h"
class Enemy : public Creatures {
 private:
  std::string const name;
  int const weekness;
  std::map<int, std::string> const attacks;

 public:
  Enemy(int health, int damage, int weekness);
  int takeDamage(int damage, int part);
  void hit() const;
};
