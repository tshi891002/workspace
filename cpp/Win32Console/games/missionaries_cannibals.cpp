#include <windows.h>

#include <algorithm>
#include <array>
#include <cwctype>
#include <string>
#include <vector>

#include "games/missionaries_cannibals.h"

namespace {

// This program presents the classic Missionaries and Cannibals river-crossing
// puzzle as a small Win32 console application. The puzzle state is deliberately
// tiny: we only need to know how many missionaries and cannibals are on the left
// bank and where the boat is. The right bank is derived from those values.
//
// Rule summary:
//   * Three missionaries and three cannibals start on the left bank.
//   * The boat carries one or two people.
//   * On either bank, if missionaries are present, cannibals must not outnumber
//     missionaries. Otherwise the missionaries on that bank would be unsafe.
//   * The goal is to move all six people to the right bank.
//
// The UI is intentionally simple and keyboard-driven:
//   1 = move 1 missionary
//   2 = move 2 missionaries
//   3 = move 1 cannibal
//   4 = move 2 cannibals
//   5 = move 1 missionary and 1 cannibal
//   H = apply the next move from a precomputed solution path
//   R = reset, Q/Esc = quit

constexpr int TOTAL_MISSIONARIES = 3;
constexpr int TOTAL_CANNIBALS = 3;

constexpr int SCREEN_WIDTH = 92;
constexpr int SCREEN_HEIGHT = 30;

// Win32 console colors are bit masks. Foreground bits color the glyph itself,
// background bits color the cell behind the glyph, and INTENSITY makes the
// selected color brighter. Keeping them named makes drawing code read like UI
// code instead of a wall of numeric constants.
constexpr WORD COLOR_NORMAL = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
constexpr WORD COLOR_DIM = FOREGROUND_INTENSITY;
constexpr WORD COLOR_TITLE = FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
constexpr WORD COLOR_BANK = FOREGROUND_GREEN | FOREGROUND_INTENSITY;
constexpr WORD COLOR_RIVER = FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
constexpr WORD COLOR_BOAT = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
constexpr WORD COLOR_MISSIONARY = FOREGROUND_BLUE | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
constexpr WORD COLOR_CANNIBAL = FOREGROUND_RED | FOREGROUND_INTENSITY;
constexpr WORD COLOR_ERROR = FOREGROUND_RED | FOREGROUND_INTENSITY;
constexpr WORD COLOR_SUCCESS = FOREGROUND_GREEN | FOREGROUND_INTENSITY;
constexpr WORD COLOR_HOTKEY = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY;

enum class Side {
    Left,
    Right
};

struct Move {
    int missionaries = 0;
    int cannibals = 0;
    wchar_t hotkey = L'?';
    const wchar_t* label = L"";
};

struct State {
    int leftMissionaries = TOTAL_MISSIONARIES;
    int leftCannibals = TOTAL_CANNIBALS;
    Side boat = Side::Left;
};

struct App {
    HANDLE output = INVALID_HANDLE_VALUE;
    HANDLE input = INVALID_HANDLE_VALUE;
    std::vector<CHAR_INFO> buffer;
    State state;
    std::vector<State> solutionPath;
    std::vector<State> history;
    std::wstring message = L"Choose a boat load. The boat carries one or two people.";
    WORD messageColor = COLOR_NORMAL;
    bool running = true;
};

const std::array<Move, 5> MOVES = {
    Move{ 1, 0, L'1', L"1 missionary" },
    Move{ 2, 0, L'2', L"2 missionaries" },
    Move{ 0, 1, L'3', L"1 cannibal" },
    Move{ 0, 2, L'4', L"2 cannibals" },
    Move{ 1, 1, L'5', L"1 missionary + 1 cannibal" },
};

bool SameState(const State& a, const State& b) {
    return a.leftMissionaries == b.leftMissionaries
        && a.leftCannibals == b.leftCannibals
        && a.boat == b.boat;
}

int RightMissionaries(const State& state) {
    return TOTAL_MISSIONARIES - state.leftMissionaries;
}

int RightCannibals(const State& state) {
    return TOTAL_CANNIBALS - state.leftCannibals;
}

bool BankIsSafe(int missionaries, int cannibals) {
    // A bank with zero missionaries is safe because there are no missionaries
    // to be outnumbered. Otherwise missionaries must be at least as numerous as
    // cannibals on that bank.
    return missionaries == 0 || missionaries >= cannibals;
}

bool IsValidState(const State& state) {
    if (state.leftMissionaries < 0 || state.leftMissionaries > TOTAL_MISSIONARIES
        || state.leftCannibals < 0 || state.leftCannibals > TOTAL_CANNIBALS) {
        return false;
    }

    return BankIsSafe(state.leftMissionaries, state.leftCannibals)
        && BankIsSafe(RightMissionaries(state), RightCannibals(state));
}

bool IsGoal(const State& state) {
    return state.leftMissionaries == 0 && state.leftCannibals == 0 && state.boat == Side::Right;
}

State ApplyMoveUnchecked(const State& state, const Move& move) {
    // The boat always moves to the opposite bank. When it leaves the left bank,
    // selected passengers are subtracted from the left-bank counts. When it
    // returns from the right bank, selected passengers are added back.
    State next = state;
    const int direction = state.boat == Side::Left ? -1 : 1;
    next.leftMissionaries += direction * move.missionaries;
    next.leftCannibals += direction * move.cannibals;
    next.boat = state.boat == Side::Left ? Side::Right : Side::Left;
    return next;
}

std::wstring DescribeState(const State& state) {
    std::wstring text = L"L(" + std::to_wstring(state.leftMissionaries) + L"M,"
        + std::to_wstring(state.leftCannibals) + L"C)  R("
        + std::to_wstring(RightMissionaries(state)) + L"M,"
        + std::to_wstring(RightCannibals(state)) + L"C)  Boat: ";
    text += state.boat == Side::Left ? L"Left" : L"Right";
    return text;
}

void SetCursorVisible(HANDLE console, bool visible) {
    // SetConsoleCursorInfo changes the console cursor owned by this screen
    // buffer. Console programs normally leave a blinking caret on screen, but
    // this app redraws the entire scene every frame. Hiding the caret keeps it
    // from appearing as a stray UI element over the river drawing.
    CONSOLE_CURSOR_INFO cursorInfo{};
    cursorInfo.dwSize = 1;
    cursorInfo.bVisible = visible ? TRUE : FALSE;
    SetConsoleCursorInfo(console, &cursorInfo);
}

void Put(App& app, int x, int y, wchar_t ch, WORD color = COLOR_NORMAL) {
    // CHAR_INFO is the native Win32 console cell structure: a Unicode character
    // plus a WORD attribute containing foreground/background color bits. We draw
    // into app.buffer first, then copy the whole buffer to the real console in
    // one WriteConsoleOutputW call. That avoids flicker and avoids scrolling.
    if (x < 0 || x >= SCREEN_WIDTH || y < 0 || y >= SCREEN_HEIGHT) {
        return;
    }

    CHAR_INFO& cell = app.buffer[y * SCREEN_WIDTH + x];
    cell.Char.UnicodeChar = ch;
    cell.Attributes = color;
}

void WriteText(App& app, int x, int y, const std::wstring& text, WORD color = COLOR_NORMAL) {
    for (int i = 0; i < static_cast<int>(text.size()); ++i) {
        Put(app, x + i, y, text[static_cast<std::size_t>(i)], color);
    }
}

void Clear(App& app) {
    for (auto& cell : app.buffer) {
        cell.Char.UnicodeChar = L' ';
        cell.Attributes = COLOR_NORMAL;
    }
}

void Flush(App& app) {
    // WriteConsoleOutputW copies a rectangular block of CHAR_INFO cells into the
    // console screen buffer. This is lower-level than std::wcout: it lets us
    // choose exact x/y coordinates and colors without emitting escape sequences.
    COORD bufferSize{ SCREEN_WIDTH, SCREEN_HEIGHT };
    COORD bufferCoord{ 0, 0 };
    SMALL_RECT target{ 0, 0, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1 };
    WriteConsoleOutputW(app.output, app.buffer.data(), bufferSize, bufferCoord, &target);
}

void DrawBox(App& app, int left, int top, int width, int height, WORD color) {
    for (int x = left; x < left + width; ++x) {
        Put(app, x, top, L'-', color);
        Put(app, x, top + height - 1, L'-', color);
    }
    for (int y = top; y < top + height; ++y) {
        Put(app, left, y, L'|', color);
        Put(app, left + width - 1, y, L'|', color);
    }
    Put(app, left, top, L'+', color);
    Put(app, left + width - 1, top, L'+', color);
    Put(app, left, top + height - 1, L'+', color);
    Put(app, left + width - 1, top + height - 1, L'+', color);
}

void DrawPeople(App& app, int x, int y, int missionaries, int cannibals) {
    // People are drawn as compact labeled tokens. The counts are the actual
    // model values; there is no separate display state that could drift out of
    // sync with the puzzle rules.
    for (int i = 0; i < missionaries; ++i) {
        WriteText(app, x, y + i, L"[M] Missionary", COLOR_MISSIONARY);
    }
    for (int i = 0; i < cannibals; ++i) {
        WriteText(app, x, y + missionaries + i, L"[C] Cannibal  ", COLOR_CANNIBAL);
    }
}

void DrawScene(App& app) {
    Clear(app);

    WriteText(app, 2, 1, L"Missionaries and Cannibals - Win32 Console", COLOR_TITLE);
    WriteText(app, 2, 2, L"Move everyone across without ever letting cannibals outnumber missionaries.", COLOR_DIM);

    DrawBox(app, 2, 5, 24, 14, COLOR_BANK);
    DrawBox(app, 66, 5, 24, 14, COLOR_BANK);
    WriteText(app, 4, 6, L"LEFT BANK", COLOR_BANK);
    WriteText(app, 68, 6, L"RIGHT BANK", COLOR_BANK);

    for (int y = 5; y < 19; ++y) {
        for (int x = 29; x < 63; ++x) {
            Put(app, x, y, y % 2 == 0 ? L'~' : L'=', COLOR_RIVER);
        }
    }
    WriteText(app, 41, 6, L"RIVER", COLOR_RIVER);

    DrawPeople(app, 5, 8, app.state.leftMissionaries, app.state.leftCannibals);
    DrawPeople(app, 69, 8, RightMissionaries(app.state), RightCannibals(app.state));

    const int boatX = app.state.boat == Side::Left ? 27 : 55;
    WriteText(app, boatX, 14, L"  ____  ", COLOR_BOAT);
    WriteText(app, boatX, 15, L" /____\\ ", COLOR_BOAT);
    WriteText(app, boatX, 16, L"   BOAT ", COLOR_BOAT);

    WriteText(app, 2, 20, L"State: " + DescribeState(app.state), COLOR_NORMAL);
    WriteText(app, 2, 21, app.message, app.messageColor);

    WriteText(app, 2, 23, L"Boat loads:", COLOR_NORMAL);
    for (int i = 0; i < static_cast<int>(MOVES.size()); ++i) {
        const Move& move = MOVES[static_cast<std::size_t>(i)];
        std::wstring item;
        item += move.hotkey;
        item += L" ";
        item += move.label;
        WriteText(app, 15 + i * 14, 23, item, COLOR_HOTKEY);
    }

    WriteText(app, 2, 25, L"H next solution move    R reset    Q/Esc quit", COLOR_DIM);
    WriteText(app, 2, 27, L"Design: state is validated after each attempted move; the hint path is found by BFS.", COLOR_DIM);

    if (IsGoal(app.state)) {
        WriteText(app, 28, 12, L"PUZZLE SOLVED", COLOR_SUCCESS);
    }
}

bool TryApplyMove(App& app, const Move& move, bool fromHint) {
    const State next = ApplyMoveUnchecked(app.state, move);

    if (!IsValidState(next)) {
        app.message = L"Illegal move: that load would empty the wrong bank or make a bank unsafe.";
        app.messageColor = COLOR_ERROR;
        return false;
    }

    app.history.push_back(app.state);
    app.state = next;

    if (IsGoal(app.state)) {
        app.message = L"Success! Everyone crossed safely.";
        app.messageColor = COLOR_SUCCESS;
    } else if (fromHint) {
        app.message = L"Hint applied. Continue with H, or choose your own next boat load.";
        app.messageColor = COLOR_SUCCESS;
    } else {
        app.message = L"Move accepted.";
        app.messageColor = COLOR_SUCCESS;
    }

    return true;
}

std::vector<State> BuildSolutionPath() {
    // Breadth-first search is a good fit because the state space is very small:
    // 4 possible missionary counts * 4 cannibal counts * 2 boat sides = 32
    // theoretical states, and fewer once safety rules are applied. BFS finds a
    // shortest legal route without hard-coding a magic sequence into the UI.
    struct Node {
        State state;
        int parent = -1;
    };

    std::vector<Node> queue;
    queue.push_back(Node{ State{}, -1 });

    for (std::size_t index = 0; index < queue.size(); ++index) {
        const Node current = queue[index];
        if (IsGoal(current.state)) {
            std::vector<State> path;
            for (int node = static_cast<int>(index); node >= 0; node = queue[static_cast<std::size_t>(node)].parent) {
                path.push_back(queue[static_cast<std::size_t>(node)].state);
            }
            std::reverse(path.begin(), path.end());
            return path;
        }

        for (const Move& move : MOVES) {
            const State next = ApplyMoveUnchecked(current.state, move);
            if (!IsValidState(next)) {
                continue;
            }

            const bool alreadySeen = std::any_of(queue.begin(), queue.end(), [&](const Node& node) {
                return SameState(node.state, next);
            });
            if (!alreadySeen) {
                queue.push_back(Node{ next, static_cast<int>(index) });
            }
        }
    }

    return {};
}

void ApplyHint(App& app) {
    if (IsGoal(app.state)) {
        app.message = L"The puzzle is already solved. Press R to try again.";
        app.messageColor = COLOR_SUCCESS;
        return;
    }

    const auto current = std::find_if(app.solutionPath.begin(), app.solutionPath.end(), [&](const State& state) {
        return SameState(state, app.state);
    });

    if (current == app.solutionPath.end() || current + 1 == app.solutionPath.end()) {
        app.message = L"No direct hint from this state. Press R for the canonical solution path.";
        app.messageColor = COLOR_ERROR;
        return;
    }

    const State next = *(current + 1);
    for (const Move& move : MOVES) {
        if (SameState(ApplyMoveUnchecked(app.state, move), next)) {
            TryApplyMove(app, move, true);
            return;
        }
    }
}

void Reset(App& app) {
    app.state = State{};
    app.history.clear();
    app.message = L"Reset. Choose a boat load or press H for the next solution move.";
    app.messageColor = COLOR_NORMAL;
}

bool ConfigureConsole(App& app) {
    app.output = GetStdHandle(STD_OUTPUT_HANDLE);
    app.input = GetStdHandle(STD_INPUT_HANDLE);
    if (app.output == INVALID_HANDLE_VALUE || app.input == INVALID_HANDLE_VALUE) {
        return false;
    }

    app.buffer.assign(SCREEN_WIDTH * SCREEN_HEIGHT, CHAR_INFO{});

    // SetConsoleScreenBufferSize changes the backing buffer dimensions. The
    // visible console window cannot be larger than its buffer, so the buffer is
    // set before the window rectangle is requested below.
    COORD size{ SCREEN_WIDTH, SCREEN_HEIGHT };
    SetConsoleScreenBufferSize(app.output, size);

    // SetConsoleWindowInfo requests the visible viewport. The call is allowed to
    // fail on terminals that virtualize console windows, so this app treats it
    // as a best-effort layout improvement instead of a fatal requirement.
    SMALL_RECT window{ 0, 0, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1 };
    SetConsoleWindowInfo(app.output, TRUE, &window);

    SetConsoleTitleW(L"Missionaries and Cannibals - Win32 Console");
    SetCursorVisible(app.output, false);

    // ReadConsoleInputW gives structured KEY_EVENT_RECORD values instead of raw
    // bytes. That means function keys, arrows, Escape, and Unicode characters
    // can be handled consistently. We disable line/echo modes so key presses are
    // delivered immediately and are not printed into the console by Windows.
    DWORD inputMode = 0;
    GetConsoleMode(app.input, &inputMode);
    inputMode &= ~(ENABLE_LINE_INPUT | ENABLE_ECHO_INPUT | ENABLE_PROCESSED_INPUT);
    inputMode |= ENABLE_WINDOW_INPUT;
    SetConsoleMode(app.input, inputMode);

    return true;
}

void HandleKey(App& app, const KEY_EVENT_RECORD& key) {
    if (!key.bKeyDown) {
        return;
    }

    const wchar_t ch = static_cast<wchar_t>(towupper(key.uChar.UnicodeChar));
    if (ch == L'Q' || key.wVirtualKeyCode == VK_ESCAPE) {
        app.running = false;
        return;
    }
    if (ch == L'R') {
        Reset(app);
        return;
    }
    if (ch == L'H') {
        ApplyHint(app);
        return;
    }

    for (const Move& move : MOVES) {
        if (ch == move.hotkey) {
            TryApplyMove(app, move, false);
            return;
        }
    }
}

void RunLoop(App& app) {
    DrawScene(app);
    Flush(app);

    while (app.running) {
        INPUT_RECORD record{};
        DWORD recordsRead = 0;

        // This blocking call is enough for a turn-based puzzle. An arcade game
        // would usually poll or wait with a timeout, but here the screen changes
        // only when the user presses a key, so blocking keeps CPU usage near zero.
        if (!ReadConsoleInputW(app.input, &record, 1, &recordsRead) || recordsRead == 0) {
            continue;
        }

        if (record.EventType == KEY_EVENT) {
            HandleKey(app, record.Event.KeyEvent);
            DrawScene(app);
            Flush(app);
        }
    }
}

} // namespace

int missionaries_cannibals_game::MissionariesCannibalsGame::Run() {
    // This puzzle keeps its App object intact because history, hint generation,
    // blocking ReadConsoleInputW handling, and rendering all belong to one
    // turn-based workflow. The real-time games use ConsoleScreen instead.
    App app;
    app.solutionPath = BuildSolutionPath();

    if (!ConfigureConsole(app)) {
        return 1;
    }

    RunLoop(app);
    SetCursorVisible(app.output, true);
    return 0;
}
