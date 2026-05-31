#pragma once

// windows.h is intentionally centralized in this header because every module
// uses Win32 handle types such as HWND and HDC. WIN32_LEAN_AND_MEAN omits rarely
// used Windows headers, which keeps compilation lighter. NOMINMAX prevents the
// Windows min/max macros from interfering with std::min and std::max.
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <random>
#include <string>

// The games draw against a fixed-size client area. "Client area" means the
// drawable interior of the window, excluding the title bar and borders.
constexpr int ClientW = 900;
constexpr int ClientH = 620;

// SetTimer identifies each timer with an integer ID. Windows posts WM_TIMER to
// our message queue approximately every 16 ms, giving animated games a simple
// update cadence near 60 Hz. This is a UI timer, not a precision physics clock.
constexpr UINT_PTR TimerId = 1;
constexpr int TimerMs = 16;

// Game code is easier to read with x/y/width/height rectangles. Native Win32
// APIs instead expect RECT values containing left/top/right/bottom edges.
struct Rect {
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;

    RECT ToWin() const;
    bool Contains(int px, int py) const;
};

// Integer coordinates used by grid-based games such as Snake and Tetris.
struct Vec2 {
    int x = 0;
    int y = 0;
};

// Shared drawing helpers wrap the repetitive lifetime rules of GDI objects.
// HDC is a "device context": a Win32 handle representing a drawing target.
// COLORREF values are created with RGB(red, green, blue).
void Fill(HDC dc, const Rect& r, COLORREF color);
void Frame(HDC dc, const Rect& r, COLORREF color, int width = 1);
void EllipseFill(HDC dc, const Rect& r, COLORREF fill, COLORREF outline = RGB(20, 20, 20));
void Text(HDC dc, int x, int y, const std::wstring& text, int size = 22,
          COLORREF color = RGB(235, 238, 242), int weight = FW_NORMAL);
void CenterText(HDC dc, const Rect& r, const std::wstring& text, int size = 22,
                COLORREF color = RGB(235, 238, 242), int weight = FW_NORMAL);
void Header(HDC dc, const std::wstring& title, const std::wstring& help);

// Screen is app-level navigation state. The playable values deliberately remain
// contiguous so keys 1..7 can be converted to a Screen with a small offset.
enum class Screen {
    Menu,
    PingPong,
    TicTacToe,
    Snake,
    Shooter,
    TankWar,
    Missionaries,
    Tetris
};

// Each game implements the same narrow interface. WindowProc never needs to
// know game rules: it forwards operating-system events to App, and App forwards
// them to the currently selected Game. Default handlers keep turn-based games
// from having to implement empty update or key-release methods.
class Game {
public:
    virtual ~Game() = default;
    virtual void Reset() = 0;
    virtual void Update() {}
    virtual void Draw(HDC dc) = 0;
    virtual void KeyDown(WPARAM key) {
        if (key == 'R') {
            Reset();
        }
    }
    virtual void KeyUp(WPARAM) {}
    virtual void MouseDown(int, int) {}
};

// A single process-wide random engine is enough for this small arcade. It is
// seeded once from the Windows uptime counter in arcade.cpp.
std::mt19937& Rng();
