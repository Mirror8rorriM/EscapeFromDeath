#pragma once

#include <map>
#include <set>
#include <string>
#include <vector>

namespace efd {

struct LocationData {
    std::string id;
    std::string name;
    std::string description;
    std::vector<std::string> neighbors;
};

struct EnemyData {
    std::string id;
    std::string name;
    int hp = 30;
    int damage = 10;
    double timeScale = 1.0;
};

struct CombatReaction {
    std::string word;
    std::string text;
};

struct CombatPhrase {
    std::string attackText;
    std::vector<CombatReaction> reactions;
};

struct DialogueEntry {
    std::string npcId;
    int minDay = 1;
    std::string locationId;
    std::string text;
};

struct ItemUseRule {
    std::string locationId;
    std::string item;
    std::vector<std::string> aliases;

    std::vector<std::string> requiresFlags;
    std::vector<std::string> requiresItems;

    std::vector<std::string> removeItems;
    std::vector<std::string> addItems;
    std::vector<std::string> setFlags;

    std::string message;
    bool spendAction = true;
};

struct GameState {
    int day = 1;
    int actionsLeft = 5;
    std::string currentLocationId = "cell";
    int lastTrainingDay = -100;
    int lastShowerDay = -100;
    int trainingCount = 0;
    std::set<std::string> inventory;
    std::map<std::string, bool> flags;

    bool hasItem(const std::string& item) const;
    void addItem(const std::string& item);
    void removeItem(const std::string& item);
    bool flag(const std::string& key) const;
    void setFlag(const std::string& key, bool value = true);
};

} // namespace efd
