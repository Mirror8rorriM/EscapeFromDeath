#include "console_encoding.h"

#include <clocale>
#include <locale>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace efd {

void setupRussianConsole() {
#ifdef _WIN32
    SetConsoleCP(1251);
    SetConsoleOutputCP(1251);
#endif

    if (std::setlocale(LC_ALL, ".1251") == nullptr) {
        if (std::setlocale(LC_ALL, "Russian_Russia.1251") == nullptr) {
            std::setlocale(LC_ALL, "rus");
        }
    }

    try {
        std::locale::global(std::locale(""));
    } catch (...) {
    }
}

} 
