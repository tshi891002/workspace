#include "games.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <deque>
#include <random>
#include <string>
#include <vector>

// Ping Pong is a continuous pixel-coordinate simulation. KeyDown and KeyUp
// maintain held-key flags because paddle movement should continue between key
// messages. Update applies those flags on every WM_TIMER tick.
class PingPongGame final : public Game {
    float ballX = 450.0f;
    float ballY = 310.0f;
    float vx = 5.0f;
    float vy = 3.5f;
    float leftY = 260.0f;
    float rightY = 260.0f;
    int leftScore = 0;
    int rightScore = 0;
    bool up = false;
    bool down = false;

public:
    void Reset() override {
        ballX = 450.0f;
        ballY = 310.0f;
        vx = 5.0f;
        vy = 3.5f;
        leftY = rightY = 260.0f;
        leftScore = rightScore = 0;
    }

    void Update() override {
        // Clamp keeps paddles inside the arena. The computer paddle follows the
        // ball with a capped speed, making it responsive but still beatable.
        if (up) leftY -= 7.0f;
        if (down) leftY += 7.0f;
        leftY = std::clamp(leftY, 112.0f, 496.0f);
        const float target = ballY - 48.0f;
        rightY += std::clamp(target - rightY, -5.0f, 5.0f);
        rightY = std::clamp(rightY, 112.0f, 496.0f);

        ballX += vx;
        ballY += vy;
        // Paddle collisions reverse horizontal velocity and adjust vertical
        // velocity based on where the ball struck the paddle.
        if (ballY < 105.0f || ballY > 596.0f) {
            vy = -vy;
        }
        if (ballX < 62.0f && ballX > 42.0f && ballY + 12.0f >= leftY && ballY <= leftY + 96.0f) {
            vx = std::abs(vx) + 0.25f;
            vy += (ballY - (leftY + 48.0f)) * 0.035f;
        }
        if (ballX > 824.0f && ballX < 846.0f && ballY + 12.0f >= rightY && ballY <= rightY + 96.0f) {
            vx = -std::abs(vx) - 0.25f;
            vy += (ballY - (rightY + 48.0f)) * 0.035f;
        }
        if (ballX < 20.0f || ballX > 880.0f) {
            // A ball beyond either horizontal edge is a score. Reset only the
            // rally state so the scoreboard survives between serves.
            if (ballX < 20.0f) ++rightScore; else ++leftScore;
            ballX = 450.0f;
            ballY = 310.0f;
            vx = ballX < 100.0f ? 5.0f : (leftScore >= rightScore ? -5.0f : 5.0f);
            vy = 3.0f;
        }
    }

    void Draw(HDC dc) override {
        Header(dc, L"Ping Pong", L"W/S moves the left paddle. The right paddle is computer controlled. R restarts, Esc returns.");
        Fill(dc, Rect{ 24, 100, 852, 500 }, RGB(18, 23, 30));
        Frame(dc, Rect{ 24, 100, 852, 500 }, RGB(82, 94, 108), 2);
        for (int y = 112; y < 590; y += 32) Fill(dc, Rect{ 448, y, 4, 18 }, RGB(78, 88, 100));
        Fill(dc, Rect{ 46, static_cast<int>(leftY), 16, 96 }, RGB(86, 190, 136));
        Fill(dc, Rect{ 838, static_cast<int>(rightY), 16, 96 }, RGB(235, 115, 94));
        EllipseFill(dc, Rect{ static_cast<int>(ballX - 8), static_cast<int>(ballY - 8), 16, 16 }, RGB(245, 220, 120));
        CenterText(dc, Rect{ 320, 112, 100, 44 }, std::to_wstring(leftScore), 36, RGB(210, 230, 222), FW_BOLD);
        CenterText(dc, Rect{ 480, 112, 100, 44 }, std::to_wstring(rightScore), 36, RGB(240, 214, 210), FW_BOLD);
    }

    void KeyDown(WPARAM key) override {
        Game::KeyDown(key);
        if (key == 'W') up = true;
        if (key == 'S') down = true;
    }

    void KeyUp(WPARAM key) override {
        if (key == 'W') up = false;
        if (key == 'S') down = false;
    }
};

// App only depends on Game, so the concrete class remains private to this file.
std::unique_ptr<Game> CreatePingPongGame() {
    return std::make_unique<PingPongGame>();
}

