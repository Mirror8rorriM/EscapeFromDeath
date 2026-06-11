#include "resource_manager.h"

#include "hero.h"
#include "json.h"
#include "utils.h"

#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace efd {

namespace {

std::vector<std::string> jsonStringArray(const Json& object, const std::string& key) {
    std::vector<std::string> values;
    if (!object.contains(key)) return values;

    for (const auto& value : object.at(key).asArray()) {
        values.push_back(value.asString());
    }
    return values;
}

} // namespace

ResourceManager::ResourceManager(std::string dataDirectory) : dataDirectory_(std::move(dataDirectory)) {}

std::filesystem::path ResourceManager::path(const std::string& fileName) const {
    return dataDirectory_ / fileName;
}

void ResourceManager::loadAll() {
    loadLocations();
    loadEnemies();
    loadCombatPhrases();
    loadDialogues();
    loadItemUseRules();
}

const std::map<std::string, LocationData>& ResourceManager::locations() const {
    return locations_;
}

const std::map<std::string, EnemyData>& ResourceManager::enemies() const {
    return enemies_;
}

const std::vector<CombatPhrase>& ResourceManager::combatPhrases() const {
    return combatPhrases_;
}

const std::vector<DialogueEntry>& ResourceManager::dialogues() const {
    return dialogues_;
}

const std::vector<ItemUseRule>& ResourceManager::itemUseRules() const {
    return itemUseRules_;
}

const LocationData& ResourceManager::location(const std::string& id) const {
    auto it = locations_.find(id);
    if (it == locations_.end()) throw std::runtime_error("Unknown location: " + id);
    return it->second;
}

const EnemyData& ResourceManager::enemy(const std::string& id) const {
    auto it = enemies_.find(id);
    if (it == enemies_.end()) throw std::runtime_error("Unknown enemy: " + id);
    return it->second;
}

std::vector<DialogueEntry> ResourceManager::dialoguesFor(const std::string& npcId, int day, const std::string& locationId) const {
    std::vector<DialogueEntry> result;
    for (const auto& entry : dialogues_) {
        if (entry.npcId == npcId && entry.locationId == locationId && day >= entry.minDay) {
            result.push_back(entry);
        }
    }
    return result;
}

void ResourceManager::loadLocations() {
    locations_.clear();
    Json root = JsonParser::parseFile(path("locations.json").string());
    for (const auto& item : root.at("locations").asArray()) {
        LocationData loc;
        loc.id = item.at("id").asString();
        loc.name = item.at("name").asString();
        loc.description = item.at("description").asString();
        for (const auto& n : item.at("neighbors").asArray()) {
            loc.neighbors.push_back(n.asString());
        }
        locations_[loc.id] = loc;
    }
}

void ResourceManager::loadEnemies() {
    enemies_.clear();
    Json root = JsonParser::parseFile(path("enemies.json").string());
    for (const auto& item : root.at("enemies").asArray()) {
        EnemyData enemy;
        enemy.id = item.at("id").asString();
        enemy.name = item.at("name").asString();
        enemy.hp = item.at("hp").asInt(30);
        enemy.damage = item.at("damage").asInt(10);
        enemy.timeScale = item.contains("timeScale") ? item.at("timeScale").asNumber(1.0) : 1.0;
        enemies_[enemy.id] = enemy;
    }
}

void ResourceManager::loadCombatPhrases() {
    combatPhrases_.clear();
    Json root = JsonParser::parseFile(path("combat_phrases.json").string());
    for (const auto& item : root.at("phrases").asArray()) {
        CombatPhrase phrase;
        phrase.attackText = item.at("attackText").asString();
        for (const auto& reaction : item.at("reactions").asArray()) {
            phrase.reactions.push_back({ reaction.at("word").asString(), reaction.at("text").asString() });
        }
        combatPhrases_.push_back(phrase);
    }
}

void ResourceManager::loadDialogues() {
    dialogues_.clear();
    std::ifstream in(path("dialogues.txt"), std::ios::binary);
    if (!in) throw std::runtime_error("Cannot open dialogues.txt");

    std::string line;
    DialogueEntry current;
    bool hasHeader = false;
    bool firstLine = true;
    while (std::getline(in, line)) {
        if (firstLine) {
            firstLine = false;
            if (line.rfind("\xEF\xBB\xBF", 0) == 0) {
                line.erase(0, 3);
            }
        }
        line = trim(line);
        if (line.empty() || line[0] == '#') continue;

        if (line.front() == '[' && line.back() == ']') {
            if (hasHeader && !current.text.empty()) dialogues_.push_back(current);
            current = DialogueEntry{};
            hasHeader = true;

            std::string header = line.substr(1, line.size() - 2);
            for (const auto& part : split(header, ';')) {
                auto pos = part.find('=');
                if (pos == std::string::npos) continue;
                std::string key = trim(part.substr(0, pos));
                std::string value = trim(part.substr(pos + 1));
                if (key == "npc") current.npcId = value;
                if (key == "day") current.minDay = std::stoi(value);
                if (key == "location") current.locationId = value;
            }
        } else if (hasHeader) {
            if (!current.text.empty()) current.text += "\n";
            current.text += line;
        }
    }
    if (hasHeader && !current.text.empty()) dialogues_.push_back(current);
}


void ResourceManager::loadItemUseRules() {
    itemUseRules_.clear();
    Json root = JsonParser::parseFile(path("item_uses.json").string());

    for (const auto& item : root.at("uses").asArray()) {
        ItemUseRule rule;
        rule.locationId = item.at("locationId").asString();
        rule.item = item.at("item").asString();
        rule.aliases = jsonStringArray(item, "aliases");
        rule.requiresFlags = jsonStringArray(item, "requiresFlags");
        rule.requiresItems = jsonStringArray(item, "requiresItems");
        rule.removeItems = jsonStringArray(item, "removeItems");
        rule.addItems = jsonStringArray(item, "addItems");
        rule.setFlags = jsonStringArray(item, "setFlags");
        rule.message = item.at("message").asString();
        rule.spendAction = item.contains("spendAction") ? item.at("spendAction").asBool(true) : true;
        itemUseRules_.push_back(std::move(rule));
    }
}

void ResourceManager::saveGame(const std::string& savePath, const GameState& state, const Hero& hero) const {
    std::filesystem::create_directories(std::filesystem::path(savePath).parent_path());
    std::ofstream out(savePath, std::ios::binary);
    if (!out) throw std::runtime_error("Cannot write save: " + savePath);

    out << "day=" << state.day << "\n";
    out << "actionsLeft=" << state.actionsLeft << "\n";
    out << "currentLocationId=" << state.currentLocationId << "\n";
    out << "lastTrainingDay=" << state.lastTrainingDay << "\n";
    out << "lastShowerDay=" << state.lastShowerDay << "\n";
    out << "trainingCount=" << state.trainingCount << "\n";
    out << "hp=" << hero.hp() << "\n";
    out << "strength=" << hero.strengthLevel() << "\n";
    out << "agility=" << hero.agilityLevel() << "\n";
    out << "endurance=" << hero.enduranceLevel() << "\n";
    out << "inventory=" << join(state.inventory, ",") << "\n";

    std::set<std::string> activeFlags;
    for (const auto& [key, value] : state.flags) {
        if (value) activeFlags.insert(key);
    }
    out << "flags=" << join(activeFlags, ",") << "\n";
}

bool ResourceManager::loadGame(const std::string& savePath, GameState& state, Hero& hero) const {
    std::ifstream in(savePath, std::ios::binary);
    if (!in) return false;

    std::map<std::string, std::string> kv;
    std::string line;
    bool firstLine = true;
    while (std::getline(in, line)) {
        if (firstLine) {
            firstLine = false;
            if (line.rfind("\xEF\xBB\xBF", 0) == 0) {
                line.erase(0, 3);
            }
        }
        auto pos = line.find('=');
        if (pos == std::string::npos) continue;
        kv[trim(line.substr(0, pos))] = trim(line.substr(pos + 1));
    }

    auto getInt = [&](const std::string& key, int fallback) {
        auto it = kv.find(key);
        if (it == kv.end()) return fallback;
        return std::stoi(it->second);
    };
    auto getString = [&](const std::string& key, const std::string& fallback) {
        auto it = kv.find(key);
        return it == kv.end() ? fallback : it->second;
    };

    state = GameState{};
    state.day = getInt("day", 1);
    state.actionsLeft = getInt("actionsLeft", 5);
    state.currentLocationId = getString("currentLocationId", "cell");
    state.lastTrainingDay = getInt("lastTrainingDay", -100);
    state.lastShowerDay = getInt("lastShowerDay", -100);
    state.trainingCount = getInt("trainingCount", 0);

    for (const auto& item : split(getString("inventory", ""), ',')) {
        state.inventory.insert(item);
    }
    for (const auto& flag : split(getString("flags", ""), ',')) {
        state.flags[flag] = true;
    }

    hero.setStats(
        getInt("strength", 1),
        getInt("agility", 1),
        getInt("endurance", 1),
        getInt("hp", 100)
    );
    return true;
}

} // namespace efd
