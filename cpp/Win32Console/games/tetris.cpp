#include <windows.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <random>
#include <string>
#include <string_view>
#include <thread>
#include <vector>

#include "games/tetris.h"
#include "common/console_screen.h"

namespace {

constexpr int SCREEN_WIDTH = 80;
constexpr int SCREEN_HEIGHT = 30;
constexpr int BOARD_WIDTH = 10;
constexpr int BOARD_HEIGHT = 20;
constexpr int BOARD_LEFT = 24;
constexpr int BOARD_TOP = 4;
constexpr int PREVIEW_LEFT = 52;
constexpr int PREVIEW_TOP = 7;
constexpr int EMPTY_CELL = -1;
constexpr int FRAME_DELAY_MS = 16;

constexpr WORD COLOR_NORMAL = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
constexpr WORD COLOR_BRIGHT = COLOR_NORMAL | FOREGROUND_INTENSITY;
constexpr WORD COLOR_TITLE = FOREGROUND_GREEN | FOREGROUND_INTENSITY;
constexpr WORD COLOR_BORDER = FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
constexpr WORD COLOR_ERROR = FOREGROUND_RED | FOREGROUND_INTENSITY;

struct Point {
    int x = 0;
    int y = 0;
};

struct Piece {
    int type = 0;
    int rotation = 0;
    int x = 3;
    int y = -1;
};

using Board = std::array<int, BOARD_WIDTH * BOARD_HEIGHT>;
using Blocks = std::array<Point, 4>;
using Clock = std::chrono::steady_clock;

constexpr std::array<Blocks, 7> SHAPES{{
    {{{0, 1}, {1, 1}, {2, 1}, {3, 1}}}, // I
    {{{1, 0}, {2, 0}, {1, 1}, {2, 1}}}, // O
    {{{1, 0}, {0, 1}, {1, 1}, {2, 1}}}, // T
    {{{1, 0}, {2, 0}, {0, 1}, {1, 1}}}, // S
    {{{0, 0}, {1, 0}, {1, 1}, {2, 1}}}, // Z
    {{{0, 0}, {0, 1}, {1, 1}, {2, 1}}}, // J
    {{{2, 0}, {0, 1}, {1, 1}, {2, 1}}}, // L
}};

constexpr std::array<WORD, 7> PIECE_COLORS{{
    FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY,
    FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY,
    FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_INTENSITY,
    FOREGROUND_GREEN | FOREGROUND_INTENSITY,
    FOREGROUND_RED | FOREGROUND_INTENSITY,
    FOREGROUND_BLUE | FOREGROUND_INTENSITY,
    FOREGROUND_RED | FOREGROUND_GREEN,
}};

constexpr std::array<int, 5> LINE_SCORES{{0, 100, 300, 500, 800}};

using Console = console_ui::ConsoleScreen;

struct Game {
    Board board{};
    Piece current;
    int nextType = 0;
    int score = 0;
    int lines = 0;
    int level = 1;
    bool paused = false;
    bool gameOver = false;
    bool running = true;
    Clock::time_point lastDrop = Clock::now();
};

bool InitializeConsole(Console& console) {
    return console.Initialize(SCREEN_WIDTH, SCREEN_HEIGHT, false);
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

void Put(Console& console, int x, int y, wchar_t ch, WORD color = COLOR_NORMAL) {
    console.Put(x, y, ch, color);
}

void WriteText(Console& console, int x, int y, std::wstring_view text, WORD color = COLOR_NORMAL) {
    console.WriteText(x, y, text, color);
}

Point RotatePoint(Point point, int rotation) {
    for (int i = 0; i < rotation % 4; ++i) {
        point = {3 - point.y, point.x};
    }
    return point;
}

Blocks GetPieceBlocks(const Piece& piece) {
    Blocks blocks{};
    for (size_t i = 0; i < blocks.size(); ++i) {
        const Point rotated = RotatePoint(SHAPES[piece.type][i], piece.rotation);
        blocks[i] = {piece.x + rotated.x, piece.y + rotated.y};
    }
    return blocks;
}

bool IsBlocked(const Board& board, const Piece& piece) {
    for (const Point& block : GetPieceBlocks(piece)) {
        if (block.x < 0 || block.x >= BOARD_WIDTH || block.y >= BOARD_HEIGHT) {
            return true;
        }
        if (block.y >= 0 && board[block.y * BOARD_WIDTH + block.x] != EMPTY_CELL) {
            return true;
        }
    }
    return false;
}

Piece CreatePiece(int type) {
    Piece piece{};
    piece.type = type;
    return piece;
}

int ClearLines(Board& board) {
    int cleared = 0;
    for (int y = BOARD_HEIGHT - 1; y >= 0; --y) {
        const auto rowBegin = board.begin() + y * BOARD_WIDTH;
        const auto rowEnd = rowBegin + BOARD_WIDTH;
        if (std::find(rowBegin, rowEnd, EMPTY_CELL) != rowEnd) {
            continue;
        }

        ++cleared;
        std::move_backward(board.begin(), rowBegin, rowEnd);
        std::fill(board.begin(), board.begin() + BOARD_WIDTH, EMPTY_CELL);
        ++y;
    }
    return cleared;
}

void Reset(Game& game, std::mt19937& rng) {
    std::uniform_int_distribution<int> pieceDistribution(0, static_cast<int>(SHAPES.size()) - 1);
    game.board.fill(EMPTY_CELL);
    game.current = CreatePiece(pieceDistribution(rng));
    game.nextType = pieceDistribution(rng);
    game.score = 0;
    game.lines = 0;
    game.level = 1;
    game.paused = false;
    game.gameOver = false;
    game.running = true;
    game.lastDrop = Clock::now();
}

int RandomPiece(std::mt19937& rng) {
    std::uniform_int_distribution<int> distribution(0, static_cast<int>(SHAPES.size()) - 1);
    return distribution(rng);
}

void SpawnNext(Game& game, std::mt19937& rng) {
    game.current = CreatePiece(game.nextType);
    game.nextType = RandomPiece(rng);
    game.gameOver = IsBlocked(game.board, game.current);
}

bool TryMove(Game& game, int dx, int dy) {
    Piece moved = game.current;
    moved.x += dx;
    moved.y += dy;
    if (IsBlocked(game.board, moved)) {
        return false;
    }

    game.current = moved;
    return true;
}

void LockPiece(Game& game, std::mt19937& rng) {
    for (const Point& block : GetPieceBlocks(game.current)) {
        if (block.y >= 0 && block.y < BOARD_HEIGHT && block.x >= 0 && block.x < BOARD_WIDTH) {
            game.board[block.y * BOARD_WIDTH + block.x] = game.current.type;
        }
    }

    const int cleared = ClearLines(game.board);
    game.score += LINE_SCORES[cleared] * game.level;
    game.lines += cleared;
    game.level = 1 + game.lines / 10;
    SpawnNext(game, rng);
}

void TryRotate(Game& game) {
    Piece rotated = game.current;
    rotated.rotation = (rotated.rotation + 1) % 4;

    for (int offset : {0, -1, 1}) {
        rotated.x = game.current.x + offset;
        if (!IsBlocked(game.board, rotated)) {
            game.current = rotated;
            return;
        }
    }
}

int DropDelayForLevel(int level) {
    return (std::max)(90, 520 - (level - 1) * 45);
}

bool WasPressed(int key) {
    return (GetAsyncKeyState(key) & 0x0001) != 0;
}

void HandleInput(Game& game, std::mt19937& rng) {
    if (WasPressed(VK_ESCAPE)) {
        game.running = false;
        return;
    }
    if (WasPressed('P') && !game.gameOver) {
        game.paused = !game.paused;
    }
    if (game.gameOver) {
        if (WasPressed('R')) {
            Reset(game, rng);
        }
        return;
    }
    if (game.paused) {
        return;
    }

    if (WasPressed(VK_LEFT)) {
        TryMove(game, -1, 0);
    }
    if (WasPressed(VK_RIGHT)) {
        TryMove(game, 1, 0);
    }
    if (WasPressed(VK_UP) || WasPressed('X')) {
        TryRotate(game);
    }
    if ((GetAsyncKeyState(VK_DOWN) & 0x8000) != 0) {
        if (TryMove(game, 0, 1)) {
            ++game.score;
        }
        game.lastDrop = Clock::now();
    }
    if (WasPressed(VK_SPACE)) {
        while (TryMove(game, 0, 1)) {
            game.score += 2;
        }
        LockPiece(game, rng);
        game.lastDrop = Clock::now();
    }
}

void Step(Game& game, std::mt19937& rng) {
    if (game.gameOver || game.paused) {
        return;
    }

    const auto now = Clock::now();
    const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(now - game.lastDrop).count();
    if (elapsed < DropDelayForLevel(game.level)) {
        return;
    }

    if (!TryMove(game, 0, 1)) {
        LockPiece(game, rng);
    }
    game.lastDrop = now;
}

void DrawBlock(Console& console, int boardX, int boardY, int type) {
    const int screenX = BOARD_LEFT + 1 + boardX * 2;
    const int screenY = BOARD_TOP + 1 + boardY;
    Put(console, screenX, screenY, L'[', PIECE_COLORS[type]);
    Put(console, screenX + 1, screenY, L']', PIECE_COLORS[type]);
}

void DrawPreviewBlock(Console& console, int x, int y, int type) {
    Put(console, PREVIEW_LEFT + x * 2, PREVIEW_TOP + y, L'[', PIECE_COLORS[type]);
    Put(console, PREVIEW_LEFT + x * 2 + 1, PREVIEW_TOP + y, L']', PIECE_COLORS[type]);
}

void DrawBorder(Console& console) {
    for (int y = 0; y <= BOARD_HEIGHT + 1; ++y) {
        Put(console, BOARD_LEFT, BOARD_TOP + y, L'|', COLOR_BORDER);
        Put(console, BOARD_LEFT + BOARD_WIDTH * 2 + 1, BOARD_TOP + y, L'|', COLOR_BORDER);
    }
    for (int x = 0; x <= BOARD_WIDTH * 2 + 1; ++x) {
        Put(console, BOARD_LEFT + x, BOARD_TOP, L'-', COLOR_BORDER);
        Put(console, BOARD_LEFT + x, BOARD_TOP + BOARD_HEIGHT + 1, L'-', COLOR_BORDER);
    }
    Put(console, BOARD_LEFT, BOARD_TOP, L'+', COLOR_BORDER);
    Put(console, BOARD_LEFT + BOARD_WIDTH * 2 + 1, BOARD_TOP, L'+', COLOR_BORDER);
    Put(console, BOARD_LEFT, BOARD_TOP + BOARD_HEIGHT + 1, L'+', COLOR_BORDER);
    Put(console, BOARD_LEFT + BOARD_WIDTH * 2 + 1, BOARD_TOP + BOARD_HEIGHT + 1, L'+', COLOR_BORDER);
}

void Draw(Console& console, const Game& game) {
    Clear(console);
    WriteText(console, 2, 1, L"Win32 Console Tetris", COLOR_TITLE);
    WriteText(console, 2, 3, L"Left/Right  Move");
    WriteText(console, 2, 4, L"Up or X     Rotate");
    WriteText(console, 2, 5, L"Down        Soft drop");
    WriteText(console, 2, 6, L"Space       Hard drop");
    WriteText(console, 2, 7, L"P           Pause");
    WriteText(console, 2, 8, L"Esc         Quit");
    DrawBorder(console);

    for (int y = 0; y < BOARD_HEIGHT; ++y) {
        for (int x = 0; x < BOARD_WIDTH; ++x) {
            const int type = game.board[y * BOARD_WIDTH + x];
            if (type != EMPTY_CELL) {
                DrawBlock(console, x, y, type);
            }
        }
    }

    for (const Point& block : GetPieceBlocks(game.current)) {
        if (block.y >= 0) {
            DrawBlock(console, block.x, block.y, game.current.type);
        }
    }

    WriteText(console, PREVIEW_LEFT, 3, L"Score", COLOR_TITLE);
    WriteText(console, PREVIEW_LEFT, 4, std::to_wstring(game.score), COLOR_BRIGHT);
    WriteText(console, PREVIEW_LEFT, 12, L"Lines", COLOR_TITLE);
    WriteText(console, PREVIEW_LEFT, 13, std::to_wstring(game.lines), COLOR_BRIGHT);
    WriteText(console, PREVIEW_LEFT, 15, L"Level", COLOR_TITLE);
    WriteText(console, PREVIEW_LEFT, 16, std::to_wstring(game.level), COLOR_BRIGHT);
    WriteText(console, PREVIEW_LEFT, 19, L"Next", COLOR_TITLE);

    Piece preview = CreatePiece(game.nextType);
    preview.x = 0;
    preview.y = 0;
    for (const Point& block : GetPieceBlocks(preview)) {
        DrawPreviewBlock(console, block.x, block.y, game.nextType);
    }

    if (game.paused) {
        WriteText(console, BOARD_LEFT + 6, BOARD_TOP + 10, L"PAUSED", COLOR_BRIGHT);
    }
    if (game.gameOver) {
        WriteText(console, BOARD_LEFT + 4, BOARD_TOP + 9, L"GAME OVER", COLOR_ERROR);
        WriteText(console, BOARD_LEFT + 2, BOARD_TOP + 11, L"Press R to restart", COLOR_BRIGHT);
    }

    Flush(console);
}

}  // namespace

int tetris_game::TetrisGame::Run() {
    // Tetris uses GetAsyncKeyState inside its timed update loop rather than
    // blocking on ReadConsoleInputW. Output-only initialization is sufficient:
    // ConsoleScreen still owns frame buffering, resizing, caret hiding, and
    // terminal restoration.
    Console console{};
    if (!InitializeConsole(console)) {
        CleanupConsole(console);
        return 1;
    }

    std::random_device randomDevice;
    std::mt19937 rng(randomDevice());
    Game game{};
    Reset(game, rng);

    while (game.running) {
        HandleInput(game, rng);
        Step(game, rng);
        Draw(console, game);
        std::this_thread::sleep_for(std::chrono::milliseconds(FRAME_DELAY_MS));
    }

    CleanupConsole(console);
    return 0;
}
