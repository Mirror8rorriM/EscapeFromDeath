#include "console_encoding.h"

#include <clocale>
#include <iostream>
#include <locale>
#include <streambuf>
#include <string>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace efd {
namespace {

#ifdef _WIN32

bool isConsoleHandle(HANDLE handle) {
    if (handle == nullptr || handle == INVALID_HANDLE_VALUE) return false;
    DWORD mode = 0;
    return GetConsoleMode(handle, &mode) != 0;
}

std::wstring utf8ToWide(const std::string& text) {
    if (text.empty()) return L"";

    int size = MultiByteToWideChar(
        CP_UTF8,
        MB_ERR_INVALID_CHARS,
        text.data(),
        static_cast<int>(text.size()),
        nullptr,
        0
    );

    if (size <= 0) {
        // Резервный вариант: не роняем программу, если в поток попали не-UTF-8 байты.
        size = MultiByteToWideChar(
            CP_ACP,
            0,
            text.data(),
            static_cast<int>(text.size()),
            nullptr,
            0
        );
    }

    if (size <= 0) return L"";

    std::wstring result(static_cast<std::size_t>(size), L'\0');
    if (MultiByteToWideChar(
            CP_UTF8,
            MB_ERR_INVALID_CHARS,
            text.data(),
            static_cast<int>(text.size()),
            result.data(),
            size
        ) <= 0) {
        MultiByteToWideChar(
            CP_ACP,
            0,
            text.data(),
            static_cast<int>(text.size()),
            result.data(),
            size
        );
    }
    return result;
}

std::string wideToUtf8(const std::wstring& text) {
    if (text.empty()) return "";

    int size = WideCharToMultiByte(
        CP_UTF8,
        0,
        text.data(),
        static_cast<int>(text.size()),
        nullptr,
        0,
        nullptr,
        nullptr
    );

    if (size <= 0) return "";

    std::string result(static_cast<std::size_t>(size), '\0');
    WideCharToMultiByte(
        CP_UTF8,
        0,
        text.data(),
        static_cast<int>(text.size()),
        result.data(),
        size,
        nullptr,
        nullptr
    );
    return result;
}

class Utf8ConsoleStreamBuf : public std::streambuf {
public:
    Utf8ConsoleStreamBuf(std::streambuf* fallback, HANDLE consoleHandle)
        : fallback_(fallback), consoleHandle_(consoleHandle), isConsole_(isConsoleHandle(consoleHandle)) {}

    ~Utf8ConsoleStreamBuf() override {
        sync();
    }

protected:
    int overflow(int ch) override {
        if (ch == traits_type::eof()) {
            return sync() == 0 ? traits_type::not_eof(ch) : traits_type::eof();
        }

        buffer_.push_back(static_cast<char>(ch));
        if (ch == '\n') flushBuffer();
        return ch;
    }

    std::streamsize xsputn(const char* s, std::streamsize count) override {
        if (count <= 0) return 0;
        buffer_.append(s, static_cast<std::size_t>(count));

        // Если выводится строка с переводом строки, сразу показываем её в консоли.
        // Промпты без '\n' выводятся при std::flush/std::endl.
        if (buffer_.find('\n') != std::string::npos || buffer_.size() >= 4096) {
            flushBuffer();
        }
        return count;
    }

    int sync() override {
        flushBuffer();
        return fallback_ ? fallback_->pubsync() : 0;
    }

private:
    void flushBuffer() {
        if (buffer_.empty()) return;

        if (isConsole_) {
            const std::wstring wide = utf8ToWide(buffer_);
            if (!wide.empty()) {
                DWORD written = 0;
                WriteConsoleW(
                    consoleHandle_,
                    wide.data(),
                    static_cast<DWORD>(wide.size()),
                    &written,
                    nullptr
                );
            }
        } else if (fallback_) {
            fallback_->sputn(buffer_.data(), static_cast<std::streamsize>(buffer_.size()));
        }

        buffer_.clear();
    }

    std::streambuf* fallback_ = nullptr;
    HANDLE consoleHandle_ = INVALID_HANDLE_VALUE;
    bool isConsole_ = false;
    std::string buffer_;
};

Utf8ConsoleStreamBuf* coutBuffer = nullptr;
Utf8ConsoleStreamBuf* cerrBuffer = nullptr;
Utf8ConsoleStreamBuf* clogBuffer = nullptr;
std::streambuf* originalCout = nullptr;
std::streambuf* originalCerr = nullptr;
std::streambuf* originalClog = nullptr;

void installUtf8ConsoleBuffers() {
    if (coutBuffer != nullptr) return;

    originalCout = std::cout.rdbuf();
    originalCerr = std::cerr.rdbuf();
    originalClog = std::clog.rdbuf();

    coutBuffer = new Utf8ConsoleStreamBuf(originalCout, GetStdHandle(STD_OUTPUT_HANDLE));
    cerrBuffer = new Utf8ConsoleStreamBuf(originalCerr, GetStdHandle(STD_ERROR_HANDLE));
    clogBuffer = new Utf8ConsoleStreamBuf(originalClog, GetStdHandle(STD_ERROR_HANDLE));

    std::cout.rdbuf(coutBuffer);
    std::cerr.rdbuf(cerrBuffer);
    std::clog.rdbuf(clogBuffer);
}

#endif // _WIN32

} // namespace

void setupRussianConsole() {
#ifdef _WIN32
    // Сами файлы проекта находятся в UTF-8. Эти настройки помогают вводу/выводу
    // и при обычном запуске, и при перенаправлении потоков.
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);
    installUtf8ConsoleBuffers();
#endif

    if (std::setlocale(LC_ALL, ".UTF-8") == nullptr) {
        if (std::setlocale(LC_ALL, "Russian_Russia.UTF-8") == nullptr) {
            std::setlocale(LC_ALL, "");
        }
    }

    try {
        std::locale::global(std::locale(""));
    } catch (...) {
    }
}

bool readLineUtf8(std::string& line) {
#ifdef _WIN32
    HANDLE input = GetStdHandle(STD_INPUT_HANDLE);
    if (isConsoleHandle(input)) {
        std::wstring wideLine;
        wchar_t chunk[256];
        DWORD read = 0;

        while (true) {
            if (!ReadConsoleW(input, chunk, static_cast<DWORD>(256), &read, nullptr) || read == 0) {
                return false;
            }

            wideLine.append(chunk, chunk + read);
            if (!wideLine.empty() && wideLine.back() == L'\n') break;
        }

        while (!wideLine.empty() && (wideLine.back() == L'\n' || wideLine.back() == L'\r')) {
            wideLine.pop_back();
        }

        line = wideToUtf8(wideLine);
        return true;
    }
#endif

    return static_cast<bool>(std::getline(std::cin, line));
}

} // namespace efd
