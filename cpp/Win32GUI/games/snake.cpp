#include "games.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <deque>
#include <random>
#include <string>
#include <vector>

// Snake stores logical cells in a deque: the front is the head and the back is
// the tail. Its tick counter moves the snake every seventh WM_TIMER update so
// the app can repaint smoothly while gameplay remains readable.
class SnakeGame final : public Game {
    std::deque<Vec2> snake;
    Vec2 food;
    Vec2 dir{ 1, 0 };
    Vec2 queued{ 1, 0 };
    int score = 0;
    int tick = 0;
    bool dead = false;
    static constexpr int W = 30;
    static constexpr int H = 20;

    bool Contains(Vec2 p) const {
        return std::find_if(snake.begin(), snake.end(), [p](Vec2 s) { return s.x == p.x && s.y == p.y; }) != snake.end();
    }

    void PlaceFood() {
        // Build a list of unoccupied interior cells, then choose one uniformly.
        // This guarantees food never appears inside the snake or border wall.
        std::vector<Vec2> cells;
        for (int y = 1; y < H - 1; ++y) for (int x = 1; x < W - 1; ++x) if (!Contains({ x, y })) cells.push_back({ x, y });
        if (!cells.empty()) food = cells[std::uniform_int_distribution<int>(0, static_cast<int>(cells.size() - 1))(Rng())];
    }

public:
    void Reset() override {
        snake = { {15,10}, {14,10}, {13,10} };
        dir = queued = { 1, 0 };
        score = tick = 0;
        dead = false;
        PlaceFood();
    }

    SnakeGame() { Reset(); }

    void Update() override {
        if (dead || ++tick < 7) return;
        tick = 0;
        dir = queued;
        Vec2 head{ snake.front().x + dir.x, snake.front().y + dir.y };
        if (head.x <= 0 || head.x >= W - 1 || head.y <= 0 || head.y >= H - 1 || Contains(head)) {
            dead = true;
            return;
        }
        snake.push_front(head);
        // Eating keeps the old tail, growing the deque by one. A normal move
        // removes the tail after inserting the new head.
        if (head.x == food.x && head.y == food.y) {
            score += 10;
            PlaceFood();
        } else {
            snake.pop_back();
        }
    }

    void Draw(HDC dc) override {
        Header(dc, L"Snake", L"Arrow keys steer. Eat the food without hitting walls or yourself. R restarts, Esc returns.");
        Rect board{ 120, 110, 660, 440 };
        Fill(dc, board, RGB(19, 27, 25));
        Frame(dc, board, RGB(78, 112, 96), 2);
        int cell = 22;
        int ox = board.x;
        int oy = board.y;
        for (int y = 0; y < H; ++y) for (int x = 0; x < W; ++x) {
            if (x == 0 || y == 0 || x == W - 1 || y == H - 1) Fill(dc, Rect{ ox + x * cell, oy + y * cell, cell, cell }, RGB(75, 91, 82));
        }
        EllipseFill(dc, Rect{ ox + food.x * cell + 4, oy + food.y * cell + 4, 14, 14 }, RGB(238, 91, 84));
        bool first = true;
        for (Vec2 s : snake) {
            Fill(dc, Rect{ ox + s.x * cell + 2, oy + s.y * cell + 2, cell - 4, cell - 4 }, first ? RGB(112, 229, 154) : RGB(68, 180, 116));
            first = false;
        }
        Text(dc, 782, 126, L"Score", 18, RGB(167, 178, 188));
        Text(dc, 782, 150, std::to_wstring(score), 32, RGB(227, 242, 232), FW_BOLD);
        if (dead) CenterText(dc, Rect{ 238, 276, 424, 64 }, L"Game Over", 38, RGB(246, 116, 101), FW_BOLD);
    }

    void KeyDown(WPARAM key) override {
        Game::KeyDown(key);
        Vec2 nd = queued;
        if (key == VK_UP) nd = { 0, -1 };
        if (key == VK_DOWN) nd = { 0, 1 };
        if (key == VK_LEFT) nd = { -1, 0 };
        if (key == VK_RIGHT) nd = { 1, 0 };
        // Prevent an immediate 180-degree reversal into the second segment.
        if (nd.x + dir.x != 0 || nd.y + dir.y != 0) queued = nd;
    }
};

std::unique_ptr<Game> CreateSnakeGame() {
    return std::make_unique<SnakeGame>();
}

