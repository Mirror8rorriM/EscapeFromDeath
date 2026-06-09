#include "models.h"

namespace efd {

bool GameState::hasItem(const std::string& item) const {
    return inventory.find(item) != inventory.end();
}

void GameState::addItem(const std::string& item) {
    inventory.insert(item);
}

void GameState::removeItem(const std::string& item) {
    inventory.erase(item);
}

bool GameState::flag(const std::string& key) const {
    auto it = flags.find(key);
    return it != flags.end() && it->second;
}

void GameState::setFlag(const std::string& key, bool value) {
    flags[key] = value;
}

} 
