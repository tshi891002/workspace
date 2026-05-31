#pragma once

#include <windows.h>

#include <algorithm>
#include <string_view>
#include <vector>

namespace console_ui {

// Win32 console colors are bit masks stored in a WORD. The foreground flags can
// be OR-ed together to form a color; enabling red, green, and blue produces the
// normal light-gray console text used as the default for every blank cell.
constexpr WORD COLOR_NORMAL = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;

// ConsoleScreen is the shared rendering surface used by the real-time games.
//
// Design overview:
//   1. Draw the next frame into a std::vector<CHAR_INFO> in ordinary memory.
//   2. Present the complete frame with one WriteConsoleOutputW call.
//   3. Restore the user's original console settings when the game ends.
//
// A CHAR_INFO stores one Unicode character and its color attributes. Treating
// the console as a fixed two-dimensional array avoids scrolling output and
// reduces flicker. This class also uses RAII: its destructor calls Cleanup(), so
// an early return cannot leave the terminal resized or the cursor hidden.
class ConsoleScreen {
public:
    ConsoleScreen() = default;
    ConsoleScreen(const ConsoleScreen&) = delete;
    ConsoleScreen& operator=(const ConsoleScreen&) = delete;

    ~ConsoleScreen() {
        Cleanup();
    }

    // Acquire the process-standard console handles and resize the console to a
    // predictable coordinate system for the game.
    //
    // configureInput is true for games that consume console input records or
    // poll keys while Windows continues to queue those records. In that mode we
    // preserve the original input flags and enable ENABLE_WINDOW_INPUT. Games
    // such as Tetris and Ping-Pong only use GetAsyncKeyState and do not require
    // the input handle to be reconfigured.
    bool Initialize(int width, int height, bool configureInput) {
        width_ = width;
        height_ = height;

        // GetStdHandle returns handles attached to this process by the shell.
        // STD_OUTPUT_HANDLE is used by the rendering APIs. STD_INPUT_HANDLE is
        // needed only when a game changes console input modes or reads events.
        output = GetStdHandle(STD_OUTPUT_HANDLE);
        input = GetStdHandle(STD_INPUT_HANDLE);
        if (!IsValidHandle(output) || (configureInput && !IsValidHandle(input))) {
            return false;
        }

        // Save the original settings before taking over the screen. A console
        // game should behave like a temporary UI, not permanently modify the
        // shell that launched it.
        hasCursorInfo_ = GetConsoleCursorInfo(output, &originalCursorInfo_) != FALSE;

        CONSOLE_SCREEN_BUFFER_INFO screenInfo{};
        if (!GetConsoleScreenBufferInfo(output, &screenInfo)) {
            return false;
        }
        originalBufferSize_ = screenInfo.dwSize;
        originalWindow_ = screenInfo.srWindow;
        hasScreenInfo_ = true;
        initialized_ = true;

        if (configureInput) {
            // GetConsoleMode reads the input behavior bit mask. Keeping the
            // exact original value lets Cleanup restore echo, line-input, and
            // processed-input behavior expected by the user's terminal.
            if (!GetConsoleMode(input, &originalInputMode_)) {
                Cleanup();
                return false;
            }
            hasInputMode_ = true;
        }

        // The visible console window must fit inside the underlying screen
        // buffer. Shrink the visible window temporarily before changing the
        // buffer dimensions, then expand it to the game's requested size.
        SMALL_RECT smallWindow = {0, 0, 0, 0};
        SMALL_RECT window = {0, 0, static_cast<SHORT>(width - 1), static_cast<SHORT>(height - 1)};
        COORD bufferSize = {static_cast<SHORT>(width), static_cast<SHORT>(height)};
        if (!SetConsoleWindowInfo(output, TRUE, &smallWindow)
            || !SetConsoleScreenBufferSize(output, bufferSize)
            || !SetConsoleWindowInfo(output, TRUE, &window)
            || (configureInput && !SetConsoleMode(input, originalInputMode_ | ENABLE_WINDOW_INPUT))) {
            Cleanup();
            return false;
        }

        // The off-screen frame has one CHAR_INFO for each visible console cell.
        buffer.resize(width * height);
        SetCursorVisible(false);
        return true;
    }

    // Restore the user's terminal settings. Cleanup is idempotent because both
    // game code and the destructor may call it. The flags record which original
    // values were successfully captured during partial initialization.
    void Cleanup() {
        if (!initialized_) {
            return;
        }

        if (!buffer.empty()) {
            Clear();
            Flush();
        }
        if (hasCursorInfo_) {
            SetConsoleCursorInfo(output, &originalCursorInfo_);
        }
        if (hasInputMode_) {
            SetConsoleMode(input, originalInputMode_);
        }
        if (hasScreenInfo_) {
            SMALL_RECT smallWindow = {0, 0, 0, 0};
            SetConsoleWindowInfo(output, TRUE, &smallWindow);
            SetConsoleScreenBufferSize(output, originalBufferSize_);
            SetConsoleWindowInfo(output, TRUE, &originalWindow_);
        }
        initialized_ = false;
    }

    void Put(int x, int y, wchar_t ch, WORD color = COLOR_NORMAL) {
        // Centralized clipping keeps all higher-level drawing code bounds-safe.
        // A label or effect that extends past the edge is simply truncated.
        if (x < 0 || x >= width_ || y < 0 || y >= height_) {
            return;
        }

        CHAR_INFO& cell = buffer[y * width_ + x];
        cell.Char.UnicodeChar = ch;
        cell.Attributes = color;
    }

    void WriteText(int x, int y, std::wstring_view text, WORD color = COLOR_NORMAL) {
        for (size_t i = 0; i < text.size(); ++i) {
            Put(x + static_cast<int>(i), y, text[i], color);
        }
    }

    void Clear(WORD color = COLOR_NORMAL) {
        // Clear only changes the in-memory frame. The user sees nothing until
        // Flush presents the completed frame, which is the core anti-flicker
        // design choice shared by these games.
        CHAR_INFO blank{};
        blank.Char.UnicodeChar = L' ';
        blank.Attributes = color;
        std::fill(buffer.begin(), buffer.end(), blank);
    }

    void Flush() {
        // WriteConsoleOutputW copies a rectangular block of CHAR_INFO cells to
        // the console screen buffer. bufferSize describes our source array,
        // bufferCoord selects its top-left source cell, and writeRegion selects
        // the destination rectangle in the real console.
        COORD bufferSize = {static_cast<SHORT>(width_), static_cast<SHORT>(height_)};
        COORD bufferCoord = {0, 0};
        SMALL_RECT writeRegion = {0, 0, static_cast<SHORT>(width_ - 1), static_cast<SHORT>(height_ - 1)};
        WriteConsoleOutputW(output, buffer.data(), bufferSize, bufferCoord, &writeRegion);
    }

    HANDLE output = INVALID_HANDLE_VALUE;
    HANDLE input = INVALID_HANDLE_VALUE;
    std::vector<CHAR_INFO> buffer;

private:
    static bool IsValidHandle(HANDLE handle) {
        return handle != nullptr && handle != INVALID_HANDLE_VALUE;
    }

    void SetCursorVisible(bool visible) {
        // SetConsoleCursorInfo controls the blinking text caret. Full-screen
        // games draw their own UI, so the normal shell caret would be visual
        // noise while a game is active.
        CONSOLE_CURSOR_INFO cursorInfo{};
        cursorInfo.dwSize = 1;
        cursorInfo.bVisible = visible ? TRUE : FALSE;
        SetConsoleCursorInfo(output, &cursorInfo);
    }

    int width_ = 0;
    int height_ = 0;
    DWORD originalInputMode_ = 0;
    CONSOLE_CURSOR_INFO originalCursorInfo_{};
    COORD originalBufferSize_{};
    SMALL_RECT originalWindow_{};
    bool hasInputMode_ = false;
    bool hasCursorInfo_ = false;
    bool hasScreenInfo_ = false;
    bool initialized_ = false;
};

}  // namespace console_ui
