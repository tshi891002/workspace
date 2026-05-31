#include <windows.h>

#include <algorithm>
#include <array>
#include <string>
#include <string_view>
#include <vector>

#include "games/tictactoe.h"
#include "common/console_screen.h"

namespace {

constexpr int SCREEN_WIDTH = 64;
constexpr int SCREEN_HEIGHT = 24;
constexpr int GRID_SIZE = 3;
constexpr int BOARD_SIZE = GRID_SIZE * GRID_SIZE;
constexpr int BOARD_LEFT = 20;
constexpr int BOARD_TOP = 6;
constexpr int CELL_WIDTH = 7;
constexpr int CELL_HEIGHT = 3;

constexpr WORD COLOR_NORMAL = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
constexpr WORD COLOR_DIM = FOREGROUND_INTENSITY;
constexpr WORD COLOR_TITLE = FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
constexpr WORD COLOR_X = FOREGROUND_RED | FOREGROUND_INTENSITY;
constexpr WORD COLOR_O = FOREGROUND_GREEN | FOREGROUND_INTENSITY;
constexpr WORD COLOR_CURSOR = BACKGROUND_BLUE | COLOR_NORMAL | FOREGROUND_INTENSITY;
constexpr WORD COLOR_WIN = BACKGROUND_GREEN | COLOR_NORMAL | FOREGROUND_INTENSITY;
constexpr WORD COLOR_ERROR = FOREGROUND_RED | FOREGROUND_INTENSITY;

using Board = std::array<wchar_t, BOARD_SIZE>;
using WinningLine = std::array<int, GRID_SIZE>;

constexpr std::array<WinningLine, 8> WINNING_LINES{{
    {{0, 1, 2}}, {{3, 4, 5}}, {{6, 7, 8}},
    {{0, 3, 6}}, {{1, 4, 7}}, {{2, 5, 8}},
    {{0, 4, 8}}, {{2, 4, 6}},
}};

using Console = console_ui::ConsoleScreen;

struct GameState {
    Board board{};
    wchar_t currentPlayer = L'X';
    int selected = 0;
    std::wstring message;
};

bool InitializeConsole(Console& console) {
    return console.Initialize(SCREEN_WIDTH, SCREEN_HEIGHT, true);
}

void Put(Console& console, int x, int y, wchar_t ch, WORD color = COLOR_NORMAL) {
    console.Put(x, y, ch, color);
}

void WriteText(Console& console, int x, int y, std::wstring_view text, WORD color = COLOR_NORMAL) {
    console.WriteText(x, y, text, color);
}

void Clear(Console& console) {
    console.Clear();
}

void Flush(Console& console) {
    console.Flush();
}

void CleanupConsole(Console& console) {
    console.Cleanup();
}

bool IsFull(const Board& board) {
    return std::none_of(board.begin(), board.end(), [](wchar_t cell) {
        return cell == L' ';
    });
}

wchar_t Winner(const Board& board, WinningLine& winningLine) {
    for (const WinningLine& line : WINNING_LINES) {
        const wchar_t first = board[line[0]];
        if (first != L' ' && first == board[line[1]] && first == board[line[2]]) {
            winningLine = line;
            return first;
        }
    }

    winningLine = {{-1, -1, -1}};
    return L' ';
}

bool IsWinningSquare(int square, const WinningLine& winningLine) {
    return std::find(winningLine.begin(), winningLine.end(), square) != winningLine.end();
}

bool GameOver(const Board& board) {
    WinningLine winningLine{};
    return Winner(board, winningLine) != L' ' || IsFull(board);
}

WORD MarkColor(wchar_t mark, bool highlighted, bool winning) {
    if (winning) {
        return COLOR_WIN;
    }
    if (highlighted) {
        return COLOR_CURSOR;
    }
    if (mark == L'X') {
        return COLOR_X;
    }
    if (mark == L'O') {
        return COLOR_O;
    }
    return COLOR_DIM;
}

void DrawBoard(Console& console, const Board& board, int selected, const WinningLine& winningLine) {
    for (int x = 0; x < CELL_WIDTH * GRID_SIZE + GRID_SIZE - 1; ++x) {
        Put(console, BOARD_LEFT + x, BOARD_TOP + CELL_HEIGHT, L'-', COLOR_DIM);
        Put(console, BOARD_LEFT + x, BOARD_TOP + CELL_HEIGHT * 2 + 1, L'-', COLOR_DIM);
    }

    for (int y = 0; y < CELL_HEIGHT * GRID_SIZE + GRID_SIZE - 1; ++y) {
        Put(console, BOARD_LEFT + CELL_WIDTH, BOARD_TOP + y, L'|', COLOR_DIM);
        Put(console, BOARD_LEFT + CELL_WIDTH * 2 + 1, BOARD_TOP + y, L'|', COLOR_DIM);
    }

    Put(console, BOARD_LEFT + CELL_WIDTH, BOARD_TOP + CELL_HEIGHT, L'+', COLOR_DIM);
    Put(console, BOARD_LEFT + CELL_WIDTH * 2 + 1, BOARD_TOP + CELL_HEIGHT, L'+', COLOR_DIM);
    Put(console, BOARD_LEFT + CELL_WIDTH, BOARD_TOP + CELL_HEIGHT * 2 + 1, L'+', COLOR_DIM);
    Put(console, BOARD_LEFT + CELL_WIDTH * 2 + 1, BOARD_TOP + CELL_HEIGHT * 2 + 1, L'+', COLOR_DIM);

    for (int index = 0; index < BOARD_SIZE; ++index) {
        const int row = index / GRID_SIZE;
        const int col = index % GRID_SIZE;
        const int x = BOARD_LEFT + col * (CELL_WIDTH + 1) + CELL_WIDTH / 2;
        const int y = BOARD_TOP + row * (CELL_HEIGHT + 1) + CELL_HEIGHT / 2;
        const bool highlighted = index == selected;
        const bool winning = IsWinningSquare(index, winningLine);
        const wchar_t display = board[index] == L' ' ? static_cast<wchar_t>(L'1' + index) : board[index];

        if (highlighted || winning) {
            for (int fillY = y - 1; fillY <= y + 1; ++fillY) {
                for (int fillX = x - 2; fillX <= x + 2; ++fillX) {
                    Put(console, fillX, fillY, L' ', winning ? COLOR_WIN : COLOR_CURSOR);
                }
            }
        }

        Put(console, x, y, display, MarkColor(board[index], highlighted, winning));
    }
}

void Draw(Console& console, const GameState& game) {
    WinningLine winningLine{};
    const wchar_t winner = Winner(game.board, winningLine);

    Clear(console);
    WriteText(console, 25, 1, L"Tic-Tac-Toe", COLOR_TITLE);
    WriteText(console, 18, 3, L"Arrows or 1-9: select a square", COLOR_DIM);
    WriteText(console, 12, 4, L"Enter/Space: place   R: restart   Esc: quit", COLOR_DIM);
    DrawBoard(console, game.board, game.selected, winningLine);

    if (winner != L' ') {
        std::wstring result = L"Player ";
        result += winner;
        result += L" wins! Press R to play again or Esc to quit.";
        WriteText(console, 8, 20, result, COLOR_WIN);
    } else if (IsFull(game.board)) {
        WriteText(console, 10, 20, L"It's a draw. Press R to play again or Esc to quit.", COLOR_TITLE);
    } else {
        std::wstring turn = L"Player ";
        turn += game.currentPlayer;
        turn += L"'s turn";
        WriteText(console, 25, 18, turn, game.currentPlayer == L'X' ? COLOR_X : COLOR_O);
        WriteText(console, 8, 20, game.message, COLOR_ERROR);
    }

    Flush(console);
}

int ReadKey(HANDLE input) {
    INPUT_RECORD record{};
    DWORD recordsRead = 0;

    while (ReadConsoleInputW(input, &record, 1, &recordsRead)) {
        if (record.EventType != KEY_EVENT || !record.Event.KeyEvent.bKeyDown) {
            continue;
        }

        const KEY_EVENT_RECORD& key = record.Event.KeyEvent;
        return key.uChar.UnicodeChar != 0 ? key.uChar.UnicodeChar : key.wVirtualKeyCode;
    }

    return VK_ESCAPE;
}

void Reset(GameState& game) {
    game.board.fill(L' ');
    game.currentPlayer = L'X';
    game.selected = 0;
    game.message.clear();
}

void MoveSelection(int key, int& selected) {
    const int row = selected / GRID_SIZE;
    const int col = selected % GRID_SIZE;

    switch (key) {
    case VK_LEFT:
        selected = row * GRID_SIZE + (col + GRID_SIZE - 1) % GRID_SIZE;
        break;
    case VK_RIGHT:
        selected = row * GRID_SIZE + (col + 1) % GRID_SIZE;
        break;
    case VK_UP:
        selected = ((row + GRID_SIZE - 1) % GRID_SIZE) * GRID_SIZE + col;
        break;
    case VK_DOWN:
        selected = ((row + 1) % GRID_SIZE) * GRID_SIZE + col;
        break;
    }
}

void PlaceMark(GameState& game) {
    if (GameOver(game.board)) {
        return;
    }
    if (game.board[game.selected] != L' ') {
        game.message = L"That square is already taken.";
        return;
    }

    game.board[game.selected] = game.currentPlayer;
    if (!GameOver(game.board)) {
        game.currentPlayer = game.currentPlayer == L'X' ? L'O' : L'X';
    }
}

}  // namespace

int tictactoe_game::TicTacToeGame::Run() {
    // Tic-Tac-Toe is turn-based, so ReadKey blocks on ReadConsoleInputW until a
    // key-down event arrives. Passing true configures the input handle while the
    // shared ConsoleScreen object continues to own visual setup and restoration.
    Console console{};
    if (!InitializeConsole(console)) {
        CleanupConsole(console);
        return 1;
    }

    GameState game{};
    Reset(game);

    bool running = true;
    while (running) {
        Draw(console, game);
        game.message.clear();

        const int key = ReadKey(console.input);
        if (key == VK_ESCAPE) {
            running = false;
        } else if (key == L'r' || key == L'R') {
            Reset(game);
        } else if (key >= L'1' && key <= L'9') {
            game.selected = key - L'1';
        } else if (key == VK_LEFT || key == VK_RIGHT || key == VK_UP || key == VK_DOWN) {
            MoveSelection(key, game.selected);
        } else if (key == VK_RETURN || key == L' ') {
            PlaceMark(game);
        }
    }

    CleanupConsole(console);
    return 0;
}
