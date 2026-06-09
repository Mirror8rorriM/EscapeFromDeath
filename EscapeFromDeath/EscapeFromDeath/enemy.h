#pragma once

#include "creatures.h"
#include "models.h"

namespace efd {

class Enemy : public Creatures {
public:
    Enemy();
    explicit Enemy(const EnemyData& data);

    double timeScale() const;

private:
    double timeScale_ = 1.0;
};

} 
