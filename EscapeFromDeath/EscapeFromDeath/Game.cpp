#include "Game.h"

#include "console_encoding.h"
#include "utils.h"

#include <algorithm>
#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace efd {

namespace {
const char* SAVE_PATH = "saves/save.txt";

bool sameTextUtf8(const std::string& left, const std::string& right) {
    return toLowerUtf8(trim(left)) == toLowerUtf8(trim(right));
}

bool ruleNameMatches(const ItemUseRule& rule, const std::string& input) {
    if (sameTextUtf8(rule.item, input)) return true;

    for (const auto& alias : rule.aliases) {
        if (sameTextUtf8(alias, input)) return true;
    }
    return false;
}

bool allFlagsAlreadySet(const GameState& state, const std::vector<std::string>& flags) {
    if (flags.empty()) return false;

    for (const auto& flag : flags) {
        if (!state.flag(flag)) return false;
    }
    return true;
}

} // namespace

Game::Game(std::string dataDirectory)
    : resources_(std::move(dataDirectory)), rng_(std::random_device{}()) {}

const std::string& Game::text(const std::string& key) const {
    return resources_.text(key);
}

void Game::say(const std::string& key) const {
    std::cout << text(key) << "\n";
}

void Game::run() {
    try {
        resources_.loadAll();
        setUtilityTexts(&resources_.texts());
        mainMenu();
    } catch (const std::exception& ex) {
        std::cout << text("error.critical_prefix") << ex.what() << "\n";
    }
}

void Game::mainMenu() {
    while (running_) {
        std::cout << text("main_menu") << "\n";

        int choice = askInt("> ", 1, 3);
        switch (choice) {
            case 1: startNewGame(); break;
            case 2: loadGame(); break;
            case 3: running_ = false; break;
        }
    }
}

void Game::startNewGame() {
    hero_ = Hero{};
    state_ = GameState{};
    inGame_ = true;
    intro();
    gameLoop();
}

void Game::loadGame() {
    Hero loadedHero;
    GameState loadedState;
    if (!resources_.loadGame(SAVE_PATH, loadedState, loadedHero)) {
        say("save.not_found");
        return;
    }
    hero_ = loadedHero;
    state_ = loadedState;
    inGame_ = true;
    say("save.loaded");
    gameLoop();
}

void Game::saveGame() {
    resources_.saveGame(SAVE_PATH, state_, hero_);
    std::cout << text("save.saved_prefix") << SAVE_PATH << text("save.saved_suffix") << "\n";
}

void Game::intro() {
    std::cout << "\n" << text("intro") << "\n\n";
}

void Game::gameLoop() {
    while (inGame_ && hero_.isAlive()) {
        if (state_.day >= 10) {
            finaleEscape();
            break;
        }

        renderHeader();
        renderLocation();
        auto actions = buildActions();
        for (std::size_t i = 0; i < actions.size(); ++i) {
            std::cout << i + 1 << ". " << actions[i].title << "\n";
        }
        int choice = askInt("> ", 1, static_cast<int>(actions.size()));
        actions[choice - 1].handler();

        if (state_.actionsLeft <= 0 && inGame_ && state_.day < 10) {
            std::cout << "\n" << text("day.no_actions_left") << "\n";
            state_.currentLocationId = "cell";
            actionSleep();
        }
    }

    inGame_ = false;
}

void Game::renderHeader() const {
    std::cout << "\n----------------------------------------\n";
    std::cout << text("header.day") << state_.day << text("header.actions_left") << state_.actionsLeft << "\n";
    std::cout << text("stats.hp") << hero_.hp() << "/" << hero_.maxHp()
              << text("stats.strength") << hero_.strengthLevel()
              << text("stats.agility") << hero_.agilityLevel()
              << text("stats.endurance") << hero_.enduranceLevel() << "\n";
    std::cout << text("header.inventory");
    printInventory();
    std::cout << "\n----------------------------------------\n";
}

void Game::renderLocation() const {
    const auto& loc = resources_.location(state_.currentLocationId);
    std::cout << text("location.prefix") << loc.name << "\n";
    std::cout << loc.description << "\n\n";
}

std::vector<Game::MenuAction> Game::buildActions() {
    std::vector<MenuAction> actions;
    const auto& loc = resources_.location(state_.currentLocationId);

    for (const auto& neighborId : loc.neighbors) {
        const auto& target = resources_.location(neighborId);
        actions.push_back({text("action.move_prefix") + target.name, [this, neighborId]() { moveTo(neighborId); }});
    }

    const std::string& id = loc.id;
    if (id == "cell") {
        actions.push_back({text("action.sleep"), [this]() { actionSleep(); }});
        if (state_.day >= 2) {
            actions.push_back({text("action.talk_old_timer"), [this]() { actionTalkOldTimer(); }});
            if (!state_.flag("old_timer_intimidated")) {
                actions.push_back({text("action.threaten_old_timer"), [this]() { actionThreatenOldTimer(); }});
            }
        }
    } else if (id == "gym") {
        actions.push_back({text("action.train_strength"), [this]() { actionTrain(StatType::Strength); }});
        actions.push_back({text("action.train_agility"), [this]() { actionTrain(StatType::Agility); }});
        actions.push_back({text("action.train_endurance"), [this]() { actionTrain(StatType::Endurance); }});
        if (state_.trainingCount >= 5 && !state_.flag("strongman_friend")) {
            actions.push_back({text("action.talk_strongman"), [this]() { actionTalkStrongman(); }});
        }
    } else if (id == "shower") {
        actions.push_back({text("action.shower"), [this]() { actionShower(); }});
        if (state_.day >= 3) {
            actions.push_back({text("action.talk_gang_boss"), [this]() { actionTalkGangBoss(); }});
        }
    } else if (id == "infirmary") {
        actions.push_back({text("action.search_infirmary"), [this]() { actionInfirmarySearch(); }});
    } else if (id == "library") {
        actions.push_back({text("action.read_library"), [this]() { actionReadLibrary(); }});
        actions.push_back({text("action.talk_librarian"), [this]() { actionTalkLibrarian(); }});
        if (!state_.flag("hidden_map_found")) {
            actions.push_back({text("action.threaten_librarian"), [this]() { actionThreatenLibrarian(); }});
        }
    } else if (id == "cafeteria") {
        actions.push_back({text("action.search_cafeteria"), [this]() { actionSearchCafeteria(); }});
        if (state_.day >= 3) {
            actions.push_back({text("action.talk_smuggler"), [this]() { actionTalkSmuggler(); }});
            if (!state_.flag("smuggler_trust") && !state_.flag("smuggler_enemy")) {
                actions.push_back({text("action.rob_smuggler"), [this]() { actionRobSmuggler(); }});
            }
        }
        if (state_.day >= 4) {
            actions.push_back({text("action.talk_cook"), [this]() { actionTalkCook(); }});
        }
    } else if (id == "yard") {
        actions.push_back({text("action.search_yard"), [this]() { actionSearchYard(); }});
        if (state_.day >= 2) {
            actions.push_back({text("action.fight_prisoner"), [this]() { actionFightPrisoner(); }});
        }
        if (state_.day >= 4) {
            actions.push_back({text("action.talk_gambler"), [this]() { actionTalkGambler(); }});
        }
    } else if (id == "laundry") {
        actions.push_back({text("action.search_laundry"), [this]() { actionSearchLaundry(); }});
        actions.push_back({text("action.talk_laundryman"), [this]() { actionTalkLaundryman(); }});
        if (!state_.flag("laundryman_defeated")) {
            actions.push_back({text("action.pressure_laundryman"), [this]() { actionPressureLaundryman(); }});
        }
        if (state_.day >= 2) {
            actions.push_back({text("action.talk_corrupt_guard"), [this]() { actionTalkCorruptGuard(); }});
        }
    }

    if (!state_.inventory.empty()) {
        actions.push_back({text("action.use_item"), [this]() { actionUseInventoryItem(); }});
    }

    actions.push_back({text("action.save"), [this]() { saveGame(); }});
    actions.push_back({text("action.exit_to_menu"), [this]() { inGame_ = false; }});
    return actions;
}

void Game::moveTo(const std::string& locationId) {
    state_.currentLocationId = locationId;
}

void Game::actionSleep() {
    if (!askYesNo(text("prompt.end_day"))) return;
    nextDay();
}

void Game::actionTrain(StatType stat) {
    if (state_.day - state_.lastTrainingDay < 2) {
        say("train.too_tired");
        return;
    }

    bool upgraded = hero_.train(stat);
    state_.lastTrainingDay = state_.day;
    state_.trainingCount++;
    spendAction();

    if (upgraded) {
        say("train.success");
    } else {
        say("train.maxed");
    }

    if (state_.trainingCount >= 5 && !state_.flag("strongman_seen")) {
        state_.setFlag("strongman_seen");
        say("train.strongman_notice");
    }
}

void Game::actionShower() {
    if (state_.day - state_.lastShowerDay < 3) {
        say("shower.too_soon");
        return;
    }
    hero_.healFull();
    state_.lastShowerDay = state_.day;
    spendAction();
    say("shower.success");
}

void Game::actionInfirmarySearch() {
    spendAction();
    if (!state_.hasItem("Скальпель") && chance(30)) {
        addItemOnce("Скальпель", text("item_found.scalpel"));
    } else {
        say("search.nothing");
    }
}

void Game::actionReadLibrary() {
    spendAction();
    if (!state_.flag("info_old_tunnels")) {
        state_.setFlag("info_old_tunnels");
        say("library.old_tunnel_found");
    } else {
        say("library.old_tunnel_repeat");
    }
}

void Game::actionTalkSmuggler() {
    spendAction();
    auto lines = resources_.dialoguesFor("smuggler", state_.day, "cafeteria");
    if (!lines.empty()) std::cout << lines.back().text << "\n";

    if (!state_.flag("smuggler_trust")) {
        say("smuggler.trust_required");
        if (askYesNo(text("prompt.start_fight"))) {
            CombatSystem combat(resources_.combatPhrases(), resources_.texts());
            bool won = combat.fight(hero_, resources_.enemy("guard"));
            if (won) {
                state_.setFlag("smuggler_trust");
                addItemOnce("Отмычка", text("item_found.lockpick_from_smuggler"));
            } else {
                handleLostFight();
            }
        }
        return;
    }

    if (!state_.hasItem("Верёвка")) {
        addItemOnce("Верёвка", text("item_found.rope_from_smuggler"));
    } else if (!state_.flag("guard_schedule")) {
        state_.setFlag("guard_schedule");
        say("smuggler.guard_schedule");
    } else {
        say("smuggler.no_more_free_help");
    }
}

void Game::actionRobSmuggler() {
    if (state_.flag("smuggler_trust")) {
        say("smuggler.rob_bad_idea");
        return;
    }
    if (!askYesNo(text("prompt.rob_smuggler"))) return;

    spendAction();
    if (fightEnemy("smuggler")) {
        state_.setFlag("smuggler_enemy");
        addItemOnce("Отмычка", text("item_found.lockpick_stolen"));
        addItemOnce("Сигареты", text("item_found.cigarettes_from_smuggler"));
        say("smuggler.enemy_now");
    } else {
        handleLostFight();
    }
}

void Game::actionTalkLibrarian() {
    spendAction();
    printLatestDialogue("librarian", "library");

    if (state_.flag("gang_librarian_debt") && !state_.flag("hidden_map_found")) {
        state_.setFlag("hidden_map_found");
        say("librarian.map_from_debt");
    } else if (!state_.flag("info_laundry_exit")) {
        state_.setFlag("info_laundry_exit");
        say("librarian.laundry_exit_hint");
    } else {
        say("librarian.no_more_help");
    }
}

void Game::actionThreatenLibrarian() {
    if (state_.flag("hidden_map_found")) {
        say("librarian.map_already_have");
        return;
    }
    if (!askYesNo(text("prompt.threaten_librarian"))) return;

    spendAction();
    if (fightEnemy("librarian")) {
        state_.setFlag("hidden_map_found");
        state_.setFlag("librarian_intimidated");
        say("librarian.map_by_force");
    } else {
        handleLostFight();
    }
}

void Game::actionTalkOldTimer() {
    spendAction();
    printLatestDialogue("old_timer", "cell");

    if (!state_.flag("old_timer_pipe_noise")) {
        state_.setFlag("old_timer_pipe_noise");
        say("old_timer.pipe_noise");
    } else if (state_.day >= 7 && !state_.flag("rainy_night_escape")) {
        state_.setFlag("rainy_night_escape");
        say("old_timer.rain_hint");
    } else {
        say("old_timer.no_more_help");
    }
}

void Game::actionThreatenOldTimer() {
    if (!askYesNo(text("prompt.threaten_old_timer"))) return;

    spendAction();
    if (fightEnemy("old_timer")) {
        state_.setFlag("old_timer_intimidated");
        state_.setFlag("old_timer_pipe_noise");
        if (state_.day >= 7) {
            state_.setFlag("rainy_night_escape");
        }
        say("old_timer.intimidated");
    } else {
        handleLostFight();
    }
}

void Game::actionTalkStrongman() {
    spendAction();
    state_.setFlag("strongman_friend");
    addItemOnce("Железный прут", text("item_found.iron_bar"));
}

void Game::actionTalkGangBoss() {
    spendAction();
    printLatestDialogue("gang_boss", "shower");

    if (!state_.flag("gang_respect")) {
        say("gang_boss.challenge");
        if (!askYesNo(text("prompt.fight_gang_boss"))) return;

        if (fightEnemy("gang_boss")) {
            state_.setFlag("gang_respect");
            addItemOnce("Заточка", text("item_found.shiv"));
            say("gang_boss.respect_won");
        } else {
            handleLostFight();
        }
        return;
    }

    if (state_.day >= 6 && !state_.flag("gang_librarian_debt")) {
        state_.setFlag("gang_librarian_debt");
        say("gang_boss.librarian_debt");
    } else {
        say("gang_boss.no_more_help");
    }
}

void Game::actionTalkGambler() {
    spendAction();
    say("gambler.offer");
    if (!askYesNo(text("prompt.play_gambler"))) return;

    if (chance(50)) {
        state_.setFlag("guard_schedule");
        say("gambler.win");
    } else {
        say("gambler.lose");
    }
}

void Game::actionTalkCorruptGuard() {
    spendAction();
    printLatestDialogue("corrupt_guard", "laundry");

    if (!state_.flag("corrupt_guard_contact")) {
        state_.setFlag("corrupt_guard_contact");
        say("corrupt_guard.contact");
        return;
    }

    if (state_.hasItem("Сигареты") && !state_.flag("laundry_bribe_paid")) {
        say("corrupt_guard.use_cigarettes_hint");
        return;
    }

    if (!state_.flag("corrupt_guard_defeated") && askYesNo(text("prompt.fight_corrupt_guard"))) {
        if (fightEnemy("corrupt_guard")) {
            state_.setFlag("corrupt_guard_defeated");
            state_.setFlag("guard_schedule");
            addItemOnce("Пропуск прачечной", text("item_found.laundry_pass"));
        } else {
            handleLostFight();
        }
        return;
    }

    say("corrupt_guard.ignores");
}

void Game::actionTalkLaundryman() {
    spendAction();
    printLatestDialogue("laundryman", "laundry");

    if (!state_.flag("laundryman_knows_you")) {
        state_.setFlag("laundryman_knows_you");
        addItemOnce("Кусок проволоки", text("item_found.wire"));
    } else if (state_.day >= 5 && !state_.flag("laundry_machine_noise")) {
        state_.setFlag("laundry_machine_noise");
        say("laundryman.machine_noise");
    } else {
        say("laundryman.no_more_help");
    }
}

void Game::actionPressureLaundryman() {
    if (!askYesNo(text("prompt.pressure_laundryman"))) return;

    spendAction();
    if (fightEnemy("laundryman")) {
        state_.setFlag("laundryman_defeated");
        state_.setFlag("laundry_lock_weak");
        addItemOnce("Ключ от подсобки", text("item_found.storage_key"));
        say("laundryman.weak_lock_notice");
    } else {
        handleLostFight();
    }
}

void Game::actionTalkCook() {
    spendAction();
    printLatestDialogue("cook", "cafeteria");

    if (!state_.flag("cook_deal") && state_.hasItem("Железный прут")) {
        if (askYesNo(text("prompt.give_bar_to_cook"))) {
            state_.removeItem("Железный прут");
            state_.setFlag("cook_deal");
            addItemOnce("Порошок для каши", text("item_found.porridge_powder"));
            return;
        }
    }

    if (state_.flag("cook_deal") && state_.day >= 7 && !state_.flag("guards_food_poisoned")) {
        state_.setFlag("guards_food_poisoned");
        say("cook.guards_poisoned");
        return;
    }

    if (!state_.flag("cook_defeated") && askYesNo(text("prompt.fight_cook"))) {
        if (fightEnemy("cook")) {
            state_.setFlag("cook_defeated");
            addItemOnce("Кухонный тесак", text("item_found.cleaver"));
        } else {
            handleLostFight();
        }
        return;
    }

    say("cook.no_more_help");
}

void Game::actionFightPrisoner() {
    if (!askYesNo(text("prompt.fight_prisoner"))) return;

    spendAction();
    if (fightEnemy("prisoner")) {
        state_.setFlag("prisoner_respect");
        addItemOnce("Сигареты", text("item_found.cigarettes_from_prisoner"));
    } else {
        handleLostFight();
    }
}

void Game::actionSearchLaundry() {
    spendAction();
    if (!state_.hasItem("Форма заключённого-прачки")) {
        addItemOnce("Форма заключённого-прачки", text("item_found.laundry_uniform"));
    } else if (!state_.flag("laundry_lock_weak")) {
        state_.setFlag("laundry_lock_weak");
        say("laundry.weak_lock_found");
    } else {
        say("laundry.fully_searched");
    }
}

void Game::actionSearchCafeteria() {
    spendAction();
    if (!state_.hasItem("Ложка")) {
        addItemOnce("Ложка", text("item_found.spoon"));
    } else {
        say("cafeteria.too_many_eyes");
    }
}

void Game::actionSearchYard() {
    spendAction();
    if (!state_.flag("yard_wall_info")) {
        state_.setFlag("yard_wall_info");
        say("yard.wall_info");
    } else {
        say("yard.no_new_info");
    }
}


void Game::actionUseInventoryItem() {
    if (state_.inventory.empty()) {
        say("item_use.no_items");
        return;
    }

    std::cout << text("header.inventory");
    printInventory();
    std::cout << "\n" << text("item_use.prompt") << " ";

    std::string input;
    if (!readLineUtf8(input)) {
        std::cout << "\n" << text("input.closed") << "\n";
        inGame_ = false;
        return;
    }

    input = trim(input);
    if (input.empty()) {
        say("item_use.cancelled");
        return;
    }

    bool inventoryContainsInput = false;
    for (const auto& item : state_.inventory) {
        if (sameTextUtf8(item, input)) {
            inventoryContainsInput = true;
            break;
        }
    }

    const std::string& locationId = state_.currentLocationId;
    for (const auto& rule : resources_.itemUseRules()) {
        if (rule.locationId != locationId || !ruleNameMatches(rule, input)) {
            continue;
        }

        if (!state_.hasItem(rule.item)) {
            std::cout << text("item_use.missing_required_prefix") << rule.item << text("item_use.sentence_suffix") << "\n";
            return;
        }

        for (const auto& item : rule.requiresItems) {
            if (!state_.hasItem(item)) {
                std::cout << text("item_use.missing_extra_prefix") << item << text("item_use.sentence_suffix") << "\n";
                return;
            }
        }

        for (const auto& flag : rule.requiresFlags) {
            if (!state_.flag(flag)) {
                say("item_use.need_more_info");
                return;
            }
        }

        if (allFlagsAlreadySet(state_, rule.setFlags)) {
            say("item_use.already_used");
            return;
        }

        for (const auto& item : rule.removeItems) {
            state_.removeItem(item);
        }

        for (const auto& item : rule.addItems) {
            state_.addItem(item);
        }

        for (const auto& flag : rule.setFlags) {
            state_.setFlag(flag);
        }

        if (rule.spendAction) {
            spendAction();
        }

        std::cout << rule.message << "\n";
        return;
    }

    if (inventoryContainsInput) {
        say("item_use.cannot_use_here");
    } else {
        say("item_use.not_in_inventory");
    }
}

void Game::spendAction() {
    state_.actionsLeft = std::max(0, state_.actionsLeft - 1);
}

void Game::nextDay(int days) {
    state_.day += days;
    state_.actionsLeft = 5;
    state_.currentLocationId = "cell";
    std::cout << "\n" << text("day.sleep_prefix") << state_.day << text("day.sleep_suffix") << "\n";
}

void Game::handleLostFight() {
    nextDay(2);
    state_.currentLocationId = "infirmary";
    hero_.healFull();
}

bool Game::hasAnyItem(std::initializer_list<const char*> items) const {
    for (const char* item : items) {
        if (state_.hasItem(item)) return true;
    }
    return false;
}

bool Game::hasAnyFlag(std::initializer_list<const char*> flags) const {
    for (const char* flag : flags) {
        if (state_.flag(flag)) return true;
    }
    return false;
}

bool Game::hasPreparedWeapon() const {
    return state_.flag("yard_weapon_ready")
        || hasAnyItem({"Кухонный тесак", "Заточка", "Железный прут", "Скальпель"});
}

bool Game::hasStrongBody() const {
    return hero_.strengthLevel() >= 2 && hero_.enduranceLevel() >= 2;
}

void Game::printMissing(const std::vector<std::string>& missing) const {
    if (missing.empty()) return;
    std::cout << text("finale.plan_not_ready") << "\n";
    for (const auto& item : missing) {
        std::cout << "- " << item << "\n";
    }
}

void Game::finaleEscape() {
    std::cout << text("finale.menu") << "\n";

    int choice = askInt("> ", 1, 6);
    switch (choice) {
        case 1: escapeThroughLaundryTunnel(); break;
        case 2: escapeThroughYardWall(); break;
        case 3: escapeWithDisguise(); break;
        case 4: escapeThroughVentilation(); break;
        case 5: escapeByForce(); break;
        case 6:
        default:
            gameOver(text("ending.surrender"));
            break;
    }
}

void Game::escapeThroughLaundryTunnel() {
    std::vector<std::string> missing;

    const bool knowsTunnel = hasAnyFlag({
        "info_old_tunnels", "info_laundry_exit", "hidden_map_found",
        "old_timer_pipe_noise", "gang_librarian_debt"
    });
    const bool canOpenDoor = hasAnyFlag({
        "laundry_lock_opened", "laundry_storage_unlocked", "laundry_service_panel_opened"
    }) || hasAnyItem({
        "Отмычка", "Самодельная отвёртка", "Ключ от подсобки", "Кусок проволоки"
    });
    const bool canPassLaundry = hasAnyFlag({
        "laundry_disguise_ready", "laundry_pass_ready", "laundry_bribe_paid"
    }) || hasAnyItem({
        "Форма заключённого-прачки", "Пропуск прачечной"
    });
    const bool safeTiming = hasAnyFlag({
        "guard_schedule", "laundry_machine_noise", "guards_food_poisoned",
        "rainy_night_escape", "strongman_friend"
    });

    if (!knowsTunnel) missing.push_back(text("missing.tunnel_info"));
    if (!canOpenDoor) missing.push_back(text("missing.open_tech_door"));
    if (!canPassLaundry) missing.push_back(text("missing.laundry_access"));
    if (!safeTiming) missing.push_back(text("missing.safe_timing"));

    if (!missing.empty()) {
        printMissing(missing);
        gameOver(text("ending.laundry_tunnel_fail"));
        return;
    }

    victory(text("ending.laundry_tunnel_win"));
}

void Game::escapeThroughYardWall() {
    std::vector<std::string> missing;

    const bool hasClimbGear = state_.flag("rope_escape_ready") || state_.hasItem("Верёвка");
    const bool fenceReady = state_.flag("yard_fence_weakened") || state_.hasItem("Железный прут");
    const bool cover = hasAnyFlag({"rainy_night_escape", "guard_schedule", "guards_food_poisoned"});
    const bool defense = hasPreparedWeapon() || hasStrongBody() || hasAnyFlag({"strongman_friend", "gang_respect"});

    if (!hasClimbGear) missing.push_back(text("missing.climb_gear"));
    if (!fenceReady) missing.push_back(text("missing.fence_ready"));
    if (!cover) missing.push_back(text("missing.cover"));
    if (!defense) missing.push_back(text("missing.defense"));

    if (!missing.empty()) {
        printMissing(missing);
        gameOver(text("ending.yard_wall_fail"));
        return;
    }

    victory(text("ending.yard_wall_win"));
}

void Game::escapeWithDisguise() {
    std::vector<std::string> missing;

    const bool disguise = state_.flag("laundry_disguise_ready") || state_.hasItem("Форма заключённого-прачки");
    const bool pass = hasAnyFlag({"laundry_pass_ready", "laundry_bribe_paid"}) || state_.hasItem("Пропуск прачечной");
    const bool schedule = hasAnyFlag({"guard_schedule", "guards_food_poisoned", "corrupt_guard_contact"});
    const bool nerve = hero_.agilityLevel() >= 2 || hero_.enduranceLevel() >= 2 || hasAnyFlag({"gang_respect", "strongman_friend"});

    if (!disguise) missing.push_back(text("missing.disguise"));
    if (!pass) missing.push_back(text("missing.pass"));
    if (!schedule) missing.push_back(text("missing.schedule"));
    if (!nerve) missing.push_back(text("missing.nerve"));

    if (!missing.empty()) {
        printMissing(missing);
        gameOver(text("ending.disguise_fail"));
        return;
    }

    victory(text("ending.disguise_win"));
}

void Game::escapeThroughVentilation() {
    std::vector<std::string> missing;

    const bool hasMap = hasAnyFlag({"hidden_map_found", "info_old_tunnels", "info_laundry_exit", "gang_librarian_debt"});
    const bool panelOpen = hasAnyFlag({"laundry_service_panel_opened", "vent_route_ready"})
        || hasAnyItem({"Самодельная отвёртка", "Скальпель"});
    const bool silentWork = hasAnyFlag({"old_timer_pipe_noise", "laundry_machine_noise"});
    const bool canBypassGrate = hasAnyFlag({"laundry_lock_opened", "laundry_storage_unlocked"})
        || hasAnyItem({"Кусок проволоки", "Отмычка", "Ключ от подсобки"});
    const bool physicalFit = hero_.agilityLevel() >= 2 || hero_.enduranceLevel() >= 2 || state_.hasItem("Верёвка");

    if (!hasMap) missing.push_back(text("missing.vent_map"));
    if (!panelOpen) missing.push_back(text("missing.panel_tool"));
    if (!silentWork) missing.push_back(text("missing.silent_work"));
    if (!canBypassGrate) missing.push_back(text("missing.bypass_grate"));
    if (!physicalFit) missing.push_back(text("missing.physical_fit"));

    if (!missing.empty()) {
        printMissing(missing);
        gameOver(text("ending.vent_fail"));
        return;
    }

    victory(text("ending.vent_win"));
}

void Game::escapeByForce() {
    std::vector<std::string> missing;

    const bool weapon = hasPreparedWeapon();
    const bool body = hasStrongBody() || hero_.strengthLevel() >= 3;
    const bool allies = hasAnyFlag({"strongman_friend", "gang_respect", "prisoner_respect"});
    const bool chaos = hasAnyFlag({"guards_food_poisoned", "rainy_night_escape", "guard_schedule"});

    if (!weapon) missing.push_back(text("missing.weapon"));
    if (!body) missing.push_back(text("missing.body"));
    if (!allies) missing.push_back(text("missing.allies"));
    if (!chaos) missing.push_back(text("missing.chaos"));

    if (!missing.empty()) {
        printMissing(missing);
        gameOver(text("ending.force_fail"));
        return;
    }

    const bool overwhelmingPreparation = hero_.strengthLevel() >= 3
        && hero_.enduranceLevel() >= 3
        && state_.flag("yard_weapon_ready")
        && state_.flag("guards_food_poisoned")
        && hasAnyFlag({"strongman_friend", "gang_respect"});

    if (overwhelmingPreparation) {
        victory(text("ending.force_overprepared_win"));
        return;
    }

    CombatSystem combat(resources_.combatPhrases(), resources_.texts());
    bool won = combat.fight(hero_, resources_.enemy("guard_squad"));
    if (won) {
        victory(text("ending.force_win"));
    } else {
        gameOver(text("ending.force_combat_fail"));
    }
}

void Game::gameOver(const std::string& text) {
    std::cout << "\n" << this->text("ending.defeat_title") << "\n" << text << "\n";
    inGame_ = false;
}

void Game::victory(const std::string& text) {
    std::cout << "\n" << this->text("ending.victory_title") << "\n" << text << "\n";
    inGame_ = false;
}

bool Game::chance(int percent) {
    std::uniform_int_distribution<int> dist(1, 100);
    return dist(rng_) <= percent;
}

bool Game::fightEnemy(const std::string& enemyId) {
    CombatSystem combat(resources_.combatPhrases(), resources_.texts());
    return combat.fight(hero_, resources_.enemy(enemyId));
}

void Game::printLatestDialogue(const std::string& npcId, const std::string& locationId) const {
    auto lines = resources_.dialoguesFor(npcId, state_.day, locationId);
    if (!lines.empty()) {
        std::cout << lines.back().text << "\n";
    }
}

void Game::printInventory() const {
    if (state_.inventory.empty()) {
        std::cout << text("inventory.empty");
        return;
    }
    bool first = true;
    for (const auto& item : state_.inventory) {
        if (!first) std::cout << ", ";
        std::cout << item;
        first = false;
    }
}

void Game::addItemOnce(const std::string& item, const std::string& message) {
    if (!state_.hasItem(item)) {
        state_.addItem(item);
        std::cout << message << "\n";
    }
}

}
