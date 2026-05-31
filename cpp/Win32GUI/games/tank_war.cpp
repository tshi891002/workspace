#include "games.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <deque>
#include <random>
#include <string>
#include <vector>

// Tank War is another continuous simulation. Shots carry two-dimensional
// velocity vectors because they may travel diagonally. The enemy tank follows
// the player and periodically aims a normalized shot in the player's direction.
class TankWarGame final : public Game {
    struct Shot { float x; float y; float vx; float vy; bool enemy; };
    struct Tank { float x; float y; int hp; };
    Tank player{ 140, 500, 5 };
    Tank enemy{ 740, 150, 5 };
    std::vector<Shot> shots;
    bool up = false, down = false, left = false, right = false;
    int cooldown = 0;

    static bool HitTank(const Shot& s, const Tank& t) {
        return s.x > t.x - 26 && s.x < t.x + 26 && s.y > t.y - 22 && s.y < t.y + 22;
    }

public:
    void Reset() override {
        player = { 140, 500, 5 };
        enemy = { 740, 150, 5 };
        shots.clear();
        up = down = left = right = false;
        cooldown = 0;
    }

    void Update() override {
        if (player.hp <= 0 || enemy.hp <= 0) return;
        if (up) player.y -= 4.0f;
        if (down) player.y += 4.0f;
        if (left) player.x -= 4.0f;
        if (right) player.x += 4.0f;
        player.x = std::clamp(player.x, 70.0f, 830.0f);
        player.y = std::clamp(player.y, 125.0f, 555.0f);
        enemy.x += (player.x > enemy.x ? 1.6f : -1.6f);
        enemy.y += (player.y > enemy.y ? 1.2f : -1.2f);
        if (cooldown > 0) --cooldown;
        if (cooldown == 0) {
            // Normalize the direction vector so every enemy projectile travels
            // at the same speed regardless of target distance.
            float dx = player.x - enemy.x;
            float dy = player.y - enemy.y;
            float len = std::max(1.0f, std::sqrt(dx * dx + dy * dy));
            shots.push_back({ enemy.x, enemy.y, dx / len * 5.5f, dy / len * 5.5f, true });
            cooldown = 75;
        }
        for (auto& s : shots) {
            s.x += s.vx;
            s.y += s.vy;
            if (!s.enemy && HitTank(s, enemy)) { --enemy.hp; s.x = -999.0f; }
            if (s.enemy && HitTank(s, player)) { --player.hp; s.x = -999.0f; }
        }
        shots.erase(std::remove_if(shots.begin(), shots.end(), [](const Shot& s) {
            return s.x < 40 || s.x > 860 || s.y < 96 || s.y > 585;
        }), shots.end());
    }

    void DrawTank(HDC dc, const Tank& t, COLORREF body) {
        Fill(dc, Rect{ static_cast<int>(t.x - 28), static_cast<int>(t.y - 20), 56, 40 }, body);
        Fill(dc, Rect{ static_cast<int>(t.x - 8), static_cast<int>(t.y - 34), 16, 34 }, body);
        Frame(dc, Rect{ static_cast<int>(t.x - 28), static_cast<int>(t.y - 20), 56, 40 }, RGB(16, 20, 24), 2);
    }

    void Draw(HDC dc) override {
        Header(dc, L"Tank War", L"WASD moves your tank. Space fires upward. Avoid enemy shots. R restarts, Esc returns.");
        Fill(dc, Rect{ 40, 100, 820, 480 }, RGB(28, 34, 28));
        Frame(dc, Rect{ 40, 100, 820, 480 }, RGB(91, 104, 82), 2);
        Fill(dc, Rect{ 250, 220, 120, 34 }, RGB(76, 83, 72));
        Fill(dc, Rect{ 520, 390, 150, 36 }, RGB(76, 83, 72));
        DrawTank(dc, player, RGB(82, 166, 223));
        DrawTank(dc, enemy, RGB(219, 98, 82));
        for (const auto& s : shots) EllipseFill(dc, Rect{ static_cast<int>(s.x - 5), static_cast<int>(s.y - 5), 10, 10 }, s.enemy ? RGB(244, 105, 87) : RGB(118, 225, 157));
        Text(dc, 56, 112, L"Player HP " + std::to_wstring(std::max(0, player.hp)), 18, RGB(220, 235, 245), FW_BOLD);
        Text(dc, 710, 112, L"Enemy HP " + std::to_wstring(std::max(0, enemy.hp)), 18, RGB(244, 214, 210), FW_BOLD);
        if (player.hp <= 0 || enemy.hp <= 0) CenterText(dc, Rect{ 280, 286, 340, 58 }, enemy.hp <= 0 ? L"You Win" : L"Tank Destroyed", 34, RGB(239, 219, 126), FW_BOLD);
    }

    void KeyDown(WPARAM key) override {
        Game::KeyDown(key);
        if (key == 'W') up = true;
        if (key == 'S') down = true;
        if (key == 'A') left = true;
        if (key == 'D') right = true;
        if (key == VK_SPACE) shots.push_back({ player.x, player.y - 28, 0.0f, -8.0f, false });
    }

    void KeyUp(WPARAM key) override {
        if (key == 'W') up = false;
        if (key == 'S') down = false;
        if (key == 'A') left = false;
        if (key == 'D') right = false;
    }
};

std::unique_ptr<Game> CreateTankWarGame() {
    return std::make_unique<TankWarGame>();
}

