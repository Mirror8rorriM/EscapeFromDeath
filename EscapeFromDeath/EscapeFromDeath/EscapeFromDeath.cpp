#include "Game.h"
#include "console_encoding.h"

int main() {
    efd::setupRussianConsole();

    efd::Game game("data");
    game.run();
    return 0;
}
