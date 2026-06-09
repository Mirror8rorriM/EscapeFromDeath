#pragma once

#include "combat_system.h"
#include "hero.h"
#include "models.h"
#include "resource_manager.h"

#include <functional>
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
    void actionTalkLibrarian();
    void actionTalkStrongman();
    void actionTalkGambler();
    void actionSearchLaundry();
    void actionSearchCafeteria();
    void actionSearchYard();

    void spendAction();
    void nextDay(int days = 1);
    void handleLostFight();
    void finaleEscape();
    void gameOver(const std::string& text);
    void victory(const std::string& text);

    bool chance(int percent);
    void printInventory() const;
    void addItemOnce(const std::string& item, const std::string& message);

    ResourceManager resources_;
    Hero hero_;
    GameState state_;
    std::mt19937 rng_;
    bool running_ = true;
    bool inGame_ = false;
};

} 
