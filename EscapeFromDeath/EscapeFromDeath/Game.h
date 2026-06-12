#pragma once

#include "combat_system.h"
#include "hero.h"
#include "models.h"
#include "resource_manager.h"

#include <functional>
#include <initializer_list>
#include <random>
#include <string>
#include <vector>

namespace efd {

class Game {
public:
    explicit Game(std::string dataDirectory = "data");
    void run();

private:
    struct MenuAction {
        std::string title;
        std::function<void()> handler;
    };

    void mainMenu();
    void startNewGame();
    void loadGame();
    void saveGame();

    void intro();
    void gameLoop();
    void renderHeader() const;
    void renderLocation() const;
    std::vector<MenuAction> buildActions();
    void moveTo(const std::string& locationId);

    void actionSleep();
    void actionTrain(StatType stat);
    void actionShower();
    void actionInfirmarySearch();
    void actionReadLibrary();
    void actionTalkSmuggler();
    void actionRobSmuggler();
    void actionTalkLibrarian();
    void actionThreatenLibrarian();
    void actionTalkOldTimer();
    void actionThreatenOldTimer();
    void actionTalkStrongman();
    void actionTalkGangBoss();
    void actionTalkGambler();
    void actionTalkCorruptGuard();
    void actionTalkLaundryman();
    void actionPressureLaundryman();
    void actionTalkCook();
    void actionFightPrisoner();
    void actionSearchLaundry();
    void actionSearchCafeteria();
    void actionSearchYard();
    void actionUseInventoryItem();

    void spendAction();
    void nextDay(int days = 1);
    void handleLostFight();
    void finaleEscape();
    void escapeThroughLaundryTunnel();
    void escapeThroughYardWall();
    void escapeWithDisguise();
    void escapeThroughVentilation();
    void escapeByForce();
    void gameOver(const std::string& text);
    void victory(const std::string& text);

    bool chance(int percent);
    bool hasAnyItem(std::initializer_list<const char*> items) const;
    bool hasAnyFlag(std::initializer_list<const char*> flags) const;
    bool hasPreparedWeapon() const;
    bool hasStrongBody() const;
    void printMissing(const std::vector<std::string>& missing) const;
    bool fightEnemy(const std::string& enemyId);
    void printLatestDialogue(const std::string& npcId, const std::string& locationId) const;
    void printInventory() const;
    void addItemOnce(const std::string& item, const std::string& message);
    const std::string& text(const std::string& key) const;
    void say(const std::string& key) const;

    ResourceManager resources_;
    Hero hero_;
    GameState state_;
    std::mt19937 rng_;
    bool running_ = true;
    bool inGame_ = false;
};

} 
