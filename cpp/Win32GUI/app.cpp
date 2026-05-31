#include "app.h"

#include "games.h"

#include <array>
#include <string>

void App::SetWindow(HWND h) {
    hwnd = h;
}

void App::Select(Screen next) {
    // Replacing unique_ptr automatically destroys the previous game's state.
    // CreateGame returns null for Menu, which is represented by no active game.
    screen = next;
    game = CreateGame(screen);
    if (!game) {
        screen = Screen::Menu;
    }
    // FALSE skips background erasure because Draw repaints every pixel. This
    // reduces flicker and queues WM_PAINT without drawing out of band.
    InvalidateRect(hwnd, nullptr, FALSE);
}

void App::Update() {
    // Turn-based games inherit an empty Update. Real-time games advance once
    // per WM_TIMER message. Painting remains separate from simulation.
    if (game) game->Update();
    InvalidateRect(hwnd, nullptr, FALSE);
}

void App::DrawMenu(HDC dc) {
    Text(dc, 38, 34, L"Win32 GUI Games", 36, RGB(248, 251, 255), FW_BOLD);
    Text(dc, 42, 78, L"Native C++ and Win32 API versions of the console games. Pick one with the mouse or number keys.", 18, RGB(169, 180, 194));
    const std::array<std::wstring, 7> names = {
        L"1  Ping Pong", L"2  Tic-Tac-Toe", L"3  Snake", L"4  Shooter",
        L"5  Tank War", L"6  Missionaries & Cannibals", L"7  Tetris"
    };
    for (int i = 0; i < 7; ++i) {
        // The same geometry is reused in MouseDown for hit testing, so the
        // visible menu cards and their clickable regions cannot drift apart.
        Rect card{ 58 + (i % 2) * 400, 140 + (i / 2) * 92, 340, 62 };
        Fill(dc, card, RGB(31, 39, 50));
        Frame(dc, card, RGB(82, 96, 116), 1);
        CenterText(dc, card, names[i], 24, RGB(232, 237, 244), FW_BOLD);
    }
    Text(dc, 58, 532, L"Esc exits from here. Inside a game, Esc returns to this menu and R restarts.", 18, RGB(165, 176, 188));
}

void App::Draw(HDC dc) {
    // WindowProc gives us the off-screen memory HDC. Paint a complete frame so
    // the final BitBlt never exposes stale pixels from an earlier screen.
    Fill(dc, Rect{ 0, 0, ClientW, ClientH }, RGB(14, 18, 24));
    if (screen == Screen::Menu) DrawMenu(dc);
    else if (game) game->Draw(dc);
}

void App::KeyDown(WPARAM key) {
    if (screen == Screen::Menu) {
        // Playable Screen enum values are contiguous after Menu, allowing a
        // direct mapping from number keys without a duplicated switch.
        if (key >= '1' && key <= '7') {
            Select(static_cast<Screen>(static_cast<int>(Screen::PingPong) + static_cast<int>(key - '1')));
        }
        if (key == VK_ESCAPE) PostQuitMessage(0);
        return;
    }
    if (key == VK_ESCAPE) {
        // Escape is app-wide navigation, so each game does not repeat it.
        Select(Screen::Menu);
        return;
    }
    if (game) game->KeyDown(key);
}

void App::KeyUp(WPARAM key) {
    if (game) game->KeyUp(key);
}

void App::MouseDown(int x, int y) {
    if (screen == Screen::Menu) {
        for (int i = 0; i < 7; ++i) {
            Rect card{ 58 + (i % 2) * 400, 140 + (i / 2) * 92, 340, 62 };
            if (card.Contains(x, y)) {
                Select(static_cast<Screen>(static_cast<int>(Screen::PingPong) + i));
            }
        }
        return;
    }
    if (game) game->MouseDown(x, y);
}
