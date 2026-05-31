#include <windows.h>

#include <algorithm>
#include <chrono>
#include <deque>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include "games/snake.h"
#include "common/console_screen.h"

namespace {

// This Snake implementation treats the Windows console as a fixed-size grid of
// colored character cells. The game never scrolls text with std::cout; instead,
// it redraws a complete frame into a memory buffer and copies that buffer to the
// console with WriteConsoleOutputW.
constexpr int SCREEN_WIDTH = 80;
constexpr int SCREEN_HEIGHT = 28;

// The playfield is smaller than the full console so there is room for a title,
// score, and instructions. All snake and food coordinates live inside this
// rectangle, using grid coordinates rather than pixel coordinates.
constexpr int FIELD_LEFT = 4;
constexpr int FIELD_TOP = 4;
constexpr int FIELD_WIDTH = 50;
constexpr int FIELD_HEIGHT = 20;

// The target frame delay controls game speed. A smaller value makes the snake
// move faster. Because the game is grid-based, one frame equals one snake step.
constexpr int FRAME_DELAY_MS = 115;

// Win32 console colors are stored as WORD bit masks. Foreground bits color the
// character, background bits color the cell behind it, and INTENSITY brightens
// the chosen foreground/background color.
constexpr WORD COLOR_NORMAL = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
constexpr WORD COLOR_DIM = FOREGROUND_INTENSITY;
constexpr WORD COLOR_TITLE = FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
constexpr WORD COLOR_WALL = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
constexpr WORD COLOR_SNAKE_HEAD = BACKGROUND_GREEN | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
constexpr WORD COLOR_SNAKE_BODY = FOREGROUND_GREEN | FOREGROUND_INTENSITY;
constexpr WORD COLOR_FOOD = FOREGROUND_RED | FOREGROUND_INTENSITY;
constexpr WORD COLOR_GAME_OVER = FOREGROUND_RED | FOREGROUND_INTENSITY;

struct Vec2 {
    int x = 0;
    int y = 0;
};

enum class Direction {
    Up,
    Down,
    Left,
    Right
};

enum class Mode {
    Playing,
    GameOver
};

using Console = console_ui::ConsoleScreen;

struct Game {
    std::deque<Vec2> snake;
    Vec2 food;
    Direction direction = Direction::Right;
    Direction queuedDirection = Direction::Right;
    Mode mode = Mode::Playing;
    int score = 0;
    bool running = true;
};

bool operator==(Vec2 a, Vec2 b) {
    return a.x == b.x && a.y == b.y;
}

void Put(Console& console, int x, int y, wchar_t ch, WORD color = COLOR_NORMAL) {
    // CHAR_INFO represents one console cell: a Unicode character plus the color
    // attribute bits for that cell. The bounds check keeps all drawing helpers
    // safe even if a label or game object would otherwise spill off-screen.
    console.Put(x, y, ch, color);
}

void WriteText(Console& console, int x, int y, const std::wstring& text, WORD color = COLOR_NORMAL) {
    // Text output is still buffer-based. Each character is placed into the next
    // console cell, then Flush copies the entire frame to the real console.
    console.WriteText(x, y, text, color);
}

void Clear(Console& console) {
    // Clearing the memory buffer starts a new frame. The actual console window
    // is unchanged until WriteConsoleOutputW is called by Flush.
    console.Clear();
}

void Flush(Console& console) {
    // WriteConsoleOutputW copies a rectangle of CHAR_INFO cells to the console
    // screen buffer. This is the core Win32 rendering API for the game: one
    // batched write per frame avoids flicker from many smaller writes.
    console.Flush();
}

Vec2 DirectionDelta(Direction direction) {
    switch (direction) {
    case Direction::Up:
        return { 0, -1 };
    case Direction::Down:
        return { 0, 1 };
    case Direction::Left:
        return { -1, 0 };
    case Direction::Right:
        return { 1, 0 };
    }

    return { 0, 0 };
}

bool AreOpposite(Direction a, Direction b) {
    // Snake cannot reverse directly into itself. Blocking opposite directions
    // preserves the classic rule and prevents accidental instant losses.
    return (a == Direction::Up && b == Direction::Down) ||
           (a == Direction::Down && b == Direction::Up) ||
           (a == Direction::Left && b == Direction::Right) ||
           (a == Direction::Right && b == Direction::Left);
}

bool ContainsSnake(const Game& game, Vec2 point) {
    // Collision checks and food placement both need to know whether a grid cell
    // is occupied by any snake segment.
    return std::find(game.snake.begin(), game.snake.end(), point) != game.snake.end();
}

Vec2 RandomFoodPosition(const Game& game, std::mt19937& rng) {
    // Build a list of every empty interior cell, then choose one uniformly.
    // This avoids a retry loop that could run forever if the snake fills nearly
    // the whole board.
    std::vector<Vec2> emptyCells;
    for (int y = 1; y < FIELD_HEIGHT - 1; ++y) {
        for (int x = 1; x < FIELD_WIDTH - 1; ++x) {
            Vec2 candidate = { x, y };
            if (!ContainsSnake(game, candidate)) {
                emptyCells.push_back(candidate);
            }
        }
    }

    if (emptyCells.empty()) {
        return { -1, -1 };
    }

    std::uniform_int_distribution<int> indexDist(0, static_cast<int>(emptyCells.size() - 1));
    return emptyCells[static_cast<size_t>(indexDist(rng))];
}

void Reset(Game& game, std::mt19937& rng) {
    // All mutable gameplay state is reset here so the restart key and first
    // launch share the same initialization path.
    game.snake.clear();
    game.snake.push_back({ FIELD_WIDTH / 2, FIELD_HEIGHT / 2 });
    game.snake.push_back({ FIELD_WIDTH / 2 - 1, FIELD_HEIGHT / 2 });
    game.snake.push_back({ FIELD_WIDTH / 2 - 2, FIELD_HEIGHT / 2 });
    game.direction = Direction::Right;
    game.queuedDirection = Direction::Right;
    game.score = 0;
    game.mode = Mode::Playing;
    game.running = true;
    game.food = RandomFoodPosition(game, rng);
}

void QueueDirection(Game& game, Direction requested) {
    // Input is queued separately from the active direction. The queued direction
    // is applied at the next step, which keeps input handling independent from
    // the fixed update cadence.
    if (!AreOpposite(game.direction, requested)) {
        game.queuedDirection = requested;
    }
}

void DrainConsoleInput(HANDLE input) {
    // The game uses GetAsyncKeyState for real-time key polling. Console input
    // events can still accumulate in the input buffer, especially arrow keys,
    // so this drains pending events to keep the terminal tidy.
    INPUT_RECORD record{};
    DWORD recordsRead = 0;
    while (PeekConsoleInputW(input, &record, 1, &recordsRead) && recordsRead > 0) {
        ReadConsoleInputW(input, &record, 1, &recordsRead);
    }
}

bool Pressed(int virtualKey) {
    // GetAsyncKeyState asks Windows for the current physical key state without
    // blocking the game loop. The high bit is set while the key is down.
    return (GetAsyncKeyState(virtualKey) & 0x8000) != 0;
}

void HandleInput(Console& console, Game& game) {
    // Keyboard handling is intentionally nonblocking. Each frame polls the keys
    // that matter, updates game state if needed, then immediately returns so the
    // snake keeps moving even when the player is not pressing anything.
    DrainConsoleInput(console.input);

    if (Pressed(VK_ESCAPE)) {
        game.running = false;
        return;
    }

    if (Pressed('R')) {
        return;
    }

    if (game.mode == Mode::GameOver) {
        return;
    }

    if (Pressed(VK_UP) || Pressed('W')) {
        QueueDirection(game, Direction::Up);
    } else if (Pressed(VK_DOWN) || Pressed('S')) {
        QueueDirection(game, Direction::Down);
    } else if (Pressed(VK_LEFT) || Pressed('A')) {
        QueueDirection(game, Direction::Left);
    } else if (Pressed(VK_RIGHT) || Pressed('D')) {
        QueueDirection(game, Direction::Right);
    }
}

bool HitsWall(Vec2 head) {
    // The outer ring of the playfield is a wall. Valid snake body positions are
    // therefore x=[1, FIELD_WIDTH-2] and y=[1, FIELD_HEIGHT-2].
    return head.x <= 0 || head.x >= FIELD_WIDTH - 1 ||
           head.y <= 0 || head.y >= FIELD_HEIGHT - 1;
}

void Step(Game& game, std::mt19937& rng) {
    // One Step advances the simulation by exactly one grid cell. Rendering is
    // separate, so the rules can be understood without any console API details.
    if (game.mode == Mode::GameOver) {
        return;
    }

    game.direction = game.queuedDirection;
    const Vec2 delta = DirectionDelta(game.direction);
    const Vec2 nextHead = { game.snake.front().x + delta.x, game.snake.front().y + delta.y };
    const bool eatsFood = nextHead == game.food;

    // If the snake is not growing, its tail moves away during this step. Remove
    // the tail before self-collision testing so moving into the old tail square
    // is allowed, matching classic Snake behavior.
    Vec2 removedTail{};
    if (!eatsFood) {
        removedTail = game.snake.back();
        game.snake.pop_back();
    }

    if (HitsWall(nextHead) || ContainsSnake(game, nextHead)) {
        if (!eatsFood) {
            game.snake.push_back(removedTail);
        }
        game.mode = Mode::GameOver;
        return;
    }

    game.snake.push_front(nextHead);
    if (eatsFood) {
        game.score += 10;
        game.food = RandomFoodPosition(game, rng);
        if (game.food.x < 0) {
            game.mode = Mode::GameOver;
        }
    }
}

void DrawBorder(Console& console) {
    // The border is drawn in console-cell coordinates, offset by FIELD_LEFT and
    // FIELD_TOP to leave space around the playfield.
    for (int x = 0; x < FIELD_WIDTH; ++x) {
        Put(console, FIELD_LEFT + x, FIELD_TOP, L'#', COLOR_WALL);
        Put(console, FIELD_LEFT + x, FIELD_TOP + FIELD_HEIGHT - 1, L'#', COLOR_WALL);
    }

    for (int y = 0; y < FIELD_HEIGHT; ++y) {
        Put(console, FIELD_LEFT, FIELD_TOP + y, L'#', COLOR_WALL);
        Put(console, FIELD_LEFT + FIELD_WIDTH - 1, FIELD_TOP + y, L'#', COLOR_WALL);
    }
}

void DrawGame(Console& console, const Game& game) {
    // DrawGame converts grid-space objects into console-space cells. The game
    // state does not know about FIELD_LEFT/FIELD_TOP; only rendering performs
    // that translation.
    Clear(console);

    WriteText(console, 4, 1, L"Snake", COLOR_TITLE);
    WriteText(console, 17, 1, L"Score: " + std::to_wstring(game.score), COLOR_NORMAL);
    WriteText(console, 4, 2, L"Move: arrows or WASD   Restart: R   Quit: Esc", COLOR_DIM);

    DrawBorder(console);

    if (game.food.x >= 0) {
        Put(console, FIELD_LEFT + game.food.x, FIELD_TOP + game.food.y, L'@', COLOR_FOOD);
    }

    for (size_t i = 0; i < game.snake.size(); ++i) {
        const Vec2 part = game.snake[i];
        const bool isHead = i == 0;
        Put(console,
            FIELD_LEFT + part.x,
            FIELD_TOP + part.y,
            isHead ? L'O' : L'o',
            isHead ? COLOR_SNAKE_HEAD : COLOR_SNAKE_BODY);
    }

    if (game.mode == Mode::GameOver) {
        WriteText(console, FIELD_LEFT + 12, FIELD_TOP + 8, L"GAME OVER", COLOR_GAME_OVER);
        WriteText(console, FIELD_LEFT + 7, FIELD_TOP + 10, L"Press R to restart or Esc to quit.", COLOR_NORMAL);
    }

    Flush(console);
}

}  // namespace

int snake_game::SnakeGame::Run() {
    // Get the process-standard console handles. The output handle is required
    // for WriteConsoleOutputW; the input handle is adjusted so the console does
    // not echo typed characters or wait for line-buffered input.
    Console console{};
    if (!console.Initialize(SCREEN_WIDTH, SCREEN_HEIGHT, true)) {
        return 1;
    }

    std::random_device randomDevice;
    std::mt19937 rng(randomDevice());

    Game game{};
    Reset(game, rng);

    bool wasRestartDown = false;
    while (game.running) {
        // The loop is fixed-timestep and intentionally simple:
        // 1. Poll keys without blocking.
        // 2. Restart on a fresh R press if requested.
        // 3. Advance the snake one grid cell.
        // 4. Draw the entire frame.
        // 5. Sleep until the next step.
        HandleInput(console, game);

        const bool restartDown = Pressed('R');
        if (restartDown && !wasRestartDown) {
            Reset(game, rng);
        }
        wasRestartDown = restartDown;

        Step(game, rng);
        DrawGame(console, game);
        std::this_thread::sleep_for(std::chrono::milliseconds(FRAME_DELAY_MS));
    }

    // Restore console state before exiting. The clear/flush pair removes the
    // last game frame, and the input mode/cursor settings are returned to the
    // values the user had before launching the game.
    console.Cleanup();
    return 0;
}
