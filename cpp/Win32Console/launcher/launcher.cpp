#include "launcher/launcher.h"

#include "games/missionaries_cannibals.h"
#include "games/pingpong.h"
#include "games/shooter.h"
#include "games/snake.h"
#include "games/tankwar.h"
#include "games/tetris.h"
#include "games/tictactoe.h"

#include <windows.h>

#include <array>
#include <cwctype>
#include <string>
#include <string_view>
#include <vector>

namespace console_games {
namespace {

constexpr SHORT MENU_WIDTH = 80;
constexpr SHORT MENU_HEIGHT = 25;
constexpr WORD COLOR_NORMAL = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
constexpr WORD COLOR_DIM = FOREGROUND_INTENSITY;
constexpr WORD COLOR_TITLE = FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
constexpr WORD COLOR_HOTKEY = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY;

using GameRunner = int (*)();

// The menu stores ordinary function pointers. RunGame adapts each concrete game
// class to that small common interface without introducing a base class or heap
// allocation. Every selection creates a fresh game instance, runs it, and then
// destroys it before control returns to the launcher menu.
template <typename Game>
int RunGame() {
    Game game;
    return game.Run();
}

struct MenuItem {
    wchar_t hotkey;
    const wchar_t* title;
    const wchar_t* description;
    GameRunner run;
};

constexpr std::array<MenuItem, 7> MENU_ITEMS{{
    {L'1', L"Ping-Pong", L"Two-player paddle game", RunGame<pingpong_game::PingPongGame>},
    {L'2', L"Tic-Tac-Toe", L"Classic two-player board game", RunGame<tictactoe_game::TicTacToeGame>},
    {L'3', L"Snake", L"Grow longer and avoid collisions", RunGame<snake_game::SnakeGame>},
    {L'4', L"Flying Shooter", L"Dodge enemies and return fire", RunGame<shooter_game::ShooterGame>},
    {L'5', L"Tank War", L"Battle tanks in a console arena", RunGame<tankwar_game::TankWarGame>},
    {L'6', L"Missionaries and Cannibals", L"Solve the river-crossing puzzle", RunGame<missionaries_cannibals_game::MissionariesCannibalsGame>},
    {L'7', L"Tetris", L"Stack falling blocks and clear lines", RunGame<tetris_game::TetrisGame>},
}};

struct ConsoleSnapshot {
    // The launcher temporarily changes the same terminal that invoked it.
    // Capture the shell's original settings once so exiting the launcher can
    // restore the exact input mode, cursor shape, viewport, and title.
    HANDLE output = INVALID_HANDLE_VALUE;
    HANDLE input = INVALID_HANDLE_VALUE;
    DWORD inputMode = 0;
    CONSOLE_CURSOR_INFO cursorInfo{};
    CONSOLE_SCREEN_BUFFER_INFO screenInfo{};
    std::wstring title;
    bool hasInputMode = false;
    bool hasCursorInfo = false;
    bool hasScreenInfo = false;
};

bool IsValidHandle(HANDLE handle) {
    return handle != nullptr && handle != INVALID_HANDLE_VALUE;
}

std::wstring ReadConsoleTitle() {
    // GetConsoleTitleW writes the title into caller-owned storage and returns
    // the number of UTF-16 code units copied, excluding the null terminator.
    std::vector<wchar_t> title(1024);
    const DWORD length = GetConsoleTitleW(title.data(), static_cast<DWORD>(title.size()));
    return std::wstring(title.data(), length);
}

ConsoleSnapshot CaptureConsole() {
    // GetStdHandle returns the console handles inherited by this process.
    // Unlike a game screen, the launcher keeps its own lightweight snapshot
    // because it must reapply menu settings after every selected game exits.
    ConsoleSnapshot snapshot{};
    snapshot.output = GetStdHandle(STD_OUTPUT_HANDLE);
    snapshot.input = GetStdHandle(STD_INPUT_HANDLE);
    if (!IsValidHandle(snapshot.output) || !IsValidHandle(snapshot.input)) {
        return snapshot;
    }

    snapshot.hasInputMode = GetConsoleMode(snapshot.input, &snapshot.inputMode) != FALSE;
    snapshot.hasCursorInfo = GetConsoleCursorInfo(snapshot.output, &snapshot.cursorInfo) != FALSE;
    snapshot.hasScreenInfo = GetConsoleScreenBufferInfo(snapshot.output, &snapshot.screenInfo) != FALSE;
    snapshot.title = ReadConsoleTitle();
    return snapshot;
}

void ResizeConsole(HANDLE output, COORD size, SMALL_RECT window) {
    // Win32 requires the viewport rectangle to fit inside the backing screen
    // buffer. Temporarily shrinking the viewport makes both enlarging and
    // shrinking the buffer reliable before the final viewport is installed.
    SMALL_RECT smallWindow = {0, 0, 0, 0};
    SetConsoleWindowInfo(output, TRUE, &smallWindow);
    SetConsoleScreenBufferSize(output, size);
    SetConsoleWindowInfo(output, TRUE, &window);
}

void SetCursorVisible(HANDLE output, bool visible) {
    CONSOLE_CURSOR_INFO cursorInfo{};
    cursorInfo.dwSize = 1;
    cursorInfo.bVisible = visible ? TRUE : FALSE;
    SetConsoleCursorInfo(output, &cursorInfo);
}

void ClearConsole(HANDLE output) {
    // The menu does not maintain a persistent off-screen frame between draws.
    // FillConsoleOutputCharacterW and FillConsoleOutputAttribute clear the
    // current Win32 screen buffer directly, then the cursor is moved home.
    CONSOLE_SCREEN_BUFFER_INFO info{};
    if (!GetConsoleScreenBufferInfo(output, &info)) {
        return;
    }

    const DWORD size = static_cast<DWORD>(info.dwSize.X) * static_cast<DWORD>(info.dwSize.Y);
    const COORD origin = {0, 0};
    DWORD written = 0;
    FillConsoleOutputCharacterW(output, L' ', size, origin, &written);
    FillConsoleOutputAttribute(output, COLOR_NORMAL, size, origin, &written);
    SetConsoleCursorPosition(output, origin);
}

void ConfigureMenuConsole(const ConsoleSnapshot& snapshot) {
    // A game may use a different screen size and input mode. Reestablish the
    // menu's expected environment every time control returns from a game.
    const COORD size = {MENU_WIDTH, MENU_HEIGHT};
    const SMALL_RECT window = {0, 0, MENU_WIDTH - 1, MENU_HEIGHT - 1};
    ResizeConsole(snapshot.output, size, window);
    if (snapshot.hasInputMode) {
        SetConsoleMode(snapshot.input, snapshot.inputMode | ENABLE_WINDOW_INPUT);
    }
    // Discard key events left over from the game, especially the Escape press
    // used to quit it. Otherwise that same record could immediately close the
    // launcher before the menu becomes usable.
    FlushConsoleInputBuffer(snapshot.input);
    SetCursorVisible(snapshot.output, false);
    SetConsoleTitleW(L"Win32 Console Games");
    ClearConsole(snapshot.output);
}

void RestoreConsole(const ConsoleSnapshot& snapshot) {
    // This is the launcher's final handoff back to the shell. It mirrors the
    // initial snapshot so the combined app remains polite when it exits.
    ClearConsole(snapshot.output);
    if (snapshot.hasInputMode) {
        SetConsoleMode(snapshot.input, snapshot.inputMode);
    }
    if (snapshot.hasCursorInfo) {
        SetConsoleCursorInfo(snapshot.output, &snapshot.cursorInfo);
    }
    if (snapshot.hasScreenInfo) {
        ResizeConsole(snapshot.output, snapshot.screenInfo.dwSize, snapshot.screenInfo.srWindow);
    }
    SetConsoleTitleW(snapshot.title.c_str());
}

void Put(std::vector<CHAR_INFO>& buffer, int x, int y, wchar_t ch, WORD color = COLOR_NORMAL) {
    if (x < 0 || x >= MENU_WIDTH || y < 0 || y >= MENU_HEIGHT) {
        return;
    }

    CHAR_INFO& cell = buffer[y * MENU_WIDTH + x];
    cell.Char.UnicodeChar = ch;
    cell.Attributes = color;
}

void WriteText(std::vector<CHAR_INFO>& buffer, int x, int y, std::wstring_view text, WORD color = COLOR_NORMAL) {
    for (size_t i = 0; i < text.size(); ++i) {
        Put(buffer, x + static_cast<int>(i), y, text[i], color);
    }
}

void DrawMenu(HANDLE output) {
    // Build a complete menu frame in memory and present it atomically. The menu
    // uses the same CHAR_INFO technique as the games, but keeps the tiny helper
    // local because it owns only one static screen.
    CHAR_INFO blank{};
    blank.Char.UnicodeChar = L' ';
    blank.Attributes = COLOR_NORMAL;
    std::vector<CHAR_INFO> buffer(MENU_WIDTH * MENU_HEIGHT, blank);

    WriteText(buffer, 25, 2, L"Win32 Console Games", COLOR_TITLE);
    WriteText(buffer, 15, 4, L"Choose a game. Each one returns here when you quit.", COLOR_DIM);

    for (size_t i = 0; i < MENU_ITEMS.size(); ++i) {
        const MenuItem& item = MENU_ITEMS[i];
        const int y = 7 + static_cast<int>(i) * 2;
        Put(buffer, 14, y, item.hotkey, COLOR_HOTKEY);
        WriteText(buffer, 15, y, L". ");
        WriteText(buffer, 17, y, item.title, COLOR_TITLE);
        WriteText(buffer, 43, y, item.description, COLOR_DIM);
    }

    WriteText(buffer, 14, 22, L"Press 1-7 to play or Esc/Q to quit.");

    COORD bufferSize = {MENU_WIDTH, MENU_HEIGHT};
    COORD bufferCoord = {0, 0};
    SMALL_RECT writeRegion = {0, 0, MENU_WIDTH - 1, MENU_HEIGHT - 1};
    WriteConsoleOutputW(output, buffer.data(), bufferSize, bufferCoord, &writeRegion);
}

int ReadMenuChoice(HANDLE input) {
    // ReadConsoleInputW returns typed INPUT_RECORD values rather than a stream
    // of characters. Filtering KEY_EVENT records and bKeyDown prevents window
    // notifications and key-release events from selecting menu items.
    INPUT_RECORD record{};
    DWORD recordsRead = 0;
    while (ReadConsoleInputW(input, &record, 1, &recordsRead)) {
        if (record.EventType != KEY_EVENT || !record.Event.KeyEvent.bKeyDown) {
            continue;
        }

        const KEY_EVENT_RECORD& key = record.Event.KeyEvent;
        if (key.wVirtualKeyCode == VK_ESCAPE || towupper(key.uChar.UnicodeChar) == L'Q') {
            return -1;
        }
        if (key.uChar.UnicodeChar >= L'1' && key.uChar.UnicodeChar <= L'7') {
            return key.uChar.UnicodeChar - L'1';
        }
    }
    return -1;
}

}  // namespace

int RunLauncher() {
    // The launcher owns the outer application loop. Individual games are
    // intentionally unaware of the menu: each game runs until its own quit key
    // is pressed, restores its screen state, and returns here.
    const ConsoleSnapshot snapshot = CaptureConsole();
    if (!IsValidHandle(snapshot.output) || !IsValidHandle(snapshot.input)) {
        return 1;
    }

    while (true) {
        ConfigureMenuConsole(snapshot);
        DrawMenu(snapshot.output);

        const int selected = ReadMenuChoice(snapshot.input);
        if (selected < 0) {
            break;
        }
        MENU_ITEMS[selected].run();
    }

    RestoreConsole(snapshot);
    return 0;
}

}  // namespace console_games
