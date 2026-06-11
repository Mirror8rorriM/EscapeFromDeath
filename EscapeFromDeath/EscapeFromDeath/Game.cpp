#include "Game.h"

#include "console_encoding.h"
#include "utils.h"

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

void Game::run() {
    try {
        resources_.loadAll();
        mainMenu();
    } catch (const std::exception& ex) {
        std::cout << "Критическая ошибка: " << ex.what() << "\n";
    }
}

void Game::mainMenu() {
    while (running_) {
        std::cout << "\n==============================\n";
        std::cout << "       ESCAPE FROM DEATH       \n";
        std::cout << "==============================\n";
        std::cout << "1. Новая игра\n";
        std::cout << "2. Загрузить игру\n";
        std::cout << "3. Выйти\n";

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
        std::cout << "Сохранение не найдено.\n";
        return;
    }
    hero_ = loadedHero;
    state_ = loadedState;
    inGame_ = true;
    std::cout << "Игра загружена.\n";
    gameLoop();
}

void Game::saveGame() {
    resources_.saveGame(SAVE_PATH, state_, hero_);
    std::cout << "Игра сохранена в " << SAVE_PATH << ".\n";
}

void Game::intro() {
    std::cout << "\nВас приговорили к смертной казни. До приговора осталось десять дней.\n";
    std::cout << "Девять дней вы можете изучать тюрьму, добывать вещи и информацию.\n";
    std::cout << "В ночь на десятый день придётся бежать. Ошибки уже не исправить.\n\n";
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
            std::cout << "\nСил на новые дела не осталось. Охрана гонит вас обратно в камеру.\n";
            state_.currentLocationId = "cell";
            actionSleep();
        }
    }

    inGame_ = false;
}

void Game::renderHeader() const {
    std::cout << "\n----------------------------------------\n";
    std::cout << "День: " << state_.day << "/9 | Действий осталось: " << state_.actionsLeft << "\n";
    std::cout << hero_.statsLine() << "\n";
    std::cout << "Инвентарь: ";
    printInventory();
    std::cout << "\n----------------------------------------\n";
}

void Game::renderLocation() const {
    const auto& loc = resources_.location(state_.currentLocationId);
    std::cout << "Локация: " << loc.name << "\n";
    std::cout << loc.description << "\n\n";
}

std::vector<Game::MenuAction> Game::buildActions() {
    std::vector<MenuAction> actions;
    const auto& loc = resources_.location(state_.currentLocationId);

    for (const auto& neighborId : loc.neighbors) {
        const auto& target = resources_.location(neighborId);
        actions.push_back({"Перейти: " + target.name, [this, neighborId]() { moveTo(neighborId); }});
    }

    const std::string& id = loc.id;
    if (id == "cell") {
        actions.push_back({"Поспать до следующего дня", [this]() { actionSleep(); }});
    } else if (id == "gym") {
        actions.push_back({"Тренировка силы", [this]() { actionTrain(StatType::Strength); }});
        actions.push_back({"Тренировка ловкости", [this]() { actionTrain(StatType::Agility); }});
        actions.push_back({"Тренировка выносливости", [this]() { actionTrain(StatType::Endurance); }});
        if (state_.trainingCount >= 5 && !state_.flag("strongman_friend")) {
            actions.push_back({"Поговорить с Силачом", [this]() { actionTalkStrongman(); }});
        }
    } else if (id == "shower") {
        actions.push_back({"Помыться и восстановить здоровье", [this]() { actionShower(); }});
    } else if (id == "infirmary") {
        actions.push_back({"Осмотреть лазарет", [this]() { actionInfirmarySearch(); }});
    } else if (id == "library") {
        actions.push_back({"Почитать о старой части тюрьмы", [this]() { actionReadLibrary(); }});
        actions.push_back({"Поговорить с Библиотекарем", [this]() { actionTalkLibrarian(); }});
    } else if (id == "cafeteria") {
        actions.push_back({"Осмотреть столовую", [this]() { actionSearchCafeteria(); }});
        if (state_.day >= 3) {
            actions.push_back({"Поговорить с Контрабандистом", [this]() { actionTalkSmuggler(); }});
        }
    } else if (id == "yard") {
        actions.push_back({"Поговорить с заключёнными во дворе", [this]() { actionSearchYard(); }});
        if (state_.day >= 4) {
            actions.push_back({"Сыграть с Игроманом на ставку", [this]() { actionTalkGambler(); }});
        }
    } else if (id == "laundry") {
        actions.push_back({"Обыскать прачечную", [this]() { actionSearchLaundry(); }});
    }

    if (!state_.inventory.empty()) {
        actions.push_back({"Применить предмет из инвентаря", [this]() { actionUseInventoryItem(); }});
    }

    actions.push_back({"Сохранить игру", [this]() { saveGame(); }});
    actions.push_back({"Выйти в главное меню", [this]() { inGame_ = false; }});
    return actions;
}

void Game::moveTo(const std::string& locationId) {
    state_.currentLocationId = locationId;
}

void Game::actionSleep() {
    if (!askYesNo("Вы точно хотите закончить этот день?")) return;
    nextDay();
}

void Game::actionTrain(StatType stat) {
    if (state_.day - state_.lastTrainingDay < 2) {
        std::cout << "Я слишком устал после прошлой тренировки. Нет сил на новую.\n";
        return;
    }

    bool upgraded = hero_.train(stat);
    state_.lastTrainingDay = state_.day;
    state_.trainingCount++;
    spendAction();

    if (upgraded) {
        std::cout << "Тренировка прошла успешно. Характеристика выросла.\n";
    } else {
        std::cout << "Эта характеристика уже на максимальном уровне, но тренировка поддержала форму.\n";
    }

    if (state_.trainingCount >= 5 && !state_.flag("strongman_seen")) {
        state_.setFlag("strongman_seen");
        std::cout << "Силач заметил вашу настойчивость. Кажется, он хочет поговорить.\n";
    }
}

void Game::actionShower() {
    if (state_.day - state_.lastShowerDay < 3) {
        std::cout << "Заключённым запрещено мыться чаще одного раза в 3 дня.\n";
        return;
    }
    hero_.healFull();
    state_.lastShowerDay = state_.day;
    spendAction();
    std::cout << "Вы смыли кровь и восстановили здоровье.\n";
}

void Game::actionInfirmarySearch() {
    spendAction();
    if (!state_.hasItem("Скальпель") && chance(30)) {
        addItemOnce("Скальпель", "Медсестра отвернулась. Вы украли скальпель.");
    } else {
        std::cout << "Ничего полезного найти не удалось.\n";
    }
}

void Game::actionReadLibrary() {
    spendAction();
    if (!state_.flag("info_old_tunnels")) {
        state_.setFlag("info_old_tunnels");
        std::cout << "В старой книге вы нашли упоминание о техническом тоннеле под прачечной.\n";
    } else {
        std::cout << "Вы перечитали записи. Тоннель под прачечной всё ещё кажется лучшим шансом.\n";
    }
}

void Game::actionTalkSmuggler() {
    spendAction();
    auto lines = resources_.dialoguesFor("smuggler", state_.day, "cafeteria");
    if (!lines.empty()) std::cout << lines.back().text << "\n";

    if (!state_.flag("smuggler_trust")) {
        std::cout << "Контрабандист требует доказать, что вы не подставной. Нужно спровоцировать драку с охраной.\n";
        if (askYesNo("Устроить драку?")) {
            CombatSystem combat(resources_.combatPhrases());
            bool won = combat.fight(hero_, resources_.enemy("guard"));
            if (won) {
                state_.setFlag("smuggler_trust");
                addItemOnce("Отмычка", "Контрабандист одобрительно кивает и передаёт вам отмычку.");
            } else {
                handleLostFight();
            }
        }
        return;
    }

    if (!state_.hasItem("Верёвка")) {
        addItemOnce("Верёвка", "Контрабандист достал для вас крепкую верёвку.");
    } else if (!state_.flag("guard_schedule")) {
        state_.setFlag("guard_schedule");
        std::cout << "Контрабандист рассказал, что у прачечной ночью только один обход.\n";
    } else {
        std::cout << "Контрабандист больше ничем не может помочь бесплатно.\n";
    }
}

void Game::actionTalkLibrarian() {
    spendAction();
    auto lines = resources_.dialoguesFor("librarian", state_.day, "library");
    if (!lines.empty()) std::cout << lines.back().text << "\n";

    if (!state_.flag("info_laundry_exit")) {
        state_.setFlag("info_laundry_exit");
        std::cout << "Библиотекарь шепчет: 'Ищи не стены, а стоки. Прачечная ближе всего к свободе'.\n";
    } else {
        std::cout << "Библиотекарь советует не доверять первому попавшемуся маршруту.\n";
    }
}

void Game::actionTalkStrongman() {
    spendAction();
    state_.setFlag("strongman_friend");
    addItemOnce("Железный прут", "Силач дал вам железный прут и пообещал отвлечь охрану в нужный момент.");
}

void Game::actionTalkGambler() {
    spendAction();
    std::cout << "Игроман предлагает ставку: ваша пайка против карты обходов.\n";
    if (!askYesNo("Согласиться играть?")) return;

    if (chance(50)) {
        state_.setFlag("guard_schedule");
        std::cout << "Вы выиграли. Игроман отдаёт карту ночных обходов.\n";
    } else {
        std::cout << "Вы проиграли ставку. День стал немного хуже, но жизнь продолжается.\n";
    }
}

void Game::actionSearchLaundry() {
    spendAction();
    if (!state_.hasItem("Форма заключённого-прачки")) {
        addItemOnce("Форма заключённого-прачки", "В куче белья вы нашли форму, в которой можно пройти мимо сонной охраны.");
    } else if (!state_.flag("laundry_lock_weak")) {
        state_.setFlag("laundry_lock_weak");
        std::cout << "Вы заметили, что замок на технической двери держится на одном ржавом винте.\n";
    } else {
        std::cout << "Прачечная уже изучена вдоль и поперёк.\n";
    }
}

void Game::actionSearchCafeteria() {
    spendAction();
    if (!state_.hasItem("Ложка")) {
        addItemOnce("Ложка", "Вы украли заточенную ложку со стола.");
    } else {
        std::cout << "В столовой слишком много глаз. Сегодня лучше не рисковать.\n";
    }
}

void Game::actionSearchYard() {
    spendAction();
    if (!state_.flag("yard_wall_info")) {
        state_.setFlag("yard_wall_info");
        std::cout << "Заключённые рассказали, что внешняя стена освещается всю ночь. Через двор идти опасно.\n";
    } else {
        std::cout << "Во дворе обсуждают слухи, но ничего нового вы не узнали.\n";
    }
}


void Game::actionUseInventoryItem() {
    if (state_.inventory.empty()) {
        std::cout << "У вас нет предметов.\n";
        return;
    }

    std::cout << "Инвентарь: ";
    printInventory();
    std::cout << "\nВведите название предмета или оставьте строку пустой для отмены: ";

    std::string input;
    if (!readLineUtf8(input)) {
        std::cout << "\nВвод закрыт.\n";
        inGame_ = false;
        return;
    }

    input = trim(input);
    if (input.empty()) {
        std::cout << "Вы передумали.\n";
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
            std::cout << "У вас нет нужного предмета: " << rule.item << ".\n";
            return;
        }

        for (const auto& item : rule.requiresItems) {
            if (!state_.hasItem(item)) {
                std::cout << "Для этого нужен ещё один предмет: " << item << ".\n";
                return;
            }
        }

        for (const auto& flag : rule.requiresFlags) {
            if (!state_.flag(flag)) {
                std::cout << "Вы пока не понимаете, как это здесь использовать. Нужно больше информации.\n";
                return;
            }
        }

        if (allFlagsAlreadySet(state_, rule.setFlags)) {
            std::cout << "Вы уже использовали это здесь.\n";
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
        std::cout << "Здесь этот предмет применить не получится.\n";
    } else {
        std::cout << "Такого предмета нет в инвентаре. Проверьте название.\n";
    }
}

void Game::spendAction() {
    state_.actionsLeft = std::max(0, state_.actionsLeft - 1);
}

void Game::nextDay(int days) {
    state_.day += days;
    state_.actionsLeft = 5;
    state_.currentLocationId = "cell";
    std::cout << "\nВы засыпаете. Наступает день " << state_.day << ".\n";
}

void Game::handleLostFight() {
    nextDay(2);
    state_.currentLocationId = "infirmary";
    hero_.healFull();
}

void Game::finaleEscape() {
    std::cout << "\n========================================\n";
    std::cout << "Ночь на десятый день. Пора бежать.\n";
    std::cout << "========================================\n";
    std::cout << "1. Идти через прачечную и старый тоннель\n";
    std::cout << "2. Прорываться через двор\n";
    std::cout << "3. Сдаться судьбе\n";
    int choice = askInt("> ", 1, 3);

    if (choice == 3) {
        gameOver("Утром приговор привели в исполнение. Побег так и не начался.");
        return;
    }

    if (choice == 2) {
        if (!state_.hasItem("Железный прут") && !state_.hasItem("Скальпель")) {
            gameOver("Во дворе вас заметили прожекторы. Без оружия прорваться не удалось.");
            return;
        }
        CombatSystem combat(resources_.combatPhrases());
        bool won = combat.fight(hero_, resources_.enemy("guard_squad"));
        if (won && (state_.flag("guard_schedule") || state_.flag("rope_escape_ready") || state_.flag("yard_fence_weakened"))) {
            victory("Вы прорвались через двор, используя подготовленное снаряжение и карту обходов. Свобода пахнет холодным воздухом.");
        } else {
            gameOver("Даже победив часть охраны, вы выбрали слишком заметный маршрут. Вас схватили у стены.");
        }
        return;
    }

    int score = 0;
    std::vector<std::string> missing;

    if (state_.flag("info_old_tunnels") || state_.flag("info_laundry_exit") || state_.flag("hidden_map_found")) ++score;
    else missing.push_back("нет информации о тоннеле");

    if (state_.hasItem("Отмычка") || state_.hasItem("Скальпель") || state_.hasItem("Самодельная отвёртка") || state_.flag("laundry_lock_weak") || state_.flag("laundry_lock_opened")) ++score;
    else missing.push_back("нечем открыть техническую дверь");

    if (state_.hasItem("Верёвка") || state_.hasItem("Форма заключённого-прачки") || state_.flag("rope_escape_ready") || state_.flag("laundry_disguise_ready")) ++score;
    else missing.push_back("нет снаряжения/маскировки");

    if (state_.flag("guard_schedule") || state_.flag("strongman_friend")) ++score;
    else missing.push_back("неизвестны ночные обходы");

    if (score >= 3) {
        victory("Вы открыли дверь прачечной, прошли по старому тоннелю и выбрались за стену до рассвета.");
    } else {
        std::cout << "План развалился. Проблемы:\n";
        for (const auto& item : missing) std::cout << "- " << item << "\n";
        gameOver("Охрана нашла вас в техническом коридоре. Казнь состоится утром.");
    }
}

void Game::gameOver(const std::string& text) {
    std::cout << "\nПОРАЖЕНИЕ\n" << text << "\n";
    inGame_ = false;
}

void Game::victory(const std::string& text) {
    std::cout << "\nПОБЕДА\n" << text << "\n";
    inGame_ = false;
}

bool Game::chance(int percent) {
    std::uniform_int_distribution<int> dist(1, 100);
    return dist(rng_) <= percent;
}

void Game::printInventory() const {
    if (state_.inventory.empty()) {
        std::cout << "ничего";
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

} // namespace efd
