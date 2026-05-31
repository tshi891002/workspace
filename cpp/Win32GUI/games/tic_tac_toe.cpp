#include "games.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <deque>
#include <random>
#include <string>
#include <vector>

// Tic-Tac-Toe is turn-based, so it inherits Game::Update unchanged. The board
// stores nine marks in row-major order. selected is shared by keyboard and mouse
// input, giving Draw one place to obtain the highlighted square.
class TicTacToeGame final : public Game {
    std::array<wchar_t, 9> board{};
    wchar_t player = L'X';
    int selected = 0;
    std::wstring message;

    wchar_t Winner(std::array<int, 3>* line = nullptr) const {
        // Optionally return the winning cells as well as the mark. Draw uses
        // that extra information to highlight a completed line.
        constexpr int lines[8][3] = {
            {0,1,2},{3,4,5},{6,7,8},{0,3,6},{1,4,7},{2,5,8},{0,4,8},{2,4,6}
        };
        for (const auto& l : lines) {
            if (board[l[0]] != L' ' && board[l[0]] == board[l[1]] && board[l[1]] == board[l[2]]) {
                if (line) *line = { l[0], l[1], l[2] };
                return board[l[0]];
            }
        }
        if (line) *line = { -1, -1, -1 };
        return L' ';
    }

    bool Full() const {
        return std::none_of(board.begin(), board.end(), [](wchar_t c) { return c == L' '; });
    }

    void Place() {
        // Centralizing placement keeps keyboard and mouse rules identical.
        if (Winner() != L' ' || Full()) return;
        if (board[selected] != L' ') {
            message = L"That square is already taken.";
            return;
        }
        board[selected] = player;
        player = player == L'X' ? L'O' : L'X';
        message.clear();
    }

public:
    void Reset() override {
        board.fill(L' ');
        player = L'X';
        selected = 0;
        message.clear();
    }

    TicTacToeGame() { Reset(); }

    void Draw(HDC dc) override {
        Header(dc, L"Tic-Tac-Toe", L"Arrow keys or 1-9 selects a square. Enter/Space places a mark. R restarts, Esc returns.");
        const Rect boardRect{ 270, 120, 360, 360 };
        Fill(dc, boardRect, RGB(22, 28, 36));
        Frame(dc, boardRect, RGB(87, 99, 116), 2);
        std::array<int, 3> line{};
        wchar_t winner = Winner(&line);
        for (int i = 1; i < 3; ++i) {
            Fill(dc, Rect{ boardRect.x + i * 120 - 2, boardRect.y + 12, 4, 336 }, RGB(91, 104, 120));
            Fill(dc, Rect{ boardRect.x + 12, boardRect.y + i * 120 - 2, 336, 4 }, RGB(91, 104, 120));
        }
        for (int i = 0; i < 9; ++i) {
            int row = i / 3;
            int col = i % 3;
            Rect cell{ boardRect.x + col * 120 + 10, boardRect.y + row * 120 + 10, 100, 100 };
            bool win = i == line[0] || i == line[1] || i == line[2];
            if (i == selected) Fill(dc, cell, RGB(42, 65, 90));
            if (win) Fill(dc, cell, RGB(48, 104, 75));
            std::wstring mark(1, board[i] == L' ' ? static_cast<wchar_t>(L'1' + i) : board[i]);
            COLORREF color = board[i] == L'X' ? RGB(238, 112, 99) : RGB(91, 205, 148);
            if (board[i] == L' ') color = RGB(115, 126, 141);
            CenterText(dc, cell, mark, 54, color, FW_BOLD);
        }
        if (winner != L' ') {
            Text(dc, 285, 510, std::wstring(L"Player ") + winner + L" wins. Press R to play again.", 24, RGB(107, 221, 159), FW_BOLD);
        } else if (Full()) {
            Text(dc, 320, 510, L"Draw. Press R to play again.", 24, RGB(236, 212, 123), FW_BOLD);
        } else {
            Text(dc, 340, 510, std::wstring(L"Current player: ") + player, 24, RGB(227, 232, 238), FW_BOLD);
        }
        if (!message.empty()) Text(dc, 330, 548, message, 18, RGB(240, 130, 116));
    }

    void KeyDown(WPARAM key) override {
        Game::KeyDown(key);
        if (key >= '1' && key <= '9') selected = static_cast<int>(key - '1');
        if (key == VK_LEFT && selected % 3 > 0) --selected;
        if (key == VK_RIGHT && selected % 3 < 2) ++selected;
        if (key == VK_UP && selected >= 3) selected -= 3;
        if (key == VK_DOWN && selected < 6) selected += 3;
        if (key == VK_RETURN || key == VK_SPACE) Place();
    }

    void MouseDown(int x, int y) override {
        // Convert client-area pixels into a row and column in the 3x3 board.
        Rect boardRect{ 270, 120, 360, 360 };
        if (!boardRect.Contains(x, y)) return;
        selected = ((y - boardRect.y) / 120) * 3 + ((x - boardRect.x) / 120);
        Place();
    }
};

std::unique_ptr<Game> CreateTicTacToeGame() {
    return std::make_unique<TicTacToeGame>();
}

