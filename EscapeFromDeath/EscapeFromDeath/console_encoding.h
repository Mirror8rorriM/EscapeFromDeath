#pragma once

#include <string>

namespace efd {

// Настраивает консоль и потоки так, чтобы UTF-8 строки из проекта
// корректно отображались в Windows Terminal, cmd.exe, PowerShell и Linux/macOS терминалах.
void setupRussianConsole();

// Читает строку ввода как UTF-8. На Windows-консоли читает UTF-16 через ReadConsoleW
// и конвертирует в UTF-8, поэтому русский ввод работает независимо от системной кодовой страницы.
bool readLineUtf8(std::string& line);

} // namespace efd
