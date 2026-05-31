#pragma once

#include "arcade.h"

#include <memory>

// App is the bridge between the Win32 message layer and the individual games.
// Win32 owns the actual HWND. App remembers that handle so state changes can
// request repainting, and owns exactly one active game through unique_ptr.
class App {
    HWND hwnd = nullptr;
    Screen screen = Screen::Menu;
    std::unique_ptr<Game> game;

public:
    void SetWindow(HWND h);
    void Select(Screen next);
    void Update();
    void DrawMenu(HDC dc);
    void Draw(HDC dc);
    void KeyDown(WPARAM key);
    void KeyUp(WPARAM key);
    void MouseDown(int x, int y);
};
