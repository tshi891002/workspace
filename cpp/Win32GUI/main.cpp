#include "app.h"

#include <windowsx.h>

namespace {

// This program creates one native window for the entire arcade. The global App
// is intentionally small: WindowProc is a C-style callback required by Win32,
// so it forwards messages into this C++ controller.
App g_app;

// Windows calls this function whenever the window receives a message. WPARAM
// and LPARAM are pointer-sized integer slots whose meaning depends on msg.
// Returning 0 means a handled message should not receive default processing.
LRESULT CALLBACK WindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_CREATE:
        // WM_CREATE arrives during CreateWindowExW. Save the HWND and start the
        // timer that drives Update. A null callback makes Windows post WM_TIMER
        // messages to this same WindowProc.
        g_app.SetWindow(hwnd);
        SetTimer(hwnd, TimerId, TimerMs, nullptr);
        return 0;
    case WM_TIMER:
        // Update changes simulation state. It does not draw immediately;
        // App::Update invalidates the window so Windows schedules WM_PAINT.
        g_app.Update();
        return 0;
    case WM_KEYDOWN:
        // Virtual-key codes include ordinary uppercase letters and constants
        // such as VK_LEFT, VK_SPACE, and VK_ESCAPE.
        g_app.KeyDown(wParam);
        return 0;
    case WM_KEYUP:
        g_app.KeyUp(wParam);
        return 0;
    case WM_LBUTTONDOWN:
        // Mouse coordinates are packed into lParam. windowsx.h provides macros
        // that unpack signed x/y values in client-area coordinates.
        g_app.MouseDown(GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        return 0;
    case WM_PAINT: {
        // BeginPaint returns an HDC for the invalid portion of the real window.
        // Drawing the whole scene directly into it can flicker, so use classic
        // double buffering: render into a compatible memory bitmap, then copy
        // the completed frame to the visible HDC with one BitBlt operation.
        PAINTSTRUCT ps{};
        HDC dc = BeginPaint(hwnd, &ps);
        HDC mem = CreateCompatibleDC(dc);
        HBITMAP bmp = CreateCompatibleBitmap(dc, ClientW, ClientH);
        HGDIOBJ old = SelectObject(mem, bmp);
        g_app.Draw(mem);
        // SRCCOPY means copy source pixels unchanged to the destination.
        BitBlt(dc, 0, 0, ClientW, ClientH, mem, 0, 0, SRCCOPY);
        // Restore the original bitmap before releasing temporary GDI objects.
        SelectObject(mem, old);
        DeleteObject(bmp);
        DeleteDC(mem);
        EndPaint(hwnd, &ps);
        return 0;
    }
    case WM_DESTROY:
        // Closing the HWND stops future timer events. PostQuitMessage places a
        // WM_QUIT sentinel in the queue so the message loop below terminates.
        KillTimer(hwnd, TimerId);
        PostQuitMessage(0);
        return 0;
    }
    // Let Windows provide normal behavior for messages we do not customize,
    // including moving, resizing, activation, and title-bar interactions.
    return DefWindowProcW(hwnd, msg, wParam, lParam);
}

} // namespace

int WINAPI wWinMain(HINSTANCE instance, HINSTANCE, PWSTR, int showCmd) {
    // wWinMain is the Unicode GUI-subsystem entry point. A WNDCLASS describes
    // the kind of window we want and connects it to WindowProc.
    const wchar_t ClassName[] = L"Win32GUIArcadeWindow";
    WNDCLASSW wc{};
    wc.lpfnWndProc = WindowProc;
    wc.hInstance = instance;
    wc.lpszClassName = ClassName;
    wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
    wc.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
    RegisterClassW(&wc);

    // CreateWindowExW expects an outer-window size. AdjustWindowRect expands
    // the desired 900x620 client area to account for borders and the title bar.
    RECT rect{ 0, 0, ClientW, ClientH };
    AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, FALSE);
    HWND hwnd = CreateWindowExW(0, ClassName, L"Win32 GUI Games", WS_OVERLAPPEDWINDOW & ~WS_MAXIMIZEBOX,
                                CW_USEDEFAULT, CW_USEDEFAULT, rect.right - rect.left, rect.bottom - rect.top,
                                nullptr, nullptr, instance, nullptr);
    if (!hwnd) return 0;

    // ShowWindow makes the HWND visible. UpdateWindow asks Windows to process
    // the initial paint now instead of waiting for a later queue iteration.
    ShowWindow(hwnd, showCmd);
    UpdateWindow(hwnd);

    MSG msg{};
    // The standard Win32 message loop blocks in GetMessageW, translates input
    // where appropriate, and dispatches each message to WindowProc. It exits
    // when PostQuitMessage causes GetMessageW to return zero.
    while (GetMessageW(&msg, nullptr, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
    }
    return static_cast<int>(msg.wParam);
}
