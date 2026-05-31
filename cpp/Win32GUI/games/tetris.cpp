#include "games.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <deque>
#include <random>
#include <string>
#include <vector>

// Tetris uses a fixed 10x20 integer grid. Zero means empty and values 1..7 map
// to tetromino colors. The active falling piece stays separate from the grid
// until Lock runs, allowing movement and rotation to test prospective states.
class TetrisGame final : public Game {
    static constexpr int W = 10;
    static constexpr int H = 20;
    std::array<int, W * H> grid{};
    int piece = 0, rot = 0, px = 3, py = 0;
    int tick = 0, score = 0;
    bool over = false;
    const std::array<std::array<Vec2, 4>, 7> shapes{ {
        {{{0,1},{1,1},{2,1},{3,1}}},
        {{{1,0},{2,0},{1,1},{2,1}}},
        {{{1,0},{0,1},{1,1},{2,1}}},
        {{{0,0},{1,0},{1,1},{2,1}}},
        {{{1,0},{2,0},{0,1},{1,1}}},
        {{{0,0},{0,1},{1,1},{2,1}}},
        {{{2,0},{0,1},{1,1},{2,1}}}
    } };
    const std::array<COLORREF, 8> colors{ RGB(25,31,40), RGB(91,184,232), RGB(236,214,94), RGB(176,116,226), RGB(93,205,142), RGB(230,104,89), RGB(96,139,226), RGB(238,156,83) };

    // Rotate a tetromino cell clockwise inside a conceptual 4x4 box. Repeating
    // the same transform r times keeps the seven source shapes compact.
    Vec2 Rotate(Vec2 p, int r) const {
        for (int i = 0; i < r; ++i) p = { 3 - p.y, p.x };
        return p;
    }

    // Collision checks are speculative: callers pass a possible position and
    // rotation, and only apply that move if the proposed cells remain inside
    // the board and do not overlap locked blocks.
    bool Collide(int nx, int ny, int nr) const {
        for (auto b : shapes[piece]) {
            Vec2 q = Rotate(b, nr);
            int x = nx + q.x;
            int y = ny + q.y;
            if (x < 0 || x >= W || y >= H) return true;
            if (y >= 0 && grid[y * W + x]) return true;
        }
        return false;
    }

    void Spawn() {
        piece = std::uniform_int_distribution<int>(0, 6)(Rng());
        rot = 0;
        px = 3;
        py = -1;
        if (Collide(px, py, rot)) over = true;
    }

    // Once a falling piece can no longer descend, merge it into the permanent
    // grid, remove completed rows, shift remaining rows downward, and spawn the
    // next piece. This is the central Tetris state transition.
    void Lock() {
        for (auto b : shapes[piece]) {
            Vec2 q = Rotate(b, rot);
            int x = px + q.x;
            int y = py + q.y;
            if (y >= 0 && x >= 0 && x < W && y < H) grid[y * W + x] = piece + 1;
        }
        for (int y = H - 1; y >= 0; --y) {
            bool full = true;
            for (int x = 0; x < W; ++x) full = full && grid[y * W + x] != 0;
            if (full) {
                for (int yy = y; yy > 0; --yy) for (int x = 0; x < W; ++x) grid[yy * W + x] = grid[(yy - 1) * W + x];
                for (int x = 0; x < W; ++x) grid[x] = 0;
                score += 100;
                ++y;
            }
        }
        Spawn();
    }

public:
    void Reset() override {
        grid.fill(0);
        tick = score = 0;
        over = false;
        Spawn();
    }

    TetrisGame() { Reset(); }

    void Update() override {
        if (over) return;
        if (++tick < 25) return;
        tick = 0;
        if (!Collide(px, py + 1, rot)) ++py; else Lock();
    }

    void DrawBlock(HDC dc, int x, int y, int value) {
        int size = 24;
        Fill(dc, Rect{ 330 + x * size, 104 + y * size, size - 2, size - 2 }, colors[value]);
    }

    void Draw(HDC dc) override {
        Header(dc, L"Tetris", L"Left/Right moves, Up rotates, Down soft drops, Space hard drops. R restarts, Esc returns.");
        Fill(dc, Rect{ 318, 92, 264, 504 }, RGB(20, 25, 33));
        Frame(dc, Rect{ 318, 92, 264, 504 }, RGB(88, 100, 118), 2);
        for (int y = 0; y < H; ++y) for (int x = 0; x < W; ++x) DrawBlock(dc, x, y, grid[y * W + x]);
        for (auto b : shapes[piece]) {
            Vec2 q = Rotate(b, rot);
            if (py + q.y >= 0) DrawBlock(dc, px + q.x, py + q.y, piece + 1);
        }
        Text(dc, 608, 126, L"Score", 18, RGB(170, 180, 192));
        Text(dc, 608, 152, std::to_wstring(score), 32, RGB(236, 239, 244), FW_BOLD);
        if (over) CenterText(dc, Rect{ 285, 286, 330, 64 }, L"Game Over", 36, RGB(239, 114, 99), FW_BOLD);
    }

    void KeyDown(WPARAM key) override {
        Game::KeyDown(key);
        if (over) return;
        if (key == VK_LEFT && !Collide(px - 1, py, rot)) --px;
        if (key == VK_RIGHT && !Collide(px + 1, py, rot)) ++px;
        if (key == VK_DOWN && !Collide(px, py + 1, rot)) ++py;
        if (key == VK_UP && !Collide(px, py, (rot + 1) % 4)) rot = (rot + 1) % 4;
        if (key == VK_SPACE) { while (!Collide(px, py + 1, rot)) ++py; Lock(); }
    }
};

std::unique_ptr<Game> CreateTetrisGame() {
    return std::make_unique<TetrisGame>();
}

