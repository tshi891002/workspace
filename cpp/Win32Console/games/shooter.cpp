#include <windows.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <random>
#include <string>
#include <thread>
#include <vector>

#include "games/shooter.h"
#include "common/console_screen.h"

namespace {

// This game uses the Win32 console as a small fixed-size display surface. The
// program does not stream text with std::cout. Instead, it draws each frame into
// a CHAR_INFO array and copies that complete frame to the console with
// WriteConsoleOutputW.
constexpr int SCREEN_WIDTH = 80;
constexpr int SCREEN_HEIGHT = 30;

// The playfield leaves room at the top for status text and a border around the
// action. Player, bullet, and enemy positions use playfield grid coordinates.
constexpr int FIELD_LEFT = 4;
constexpr int FIELD_TOP = 4;
constexpr int FIELD_WIDTH = 58;
constexpr int FIELD_HEIGHT = 22;

// A fixed frame delay keeps the game deterministic enough for a console game.
// Movement speeds are expressed as "frames between steps", so different object
// types can move at different rates while sharing one simple loop.
constexpr int FRAME_DELAY_MS = 45;
constexpr int BULLET_STEP_FRAMES = 1;
constexpr int ENEMY_STEP_FRAMES = 5;
constexpr int ENEMY_SPAWN_FRAMES = 18;
constexpr int STARTING_LIVES = 3;

// Win32 console colors are WORD bit masks. Foreground bits color the character;
// background bits color the cell behind it; intensity brightens the result.
constexpr WORD COLOR_NORMAL = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
constexpr WORD COLOR_DIM = FOREGROUND_INTENSITY;
constexpr WORD COLOR_TITLE = FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
constexpr WORD COLOR_WALL = FOREGROUND_BLUE | FOREGROUND_INTENSITY;
constexpr WORD COLOR_PLAYER = FOREGROUND_GREEN | FOREGROUND_INTENSITY;
constexpr WORD COLOR_BULLET = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
constexpr WORD COLOR_ENEMY = FOREGROUND_RED | FOREGROUND_INTENSITY;
constexpr WORD COLOR_HIT = BACKGROUND_RED | FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
constexpr WORD COLOR_GAME_OVER = FOREGROUND_RED | FOREGROUND_INTENSITY;

struct Vec2 {
    int x = 0;
    int y = 0;
};

struct Bullet {
    Vec2 position;
};

struct Enemy {
    Vec2 position;
};

enum class Mode {
    Playing,
    GameOver
};

using Console = console_ui::ConsoleScreen;

struct Game {
    Vec2 player;
    std::vector<Bullet> bullets;
    std::vector<Enemy> enemies;
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

void Put(Console& console, int x, int y, wchar_t ch, WORD color = COLOR_NORMAL) {
    // CHAR_INFO is the Win32 console cell type: it stores one Unicode character
    // and one color attribute. All drawing funnels through this helper so the
    // rest of the code can think in x/y coordinates.
    console.Put(x, y, ch, color);
}

void WriteText(Console& console, int x, int y, const std::wstring& text, WORD color = COLOR_NORMAL) {
    // UI labels are still drawn into the frame buffer one cell at a time. This
    // keeps text rendering consistent with ships, bullets, enemies, and borders.
    console.WriteText(x, y, text, color);
}

void Clear(Console& console) {
    // Clear only modifies the off-screen buffer. The user sees the new blank
    // frame only after Flush calls WriteConsoleOutputW.
    console.Clear();
}

void Flush(Console& console) {
    // WriteConsoleOutputW copies a rectangular CHAR_INFO buffer to the real
    // console screen buffer. A single full-frame write avoids visible flicker
    // from many small console writes.
    console.Flush();
}

bool Pressed(int virtualKey) {
    // GetAsyncKeyState is used for real-time controls. It checks the current
    // physical key state without blocking the frame loop.
    return (GetAsyncKeyState(virtualKey) & 0x8000) != 0;
}

void DrainConsoleInput(HANDLE input) {
    // Even though movement is read with GetAsyncKeyState, the console still
    // queues input records. Draining them prevents arrow-key events and window
    // events from piling up while the game is running.
    INPUT_RECORD record{};
    DWORD recordsRead = 0;
    while (PeekConsoleInputW(input, &record, 1, &recordsRead) && recordsRead > 0) {
        ReadConsoleInputW(input, &record, 1, &recordsRead);
    }
}

void Reset(Game& game) {
    // Reset all gameplay state through one function so first launch and restart
    // behave exactly the same way.
    game.player = { FIELD_WIDTH / 2, FIELD_HEIGHT - 3 };
    game.bullets.clear();
    game.enemies.clear();
    game.mode = Mode::Playing;
    game.score = 0;
    game.lives = STARTING_LIVES;
    game.frame = 0;
    game.running = true;
    game.flashHit = false;
}

void TryShoot(Game& game) {
    // Bullets spawn one row above the ship. The small bullet cap keeps the
    // screen readable and prevents holding Space from filling the buffer with a
    // dense wall of shots.
    if (game.bullets.size() >= 6 || game.mode != Mode::Playing) {
        return;
    }

    game.bullets.push_back({ { game.player.x, game.player.y - 1 } });
}

void HandleInput(Console& console, Game& game, bool& wasShootDown, bool& wasRestartDown) {
    // Input is intentionally nonblocking. Each frame polls the keys that matter
    // and returns immediately, allowing enemies and bullets to keep moving even
    // when the player is not pressing anything.
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

    if ((Pressed(VK_LEFT) || Pressed('A')) && game.player.x > 1) {
        --game.player.x;
    }
    if ((Pressed(VK_RIGHT) || Pressed('D')) && game.player.x < FIELD_WIDTH - 2) {
        ++game.player.x;
    }
    if ((Pressed(VK_UP) || Pressed('W')) && game.player.y > FIELD_HEIGHT / 2) {
        --game.player.y;
    }
    if ((Pressed(VK_DOWN) || Pressed('S')) && game.player.y < FIELD_HEIGHT - 3) {
        ++game.player.y;
    }

    const bool shootDown = Pressed(VK_SPACE);
    if (shootDown && !wasShootDown) {
        TryShoot(game);
    }
    wasShootDown = shootDown;
}

void SpawnEnemy(Game& game, std::mt19937& rng) {
    // Enemies enter from the top interior row. The random x coordinate is kept
    // away from the border so the enemy always appears inside the playfield.
    std::uniform_int_distribution<int> xDist(1, FIELD_WIDTH - 2);
    game.enemies.push_back({ { xDist(rng), 1 } });
}

void MoveBullets(Game& game) {
    // Bullets move upward and disappear after they leave the playfield. Erase
    // with remove_if keeps the vector compact without manual index juggling.
    if (game.frame % BULLET_STEP_FRAMES != 0) {
        return;
    }

    for (Bullet& bullet : game.bullets) {
        --bullet.position.y;
    }

    game.bullets.erase(
        std::remove_if(game.bullets.begin(), game.bullets.end(),
            [](const Bullet& bullet) { return bullet.position.y <= 0; }),
        game.bullets.end());
}

void MoveEnemies(Game& game) {
    // Enemies advance downward more slowly than bullets. If they reach the
    // bottom, the player loses a life and the enemy is removed.
    if (game.frame % ENEMY_STEP_FRAMES != 0) {
        return;
    }

    for (Enemy& enemy : game.enemies) {
        ++enemy.position.y;
    }

    const auto oldSize = game.enemies.size();
    game.enemies.erase(
        std::remove_if(game.enemies.begin(), game.enemies.end(),
            [](const Enemy& enemy) { return enemy.position.y >= FIELD_HEIGHT - 1; }),
        game.enemies.end());

    const int escaped = static_cast<int>(oldSize - game.enemies.size());
    if (escaped > 0) {
        game.lives -= escaped;
        game.flashHit = true;
    }
}

void ResolveBulletEnemyHits(Game& game) {
    // Collision detection is grid-based: a bullet hits an enemy when they occupy
    // the same cell. A hit removes both objects and awards points.
    std::vector<bool> removeBullets(game.bullets.size(), false);
    std::vector<bool> removeEnemies(game.enemies.size(), false);

    for (size_t bulletIndex = 0; bulletIndex < game.bullets.size(); ++bulletIndex) {
        for (size_t enemyIndex = 0; enemyIndex < game.enemies.size(); ++enemyIndex) {
            if (!removeEnemies[enemyIndex] && game.bullets[bulletIndex].position == game.enemies[enemyIndex].position) {
                removeBullets[bulletIndex] = true;
                removeEnemies[enemyIndex] = true;
                game.score += 10;
                break;
            }
        }
    }

    for (size_t i = game.bullets.size(); i > 0; --i) {
        if (removeBullets[i - 1]) {
            game.bullets.erase(game.bullets.begin() + static_cast<std::ptrdiff_t>(i - 1));
        }
    }

    for (size_t i = game.enemies.size(); i > 0; --i) {
        if (removeEnemies[i - 1]) {
            game.enemies.erase(game.enemies.begin() + static_cast<std::ptrdiff_t>(i - 1));
        }
    }
}

void ResolvePlayerHits(Game& game) {
    // If an enemy reaches the player ship, the player loses a life and that
    // enemy disappears. This keeps one collision from draining all lives across
    // multiple frames.
    const auto oldSize = game.enemies.size();
    game.enemies.erase(
        std::remove_if(game.enemies.begin(), game.enemies.end(),
            [&game](const Enemy& enemy) { return enemy.position == game.player; }),
        game.enemies.end());

    const int hits = static_cast<int>(oldSize - game.enemies.size());
    if (hits > 0) {
        game.lives -= hits;
        game.flashHit = true;
    }
}

void Step(Game& game, std::mt19937& rng) {
    // Step contains the game rules and deliberately has no console drawing code.
    // That separation makes the simulation easier to reason about and keeps the
    // Win32 API details isolated in rendering/input helpers.
    if (game.mode == Mode::GameOver) {
        return;
    }

    ++game.frame;
    game.flashHit = false;

    if (game.frame % ENEMY_SPAWN_FRAMES == 0) {
        SpawnEnemy(game, rng);
    }

    MoveBullets(game);
    MoveEnemies(game);
    ResolveBulletEnemyHits(game);
    ResolvePlayerHits(game);

    if (game.lives <= 0) {
        game.lives = 0;
        game.mode = Mode::GameOver;
    }
}

void DrawBorder(Console& console) {
    // The visible border is drawn in console coordinates by offsetting the
    // playfield grid with FIELD_LEFT and FIELD_TOP.
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
    // DrawGame translates the pure game state into console cells. The gameplay
    // model knows nothing about colors or screen offsets; only this layer knows
    // how to turn positions into visible Win32 console output.
    Clear(console);

    WriteText(console, 4, 1, L"Flying Shooter", COLOR_TITLE);
    WriteText(console, 24, 1, L"Score: " + std::to_wstring(game.score), COLOR_NORMAL);
    WriteText(console, 39, 1, L"Lives: " + std::to_wstring(game.lives), game.flashHit ? COLOR_HIT : COLOR_NORMAL);
    WriteText(console, 4, 2, L"Move: arrows/WASD   Fire: Space   Restart: R   Quit: Esc", COLOR_DIM);

    DrawBorder(console);

    for (const Bullet& bullet : game.bullets) {
        Put(console, FIELD_LEFT + bullet.position.x, FIELD_TOP + bullet.position.y, L'|', COLOR_BULLET);
    }

    for (const Enemy& enemy : game.enemies) {
        Put(console, FIELD_LEFT + enemy.position.x, FIELD_TOP + enemy.position.y, L'V', COLOR_ENEMY);
    }

    Put(console, FIELD_LEFT + game.player.x, FIELD_TOP + game.player.y, L'^', game.flashHit ? COLOR_HIT : COLOR_PLAYER);
    Put(console, FIELD_LEFT + game.player.x - 1, FIELD_TOP + game.player.y + 1, L'/', COLOR_PLAYER);
    Put(console, FIELD_LEFT + game.player.x + 1, FIELD_TOP + game.player.y + 1, L'\\', COLOR_PLAYER);

    if (game.mode == Mode::GameOver) {
        WriteText(console, FIELD_LEFT + 19, FIELD_TOP + 9, L"GAME OVER", COLOR_GAME_OVER);
        WriteText(console, FIELD_LEFT + 13, FIELD_TOP + 11, L"Press R to restart or Esc to quit.", COLOR_NORMAL);
    }

    Flush(console);
}

}  // namespace

int shooter_game::ShooterGame::Run() {
    // Get the standard console handles. The output handle is used for direct
    // screen-buffer writes, and the input handle is configured so the game can
    // own keyboard behavior while it is running.
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
        // The main loop follows the common arcade-game shape:
        // 1. Poll input without blocking.
        // 2. Update the simulation by one frame.
        // 3. Render the whole scene into the console buffer.
        // 4. Sleep briefly to control speed.
        HandleInput(console, game, wasShootDown, wasRestartDown);
        Step(game, rng);
        DrawGame(console, game);
        std::this_thread::sleep_for(std::chrono::milliseconds(FRAME_DELAY_MS));
    }

    // Leave the console clean: clear the last frame, restore the text cursor,
    // and put the input mode back to its original value.
    console.Cleanup();
    return 0;
}
