#include <windows.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include "games/tankwar.h"
#include "common/console_screen.h"

namespace {

// Tank War uses the console as a fixed-size grid display. The program renders
// to an off-screen CHAR_INFO buffer first, then presents the whole frame with
// WriteConsoleOutputW. This gives the console a game-like "screen refresh"
// instead of a scrolling text-output feel.
constexpr int SCREEN_WIDTH = 90;
constexpr int SCREEN_HEIGHT = 32;

// The battlefield is offset from the top-left corner so status text can live
// above it. All gameplay coordinates are local to this battlefield rectangle.
constexpr int FIELD_LEFT = 3;
constexpr int FIELD_TOP = 4;
constexpr int FIELD_WIDTH = 64;
constexpr int FIELD_HEIGHT = 24;

// The frame delay controls the main loop cadence. Individual objects can move
// less often by checking the frame counter, which keeps the design simple while
// allowing bullets and enemy tanks to feel different.
constexpr int FRAME_DELAY_MS = 55;
constexpr int PLAYER_BULLET_STEP_FRAMES = 1;
constexpr int ENEMY_BULLET_STEP_FRAMES = 2;
constexpr int ENEMY_MOVE_FRAMES = 9;
constexpr int ENEMY_FIRE_FRAMES = 22;
constexpr int STARTING_LIVES = 3;

// Win32 console colors are bit masks. Foreground bits affect the character,
// background bits affect the cell behind it, and intensity brightens the color.
constexpr WORD COLOR_NORMAL = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
constexpr WORD COLOR_DIM = FOREGROUND_INTENSITY;
constexpr WORD COLOR_TITLE = FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
constexpr WORD COLOR_WALL = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
constexpr WORD COLOR_PLAYER = FOREGROUND_GREEN | FOREGROUND_INTENSITY;
constexpr WORD COLOR_ENEMY = FOREGROUND_RED | FOREGROUND_INTENSITY;
constexpr WORD COLOR_BULLET = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
constexpr WORD COLOR_ENEMY_BULLET = FOREGROUND_RED | FOREGROUND_INTENSITY;
constexpr WORD COLOR_HIT = BACKGROUND_RED | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
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

struct Tank {
    Vec2 position;
    Direction direction = Direction::Up;
    bool alive = true;
};

struct Bullet {
    Vec2 position;
    Direction direction = Direction::Up;
    bool fromPlayer = true;
};

using Console = console_ui::ConsoleScreen;

struct Game {
    Tank player;
    std::vector<Tank> enemies;
    std::vector<Bullet> bullets;
    std::vector<Vec2> walls;
    Mode mode = Mode::Playing;
    int score = 0;
    int lives = STARTING_LIVES;
    int frame = 0;
    bool running = true;
    bool flashHit = false;
};

bool operator==(Vec2 a, Vec2 b) {
    return a.x == b.x && a.y == b.y;
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

wchar_t DirectionGlyph(Direction direction) {
    // ASCII-like glyphs are chosen so the game displays correctly on classic
    // Windows console fonts without relying on box-drawing or emoji support.
    switch (direction) {
    case Direction::Up:
        return L'^';
    case Direction::Down:
        return L'v';
    case Direction::Left:
        return L'<';
    case Direction::Right:
        return L'>';
    }

    return L'^';
}

void Put(Console& console, int x, int y, wchar_t ch, WORD color = COLOR_NORMAL) {
    // CHAR_INFO is the basic Win32 console cell: one Unicode character plus one
    // color attribute. Centralizing writes here keeps all drawing bounds-safe.
    console.Put(x, y, ch, color);
}

void WriteText(Console& console, int x, int y, const std::wstring& text, WORD color = COLOR_NORMAL) {
    // Text is drawn into the same frame buffer as tanks and bullets, so labels
    // and game objects are presented together by a single Flush call.
    console.WriteText(x, y, text, color);
}

void Clear(Console& console) {
    // Clear prepares the next frame in memory. The visible console is untouched
    // until Flush pushes this buffer to the screen.
    console.Clear();
}

void Flush(Console& console) {
    // WriteConsoleOutputW copies a rectangular CHAR_INFO buffer into the
    // console's screen buffer. One full-frame write avoids the flicker caused by
    // many small cursor-position-and-print operations.
    console.Flush();
}

bool Pressed(int virtualKey) {
    // GetAsyncKeyState reads the current physical key state without blocking.
    // This is a good fit for arcade controls because the game loop keeps moving
    // whether or not the player presses a key.
    return (GetAsyncKeyState(virtualKey) & 0x8000) != 0;
}

void DrainConsoleInput(HANDLE input) {
    // The game uses GetAsyncKeyState for controls, but the console still queues
    // input records. Draining pending records keeps the console input buffer
    // from filling with arrow-key and window events while the loop runs.
    INPUT_RECORD record{};
    DWORD recordsRead = 0;
    while (PeekConsoleInputW(input, &record, 1, &recordsRead) && recordsRead > 0) {
        ReadConsoleInputW(input, &record, 1, &recordsRead);
    }
}

bool InsideField(Vec2 point) {
    // The outer ring is a border, so playable cells are inside the border.
    return point.x > 0 && point.x < FIELD_WIDTH - 1 &&
           point.y > 0 && point.y < FIELD_HEIGHT - 1;
}

bool ContainsWall(const Game& game, Vec2 point) {
    return std::find(game.walls.begin(), game.walls.end(), point) != game.walls.end();
}

bool ContainsEnemy(const Game& game, Vec2 point) {
    for (const Tank& enemy : game.enemies) {
        if (enemy.alive && enemy.position == point) {
            return true;
        }
    }
    return false;
}

bool OccupiedByTank(const Game& game, Vec2 point) {
    return game.player.position == point || ContainsEnemy(game, point);
}

void BuildWalls(Game& game) {
    // Static walls create lanes and cover without requiring a level file. The
    // coordinates are local battlefield cells, not absolute console cells.
    game.walls.clear();

    for (int y = 5; y <= 16; ++y) {
        if (y != 10 && y != 11) {
            game.walls.push_back({ 18, y });
            game.walls.push_back({ 45, y });
        }
    }

    for (int x = 25; x <= 38; ++x) {
        if (x != 31 && x != 32) {
            game.walls.push_back({ x, 8 });
            game.walls.push_back({ x, 17 });
        }
    }
}

void Reset(Game& game) {
    // Reset centralizes all launch/restart state. This makes pressing R behave
    // exactly like starting a fresh game.
    game.player = { { FIELD_WIDTH / 2, FIELD_HEIGHT - 3 }, Direction::Up, true };
    game.enemies = {
        { { 8, 3 }, Direction::Down, true },
        { { FIELD_WIDTH / 2, 3 }, Direction::Down, true },
        { { FIELD_WIDTH - 9, 3 }, Direction::Down, true }
    };
    game.bullets.clear();
    BuildWalls(game);
    game.mode = Mode::Playing;
    game.score = 0;
    game.lives = STARTING_LIVES;
    game.frame = 0;
    game.running = true;
    game.flashHit = false;
}

bool CanMoveTo(const Game& game, Vec2 point) {
    // Tanks cannot move through the border, walls, or other tanks. Bullets use
    // different collision rules because they may hit and destroy targets.
    return InsideField(point) && !ContainsWall(game, point) && !OccupiedByTank(game, point);
}

void DamagePlayer(Game& game);

void TryMovePlayer(Game& game, Direction direction) {
    // Turning and moving happen together: pressing a direction rotates the tank
    // and tries to advance one cell. If blocked, the tank still faces that way.
    game.player.direction = direction;
    const Vec2 delta = DirectionDelta(direction);
    const Vec2 next = { game.player.position.x + delta.x, game.player.position.y + delta.y };
    if (CanMoveTo(game, next)) {
        game.player.position = next;
    }
}

void FireBullet(Game& game, const Tank& tank, bool fromPlayer) {
    // Bullets spawn one cell in front of the tank barrel. A small cap on player
    // bullets keeps the playfield readable and prevents holding Space from
    // turning the game into a solid column of shots.
    if (fromPlayer) {
        const int playerBullets = static_cast<int>(std::count_if(game.bullets.begin(), game.bullets.end(),
            [](const Bullet& bullet) { return bullet.fromPlayer; }));
        if (playerBullets >= 3) {
            return;
        }
    }

    const Vec2 delta = DirectionDelta(tank.direction);
    const Vec2 start = { tank.position.x + delta.x, tank.position.y + delta.y };
    if (!InsideField(start) || ContainsWall(game, start)) {
        return;
    }

    // Resolve point-blank shots immediately. Without this, a bullet that starts
    // inside an adjacent target would move past it on the next frame before the
    // collision check runs.
    if (fromPlayer) {
        for (Tank& enemy : game.enemies) {
            if (enemy.alive && enemy.position == start) {
                enemy.alive = false;
                game.score += 100;
                return;
            }
        }
    } else if (game.player.position == start) {
        DamagePlayer(game);
        return;
    }

    game.bullets.push_back({ start, tank.direction, fromPlayer });
}

void HandleInput(Console& console, Game& game, bool& wasShootDown, bool& wasRestartDown) {
    // Input polling is nonblocking. The loop reads currently pressed keys, then
    // immediately continues to simulation and drawing so enemies keep moving.
    DrainConsoleInput(console.input);

    if (Pressed(VK_ESCAPE)) {
        game.running = false;
        return;
    }

    const bool restartDown = Pressed('R');
    if (restartDown && !wasRestartDown) {
        Reset(game);
    }
    wasRestartDown = restartDown;

    if (game.mode != Mode::Playing) {
        return;
    }

    if (Pressed(VK_UP) || Pressed('W')) {
        TryMovePlayer(game, Direction::Up);
    } else if (Pressed(VK_DOWN) || Pressed('S')) {
        TryMovePlayer(game, Direction::Down);
    } else if (Pressed(VK_LEFT) || Pressed('A')) {
        TryMovePlayer(game, Direction::Left);
    } else if (Pressed(VK_RIGHT) || Pressed('D')) {
        TryMovePlayer(game, Direction::Right);
    }

    const bool shootDown = Pressed(VK_SPACE);
    if (shootDown && !wasShootDown) {
        FireBullet(game, game.player, true);
    }
    wasShootDown = shootDown;
}

Direction RandomDirection(std::mt19937& rng) {
    std::uniform_int_distribution<int> directionDist(0, 3);
    return static_cast<Direction>(directionDist(rng));
}

void MoveEnemies(Game& game, std::mt19937& rng) {
    // Enemies move less often than the player. When blocked, they choose a new
    // random direction, giving a simple patrol behavior without pathfinding.
    if (game.frame % ENEMY_MOVE_FRAMES != 0) {
        return;
    }

    for (Tank& enemy : game.enemies) {
        if (!enemy.alive) {
            continue;
        }

        if (game.frame % (ENEMY_MOVE_FRAMES * 3) == 0) {
            enemy.direction = RandomDirection(rng);
        }

        const Vec2 delta = DirectionDelta(enemy.direction);
        const Vec2 next = { enemy.position.x + delta.x, enemy.position.y + delta.y };
        if (CanMoveTo(game, next)) {
            enemy.position = next;
        } else {
            enemy.direction = RandomDirection(rng);
        }
    }
}

void EnemyFire(Game& game, std::mt19937& rng) {
    // Enemy firing is probabilistic, checked on a slower cadence. This keeps the
    // battlefield active without making every enemy shoot every time.
    if (game.frame % ENEMY_FIRE_FRAMES != 0) {
        return;
    }

    std::uniform_int_distribution<int> chanceDist(0, 99);
    for (const Tank& enemy : game.enemies) {
        if (enemy.alive && chanceDist(rng) < 55) {
            FireBullet(game, enemy, false);
        }
    }
}

void RespawnPlayer(Game& game) {
    // After being hit, the player returns to the bottom center if possible. If
    // that spot is occupied, the game still places the player there to avoid a
    // confusing invisible/dead state.
    game.player.position = { FIELD_WIDTH / 2, FIELD_HEIGHT - 3 };
    game.player.direction = Direction::Up;
    game.flashHit = true;
}

void DamagePlayer(Game& game) {
    --game.lives;
    if (game.lives <= 0) {
        game.lives = 0;
        game.mode = Mode::GameOver;
    } else {
        RespawnPlayer(game);
    }
}

void ResolveBulletCollision(Game& game, Bullet& bullet, unsigned char& removeBullet) {
    // Bullet collision order is intentional: walls/borders stop shots, player
    // bullets destroy enemies, and enemy bullets damage the player.
    if (!InsideField(bullet.position) || ContainsWall(game, bullet.position)) {
        removeBullet = 1;
        return;
    }

    if (bullet.fromPlayer) {
        for (Tank& enemy : game.enemies) {
            if (enemy.alive && enemy.position == bullet.position) {
                enemy.alive = false;
                game.score += 100;
                removeBullet = 1;
                return;
            }
        }
    } else if (game.player.position == bullet.position) {
        removeBullet = 1;
        DamagePlayer(game);
    }
}

void MoveBullets(Game& game) {
    // Bullets travel in their facing direction. Player bullets move every frame;
    // enemy bullets move slightly slower to make dodging possible.
    // Do not use std::vector<bool> here: the standard library specializes it to
    // return proxy bit references, which cannot bind to a normal bool& on MSVC.
    std::vector<unsigned char> removeBullets(game.bullets.size(), 0);

    for (size_t i = 0; i < game.bullets.size(); ++i) {
        Bullet& bullet = game.bullets[i];
        const int stepRate = bullet.fromPlayer ? PLAYER_BULLET_STEP_FRAMES : ENEMY_BULLET_STEP_FRAMES;
        if (game.frame % stepRate != 0) {
            continue;
        }

        const Vec2 delta = DirectionDelta(bullet.direction);
        bullet.position = { bullet.position.x + delta.x, bullet.position.y + delta.y };
        ResolveBulletCollision(game, bullet, removeBullets[i]);
    }

    for (size_t i = game.bullets.size(); i > 0; --i) {
        if (removeBullets[i - 1] != 0) {
            game.bullets.erase(game.bullets.begin() + static_cast<std::ptrdiff_t>(i - 1));
        }
    }
}

void RemoveDestroyedEnemies(Game& game) {
    game.enemies.erase(
        std::remove_if(game.enemies.begin(), game.enemies.end(),
            [](const Tank& enemy) { return !enemy.alive; }),
        game.enemies.end());
}

void RefillEnemies(Game& game, std::mt19937& rng) {
    // New enemies appear near the top whenever the battlefield gets too quiet.
    // Spawning only in unoccupied cells prevents immediate overlap.
    if (game.enemies.size() >= 3 || game.frame % 60 != 0) {
        return;
    }

    std::uniform_int_distribution<int> xDist(3, FIELD_WIDTH - 4);
    for (int attempts = 0; attempts < 12 && game.enemies.size() < 3; ++attempts) {
        Vec2 spawn = { xDist(rng), 2 };
        if (!ContainsWall(game, spawn) && !OccupiedByTank(game, spawn)) {
            game.enemies.push_back({ spawn, Direction::Down, true });
        }
    }
}

void Step(Game& game, std::mt19937& rng) {
    // Step is the simulation layer. It advances the rules without knowing about
    // console colors, screen offsets, or Win32 drawing details.
    if (game.mode == Mode::GameOver) {
        return;
    }

    ++game.frame;
    game.flashHit = false;

    MoveEnemies(game, rng);
    EnemyFire(game, rng);
    MoveBullets(game);
    RemoveDestroyedEnemies(game);
    RefillEnemies(game, rng);
}

void DrawBorder(Console& console) {
    // The border is drawn in absolute console coordinates by adding FIELD_LEFT
    // and FIELD_TOP to each local battlefield coordinate.
    for (int x = 0; x < FIELD_WIDTH; ++x) {
        Put(console, FIELD_LEFT + x, FIELD_TOP, L'#', COLOR_WALL);
        Put(console, FIELD_LEFT + x, FIELD_TOP + FIELD_HEIGHT - 1, L'#', COLOR_WALL);
    }

    for (int y = 0; y < FIELD_HEIGHT; ++y) {
        Put(console, FIELD_LEFT, FIELD_TOP + y, L'#', COLOR_WALL);
        Put(console, FIELD_LEFT + FIELD_WIDTH - 1, FIELD_TOP + y, L'#', COLOR_WALL);
    }
}

void DrawTank(Console& console, const Tank& tank, WORD color) {
    // Tanks are one-cell gameplay objects represented by a directional glyph.
    // Keeping them one cell wide makes collision logic simple and readable.
    Put(console,
        FIELD_LEFT + tank.position.x,
        FIELD_TOP + tank.position.y,
        DirectionGlyph(tank.direction),
        color);
}

void DrawGame(Console& console, const Game& game) {
    // DrawGame translates game state into visible console cells. The state uses
    // local battlefield positions; rendering is the only place that knows the
    // battlefield's absolute location inside the console window.
    Clear(console);

    WriteText(console, 3, 1, L"Tank War", COLOR_TITLE);
    WriteText(console, 17, 1, L"Score: " + std::to_wstring(game.score), COLOR_NORMAL);
    WriteText(console, 34, 1, L"Lives: " + std::to_wstring(game.lives), game.flashHit ? COLOR_HIT : COLOR_NORMAL);
    WriteText(console, 3, 2, L"Move/turn: arrows or WASD   Fire: Space   Restart: R   Quit: Esc", COLOR_DIM);

    DrawBorder(console);

    for (const Vec2& wall : game.walls) {
        Put(console, FIELD_LEFT + wall.x, FIELD_TOP + wall.y, L'%', COLOR_WALL);
    }

    for (const Bullet& bullet : game.bullets) {
        Put(console,
            FIELD_LEFT + bullet.position.x,
            FIELD_TOP + bullet.position.y,
            bullet.direction == Direction::Left || bullet.direction == Direction::Right ? L'-' : L'|',
            bullet.fromPlayer ? COLOR_BULLET : COLOR_ENEMY_BULLET);
    }

    for (const Tank& enemy : game.enemies) {
        DrawTank(console, enemy, COLOR_ENEMY);
    }

    DrawTank(console, game.player, game.flashHit ? COLOR_HIT : COLOR_PLAYER);

    if (game.mode == Mode::GameOver) {
        WriteText(console, FIELD_LEFT + 27, FIELD_TOP + 10, L"GAME OVER", COLOR_GAME_OVER);
        WriteText(console, FIELD_LEFT + 20, FIELD_TOP + 12, L"Press R to restart or Esc to quit.", COLOR_NORMAL);
    }

    Flush(console);
}

}  // namespace

int tankwar_game::TankWarGame::Run() {
    // Get standard Win32 console handles. The output handle is used for
    // WriteConsoleOutputW; the input handle is configured so gameplay owns
    // keyboard behavior while the program runs.
    Console console{};
    if (!console.Initialize(SCREEN_WIDTH, SCREEN_HEIGHT, true)) {
        return 1;
    }

    std::random_device randomDevice;
    std::mt19937 rng(randomDevice());

    Game game{};
    Reset(game);

    bool wasShootDown = false;
    bool wasRestartDown = false;
    while (game.running) {
        // Main loop structure:
        // 1. Poll keyboard state without blocking.
        // 2. Advance enemies, bullets, collisions, and respawns.
        // 3. Render a full frame through the Win32 console buffer.
        // 4. Sleep briefly to set the game speed.
        HandleInput(console, game, wasShootDown, wasRestartDown);
        Step(game, rng);
        DrawGame(console, game);
        std::this_thread::sleep_for(std::chrono::milliseconds(FRAME_DELAY_MS));
    }

    // Restore the terminal before returning control to the shell.
    console.Cleanup();
    return 0;
}
