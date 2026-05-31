#include "games.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <deque>
#include <random>
#include <string>
#include <vector>

// Shooter demonstrates a lightweight entity-list design. Bullets and enemies
// are plain structs stored in vectors. Update moves entities, resolves simple
// axis-aligned collisions, and erases bullets after they leave the arena.
class ShooterGame final : public Game {
    struct Bullet { float x; float y; float vy; bool enemy; };
    struct Enemy { float x; float y; bool alive; };
    std::vector<Bullet> bullets;
    std::vector<Enemy> enemies;
    float playerX = 450.0f;
    int tick = 0;
    int score = 0;
    bool left = false, right = false, over = false;

    void Fire() {
        bullets.push_back({ playerX, 520.0f, -9.0f, false });
    }

public:
    void Reset() override {
        bullets.clear();
        enemies.clear();
        for (int r = 0; r < 4; ++r) for (int c = 0; c < 9; ++c) enemies.push_back({ 150.0f + c * 70.0f, 128.0f + r * 42.0f, true });
        playerX = 450.0f;
        tick = score = 0;
        left = right = over = false;
    }

    ShooterGame() { Reset(); }

    void Update() override {
        if (over) return;
        if (left) playerX -= 7.0f;
        if (right) playerX += 7.0f;
        playerX = std::clamp(playerX, 60.0f, 840.0f);
        ++tick;
        // Erase-remove is the standard C++ pattern for dropping expired vector
        // elements while retaining the remaining bullets.
        for (auto& b : bullets) b.y += b.vy;
        bullets.erase(std::remove_if(bullets.begin(), bullets.end(), [](const Bullet& b) { return b.y < 92 || b.y > 590; }), bullets.end());
        for (auto& e : enemies) {
            if (!e.alive) continue;
            e.y += 0.055f;
            // Every surviving enemy gets an occasional chance to fire. Marking
            // ownership on each bullet lets one vector hold both directions.
            if (tick % 80 == 0 && std::uniform_int_distribution<int>(0, 22)(Rng()) == 0) bullets.push_back({ e.x, e.y + 18.0f, 5.0f, true });
            for (auto& b : bullets) {
                if (!b.enemy && std::abs(b.x - e.x) < 24.0f && std::abs(b.y - e.y) < 18.0f) {
                    e.alive = false;
                    b.y = -100.0f;
                    score += 20;
                }
            }
            if (e.y > 500.0f) over = true;
        }
        for (const auto& b : bullets) {
            if (b.enemy && std::abs(b.x - playerX) < 28.0f && b.y > 505.0f && b.y < 552.0f) over = true;
        }
        if (std::none_of(enemies.begin(), enemies.end(), [](const Enemy& e) { return e.alive; })) over = true;
    }

    void Draw(HDC dc) override {
        Header(dc, L"Shooter", L"Left/Right moves, Space fires. Clear the formation before it reaches you. R restarts, Esc returns.");
        Fill(dc, Rect{ 40, 100, 820, 480 }, RGB(18, 22, 32));
        Frame(dc, Rect{ 40, 100, 820, 480 }, RGB(78, 88, 106), 2);
        for (const auto& e : enemies) if (e.alive) {
            Fill(dc, Rect{ static_cast<int>(e.x - 22), static_cast<int>(e.y - 14), 44, 28 }, RGB(218, 101, 96));
            Fill(dc, Rect{ static_cast<int>(e.x - 12), static_cast<int>(e.y - 24), 24, 10 }, RGB(246, 174, 101));
        }
        for (const auto& b : bullets) Fill(dc, Rect{ static_cast<int>(b.x - 2), static_cast<int>(b.y - 8), 4, 16 }, b.enemy ? RGB(238, 88, 88) : RGB(108, 222, 158));
        Fill(dc, Rect{ static_cast<int>(playerX - 30), 530, 60, 18 }, RGB(95, 178, 234));
        Fill(dc, Rect{ static_cast<int>(playerX - 10), 510, 20, 22 }, RGB(95, 178, 234));
        Text(dc, 724, 112, L"Score " + std::to_wstring(score), 20, RGB(226, 232, 240), FW_BOLD);
        if (over) CenterText(dc, Rect{ 260, 286, 380, 60 }, L"Round Complete", 34, RGB(236, 218, 126), FW_BOLD);
    }

    void KeyDown(WPARAM key) override {
        Game::KeyDown(key);
        if (key == VK_LEFT) left = true;
        if (key == VK_RIGHT) right = true;
        if (key == VK_SPACE && tick % 8 != 1) Fire();
    }

    void KeyUp(WPARAM key) override {
        if (key == VK_LEFT) left = false;
        if (key == VK_RIGHT) right = false;
    }
};

std::unique_ptr<Game> CreateShooterGame() {
    return std::make_unique<ShooterGame>();
}

