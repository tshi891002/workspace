#include <windows.h>

#include <algorithm>
#include <array>
#include <string>
#include <vector>

namespace {

// The game draws directly into a fixed-size Win32 console screen buffer.
// Using a known width/height keeps all board coordinates deterministic, so the
// render code can address cells by x/y position instead of relying on scrolling
// text output through std::cout.
constexpr int SCREEN_WIDTH = 64;
constexpr int SCREEN_HEIGHT = 24;

// Board layout constants. The board itself is drawn as a 3x3 grid where each
// logical square has a small rectangular region around it. That gives us room to
// highlight the selected cell or winning cells with background colors.
constexpr int BOARD_LEFT = 20;
constexpr int BOARD_TOP = 6;
constexpr int CELL_WIDTH = 7;
constexpr int CELL_HEIGHT = 3;

// Console colors are bit masks understood by the Win32 console API. Foreground
// bits set text color, background bits set the cell background color, and
// FOREGROUND_INTENSITY makes the chosen color brighter.
constexpr WORD COLOR_NORMAL = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
constexpr WORD COLOR_DIM = FOREGROUND_INTENSITY;
constexpr WORD COLOR_TITLE = FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
constexpr WORD COLOR_X = FOREGROUND_RED | FOREGROUND_INTENSITY;
constexpr WORD COLOR_O = FOREGROUND_GREEN | FOREGROUND_INTENSITY;
constexpr WORD COLOR_CURSOR = BACKGROUND_BLUE | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
constexpr WORD COLOR_WIN = BACKGROUND_GREEN | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
constexpr WORD COLOR_ERROR = FOREGROUND_RED | FOREGROUND_INTENSITY;

using Board = std::array<wchar_t, 9>;

// Handles and the frame buffer are kept together because all drawing routines
// need the same three things: the output handle, input handle, and off-screen
// character buffer. The buffer is the important part: we draw the whole frame in
// memory first, then copy it to the console in one Win32 call to avoid flicker.
struct Console {
    HANDLE output = INVALID_HANDLE_VALUE;
    HANDLE input = INVALID_HANDLE_VALUE;
    std::vector<CHAR_INFO> buffer;
};

void SetCursorVisible(HANDLE console, bool visible) {
    // CONSOLE_CURSOR_INFO controls the blinking text cursor. This game is
    // rendered like a tiny UI, so the default cursor would only distract from
    // the highlighted board selection.
    CONSOLE_CURSOR_INFO cursorInfo{};
    cursorInfo.dwSize = 1;
    cursorInfo.bVisible = visible ? TRUE : FALSE;
    SetConsoleCursorInfo(console, &cursorInfo);
}

void Put(Console& console, int x, int y, wchar_t ch, WORD color = COLOR_NORMAL) {
    // Bounds-check every write so higher-level drawing code can be simple.
    // CHAR_INFO stores both the Unicode character and its color attributes for
    // one console cell.
    if (x < 0 || x >= SCREEN_WIDTH || y < 0 || y >= SCREEN_HEIGHT) {
        return;
    }

    CHAR_INFO& cell = console.buffer[y * SCREEN_WIDTH + x];
    cell.Char.UnicodeChar = ch;
    cell.Attributes = color;
}

void WriteText(Console& console, int x, int y, const std::wstring& text, WORD color = COLOR_NORMAL) {
    // Text is just repeated cell writes. Wide strings are used because this
    // file consistently talks to the W-suffixed Win32 Unicode APIs.
    for (size_t i = 0; i < text.size(); ++i) {
        Put(console, x + static_cast<int>(i), y, text[i], color);
    }
}

void Clear(Console& console) {
    // Reset every cell in the off-screen frame buffer to a blank space before
    // drawing a new frame. Nothing appears on screen until Flush is called.
    CHAR_INFO blank{};
    blank.Char.UnicodeChar = L' ';
    blank.Attributes = COLOR_NORMAL;
    std::fill(console.buffer.begin(), console.buffer.end(), blank);
}

void Flush(Console& console) {
    // WriteConsoleOutputW copies a rectangular block of CHAR_INFO cells into the
    // actual console screen buffer. This is the main Win32 rendering trick used
    // here: batch a full frame into one API call instead of printing one
    // character at a time.
    COORD bufferSize = { SCREEN_WIDTH, SCREEN_HEIGHT };
    COORD bufferCoord = { 0, 0 };
    SMALL_RECT writeRegion = { 0, 0, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1 };
    WriteConsoleOutputW(console.output, console.buffer.data(), bufferSize, bufferCoord, &writeRegion);
}

bool IsFull(const Board& board) {
    // A full board without a winner is a draw. The board uses a space character
    // as the sentinel for "empty square".
    for (wchar_t cell : board) {
        if (cell == L' ') {
            return false;
        }
    }
    return true;
}

wchar_t Winner(const Board& board, std::array<int, 3>& winningLine) {
    // Tic-tac-toe has exactly eight possible winning lines: three rows, three
    // columns, and two diagonals. Returning the winning line lets the renderer
    // color those squares differently after the game ends.
    constexpr int lines[8][3] = {
        {0, 1, 2}, {3, 4, 5}, {6, 7, 8},
        {0, 3, 6}, {1, 4, 7}, {2, 5, 8},
        {0, 4, 8}, {2, 4, 6}
    };

    for (const auto& line : lines) {
        const wchar_t first = board[line[0]];
        if (first != L' ' && first == board[line[1]] && first == board[line[2]]) {
            winningLine = { line[0], line[1], line[2] };
            return first;
        }
    }

    winningLine = { -1, -1, -1 };
    return L' ';
}

bool IsWinningSquare(int square, const std::array<int, 3>& winningLine) {
    // Helper used by the renderer so the game-state code and visual highlighting
    // stay loosely coupled.
    return square == winningLine[0] || square == winningLine[1] || square == winningLine[2];
}

WORD MarkColor(wchar_t mark, bool highlighted, bool winning) {
    // Rendering priority matters: winning squares keep their green background
    // even if the cursor is on top of them; otherwise the selected square uses a
    // blue background; normal X/O marks use player colors.
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

void DrawBoard(Console& console, const Board& board, int selected, const std::array<int, 3>& winningLine) {
    // First draw the grid lines. The board is deliberately ASCII-compatible so
    // it displays correctly on old Windows consoles without depending on box
    // drawing glyph support.
    for (int x = 0; x < CELL_WIDTH * 3 + 2; ++x) {
        Put(console, BOARD_LEFT + x, BOARD_TOP + CELL_HEIGHT, L'-', COLOR_DIM);
        Put(console, BOARD_LEFT + x, BOARD_TOP + CELL_HEIGHT * 2 + 1, L'-', COLOR_DIM);
    }

    for (int y = 0; y < CELL_HEIGHT * 3 + 2; ++y) {
        Put(console, BOARD_LEFT + CELL_WIDTH, BOARD_TOP + y, L'|', COLOR_DIM);
        Put(console, BOARD_LEFT + CELL_WIDTH * 2 + 1, BOARD_TOP + y, L'|', COLOR_DIM);
    }

    Put(console, BOARD_LEFT + CELL_WIDTH, BOARD_TOP + CELL_HEIGHT, L'+', COLOR_DIM);
    Put(console, BOARD_LEFT + CELL_WIDTH * 2 + 1, BOARD_TOP + CELL_HEIGHT, L'+', COLOR_DIM);
    Put(console, BOARD_LEFT + CELL_WIDTH, BOARD_TOP + CELL_HEIGHT * 2 + 1, L'+', COLOR_DIM);
    Put(console, BOARD_LEFT + CELL_WIDTH * 2 + 1, BOARD_TOP + CELL_HEIGHT * 2 + 1, L'+', COLOR_DIM);

    // Then draw each logical square. Empty squares show their 1-9 shortcut key;
    // occupied squares show X or O. Highlighting fills a small rectangle behind
    // the mark so the current selection is obvious.
    for (int index = 0; index < 9; ++index) {
        const int row = index / 3;
        const int col = index % 3;
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

void Draw(Console& console, const Board& board, int selected, wchar_t currentPlayer, const std::wstring& message) {
    // Draw is pure presentation: it derives the visible state from the board,
    // selected square, current player, and one transient status message. Keeping
    // this separate from input handling makes the main loop easy to follow.
    std::array<int, 3> winningLine{};
    const wchar_t winner = Winner(board, winningLine);

    Clear(console);
    WriteText(console, 25, 1, L"Tic-Tac-Toe", COLOR_TITLE);
    WriteText(console, 9, 3, L"Use arrow keys or 1-9. Press Enter/Space to place. R restarts. Esc quits.", COLOR_DIM);

    DrawBoard(console, board, selected, winningLine);

    if (winner != L' ') {
        std::wstring result = L"Player ";
        result += winner;
        result += L" wins! Press R to play again or Esc to quit.";
        WriteText(console, 8, 20, result, COLOR_WIN);
    } else if (IsFull(board)) {
        WriteText(console, 10, 20, L"It's a draw. Press R to play again or Esc to quit.", COLOR_TITLE);
    } else {
        std::wstring turn = L"Player ";
        turn += currentPlayer;
        turn += L"'s turn";
        WriteText(console, 25, 18, turn, currentPlayer == L'X' ? COLOR_X : COLOR_O);
        WriteText(console, 8, 20, message, message.empty() ? COLOR_NORMAL : COLOR_ERROR);
    }

    Flush(console);
}

int ReadKey(HANDLE input) {
    // ReadConsoleInputW exposes low-level input events, including virtual key
    // codes for arrows and Unicode characters for regular keys. We loop until a
    // key-down event arrives so mouse/window events do not affect gameplay.
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

void Reset(Board& board, wchar_t& currentPlayer, int& selected, std::wstring& message) {
    // Reset all mutable game state. X starts every new round, and the cursor
    // returns to the top-left square for a predictable restart.
    board.fill(L' ');
    currentPlayer = L'X';
    selected = 0;
    message.clear();
}

void MoveSelection(int key, int& selected) {
    // The cursor wraps around the board edges. This keeps navigation quick in a
    // tiny 3x3 grid and avoids dead-end key presses at the borders.
    const int row = selected / 3;
    const int col = selected % 3;

    switch (key) {
    case VK_LEFT:
        selected = row * 3 + (col + 2) % 3;
        break;
    case VK_RIGHT:
        selected = row * 3 + (col + 1) % 3;
        break;
    case VK_UP:
        selected = ((row + 2) % 3) * 3 + col;
        break;
    case VK_DOWN:
        selected = ((row + 1) % 3) * 3 + col;
        break;
    default:
        break;
    }
}

bool GameOver(const Board& board) {
    // Centralized end-state check used by input handling to prevent additional
    // moves after a win or draw.
    std::array<int, 3> winningLine{};
    return Winner(board, winningLine) != L' ' || IsFull(board);
}

}  // namespace

int main() {
    // Get the standard Win32 console handles. The output handle is used for
    // WriteConsoleOutputW rendering; the input handle is used for event-based
    // keyboard input through ReadConsoleInputW.
    Console console{};
    console.output = GetStdHandle(STD_OUTPUT_HANDLE);
    console.input = GetStdHandle(STD_INPUT_HANDLE);
    if (console.output == INVALID_HANDLE_VALUE || console.input == INVALID_HANDLE_VALUE) {
        return 1;
    }

    console.buffer.resize(SCREEN_WIDTH * SCREEN_HEIGHT);

    // Preserve the user's original input mode so it can be restored on exit.
    // ENABLE_WINDOW_INPUT lets the input stream report console window events;
    // ReadKey simply ignores those and waits for real key presses.
    DWORD oldInputMode = 0;
    GetConsoleMode(console.input, &oldInputMode);
    SetConsoleMode(console.input, ENABLE_WINDOW_INPUT);

    // Resize the console buffer and visible window to match the render target.
    // The buffer is set first because Windows requires the screen buffer to be
    // at least as large as the visible window rectangle.
    SMALL_RECT windowSize = { 0, 0, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1 };
    COORD bufferSize = { SCREEN_WIDTH, SCREEN_HEIGHT };
    SetConsoleScreenBufferSize(console.output, bufferSize);
    SetConsoleWindowInfo(console.output, TRUE, &windowSize);
    SetCursorVisible(console.output, false);

    Board board{};
    wchar_t currentPlayer = L'X';
    int selected = 0;
    std::wstring message;
    Reset(board, currentPlayer, selected, message);

    bool running = true;
    while (running) {
        // The loop follows a simple UI-game pattern:
        // 1. Render the whole current state.
        // 2. Read one key event.
        // 3. Mutate game state based on that key.
        // 4. Repeat until Escape exits.
        Draw(console, board, selected, currentPlayer, message);
        message.clear();

        const int key = ReadKey(console.input);
        if (key == VK_ESCAPE) {
            running = false;
        } else if (key == L'r' || key == L'R') {
            Reset(board, currentPlayer, selected, message);
        } else if (key >= L'1' && key <= L'9') {
            selected = key - L'1';
        } else if (key == VK_LEFT || key == VK_RIGHT || key == VK_UP || key == VK_DOWN) {
            MoveSelection(key, selected);
        } else if ((key == VK_RETURN || key == L' ') && !GameOver(board)) {
            if (board[selected] != L' ') {
                message = L"That square is already taken.";
            } else {
                // A successful move claims the selected square, then switches
                // turns only if the move did not immediately end the game.
                board[selected] = currentPlayer;
                if (!GameOver(board)) {
                    currentPlayer = currentPlayer == L'X' ? L'O' : L'X';
                }
            }
        }
    }

    // Leave the console in a tidy state: clear the game frame, restore the
    // cursor, and put the input mode back exactly as it was before launch.
    Clear(console);
    Flush(console);
    SetCursorVisible(console.output, true);
    SetConsoleMode(console.input, oldInputMode);
    return 0;
}
