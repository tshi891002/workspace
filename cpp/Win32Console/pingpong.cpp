#include <windows.h>
#include <vector>
#include <string>
#include <algorithm>
#include <chrono>
#include <thread>

// This program uses the Win32 console API directly to render a simple
// ping-pong game in the Windows console. It updates the full screen
// every frame using a CHAR_INFO buffer and WriteConsoleOutputW, which
// minimizes flicker compared to character-by-character writes.
static const int SCREEN_WIDTH = 80;
static const int SCREEN_HEIGHT = 25;
static const int PADDLE_HEIGHT = 5;
static const int PADDLE_MARGIN = 2;
static const wchar_t BALL_CHAR = L'O';
static const wchar_t PADDLE_CHAR = L'|';

struct Vec2 { int x; int y; };

void SetConsoleCursorVisible(HANDLE hConsole, bool visible) {
    // Hide or show the blinking console cursor. This is purely cosmetic
    // and keeps the game display from distracting the player.
    CONSOLE_CURSOR_INFO cursorInfo;
    cursorInfo.dwSize = 1;
    cursorInfo.bVisible = visible ? TRUE : FALSE;
    SetConsoleCursorInfo(hConsole, &cursorInfo);
}

void WriteConsoleBuffer(HANDLE hConsole, const std::vector<CHAR_INFO>& buffer) {
    // Write the entire console screen buffer in a single Win32 call.
    // WriteConsoleOutputW takes the character+attribute buffer and copies it
    // into the console window region, which is much less splashy than
    // many individual writes.
    COORD bufferSize = { SCREEN_WIDTH, SCREEN_HEIGHT };
    COORD bufferCoord = { 0, 0 };
    SMALL_RECT writeRegion = { 0, 0, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1 };
    WriteConsoleOutputW(hConsole, buffer.data(), bufferSize, bufferCoord, &writeRegion);
}

void ClearBuffer(std::vector<CHAR_INFO>& buffer) {
    // Initialize the screen buffer with blank space and a standard color
    // attribute. The buffer is reused every frame.
    CHAR_INFO blankChar;
    blankChar.Char.UnicodeChar = L' ';
    blankChar.Attributes = FOREGROUND_GREEN | FOREGROUND_BLUE | FOREGROUND_RED;
    std::fill(buffer.begin(), buffer.end(), blankChar);
}

int main() {
    // Get the console output handle for direct Win32 screen rendering.
    // We use the Windows console API rather than std::cout so we can update
    // the entire text grid each frame and control character attributes.
    HANDLE hConsole = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hConsole == INVALID_HANDLE_VALUE) {
        return 1;
    }

    // Configure the console window size and buffer size to a fixed resolution.
    // This ensures the game logic is written against a stable 80x25 display.
    SMALL_RECT windowSize = { 0, 0, SCREEN_WIDTH - 1, SCREEN_HEIGHT - 1 };
    COORD bufferSize = { SCREEN_WIDTH, SCREEN_HEIGHT };
    SetConsoleScreenBufferSize(hConsole, bufferSize);
    SetConsoleWindowInfo(hConsole, TRUE, &windowSize);
    SetConsoleCursorVisible(hConsole, false);

    // The screen buffer holds a CHAR_INFO cell for each console position.
    // Each frame we clear it and then write the title, paddles, ball, and score.
    std::vector<CHAR_INFO> screenBuffer(SCREEN_WIDTH * SCREEN_HEIGHT);
    ClearBuffer(screenBuffer);

    // Game state: paddle positions, ball position/velocity, and score.
    // Paddles are positioned along the left and right edges of the playfield.
    Vec2 leftPaddle = { PADDLE_MARGIN, SCREEN_HEIGHT / 2 - PADDLE_HEIGHT / 2 };
    Vec2 rightPaddle = { SCREEN_WIDTH - PADDLE_MARGIN - 1, SCREEN_HEIGHT / 2 - PADDLE_HEIGHT / 2 };
    Vec2 ball = { SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 };
    Vec2 ballVelocity = { 1, 1 };
    int scoreLeft = 0;
    int scoreRight = 0;
    const int frameDelayMs = 50;  // Frame rate control for playable speed.

    auto clamp = [&](int value, int minVal, int maxVal) {
        return value < minVal ? minVal : (value > maxVal ? maxVal : value);
    };

    // Main game loop: process input, update game state, render frame.
    bool running = true;
    while (running) {
        // Poll keyboard state using Win32 GetAsyncKeyState. This avoids blocking
        // input calls and allows both players to move their paddles smoothly.
        bool up1 = (GetAsyncKeyState('W') & 0x8000) != 0;
        bool down1 = (GetAsyncKeyState('S') & 0x8000) != 0;
        bool up2 = (GetAsyncKeyState(VK_UP) & 0x8000) != 0;
        bool down2 = (GetAsyncKeyState(VK_DOWN) & 0x8000) != 0;
        bool escape = (GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0;

        if (escape) {
            running = false;
        }

        // Move paddles within the vertical bounds of the playfield.
        if (up1) leftPaddle.y = clamp(leftPaddle.y - 1, 0, SCREEN_HEIGHT - PADDLE_HEIGHT);
        if (down1) leftPaddle.y = clamp(leftPaddle.y + 1, 0, SCREEN_HEIGHT - PADDLE_HEIGHT);
        if (up2) rightPaddle.y = clamp(rightPaddle.y - 1, 0, SCREEN_HEIGHT - PADDLE_HEIGHT);
        if (down2) rightPaddle.y = clamp(rightPaddle.y + 1, 0, SCREEN_HEIGHT - PADDLE_HEIGHT);

        // Move the ball each frame based on its current velocity vector.
        ball.x += ballVelocity.x;
        ball.y += ballVelocity.y;

        if (ball.y <= 0 || ball.y >= SCREEN_HEIGHT - 1) {
            // Bounce off the top and bottom walls by flipping the Y velocity.
            // Clamp keeps the ball inside the visible region.
            ballVelocity.y = -ballVelocity.y;
            ball.y = clamp(ball.y, 0, SCREEN_HEIGHT - 1);
        }

        // Detect paddle collisions by checking if the ball has reached the
        // horizontal plane of either paddle and its vertical position overlaps.
        bool hitLeftPaddle = ball.x == leftPaddle.x + 1 && ball.y >= leftPaddle.y && ball.y < leftPaddle.y + PADDLE_HEIGHT;
        bool hitRightPaddle = ball.x == rightPaddle.x - 1 && ball.y >= rightPaddle.y && ball.y < rightPaddle.y + PADDLE_HEIGHT;

        if (hitLeftPaddle || hitRightPaddle) {
            // Reverse the X direction when the ball collides with a paddle.
            // We also nudge the ball away from the paddle to prevent repeated
            // collision detection on the same frame.
            ballVelocity.x = -ballVelocity.x;
            if (ballVelocity.x > 0) {
                ball.x = leftPaddle.x + 2;
            } else {
                ball.x = rightPaddle.x - 2;
            }
        }

        if (ball.x <= 0) {
            // The ball exited the left side: right player scores.
            scoreRight++;
            ball = { SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 };
            ballVelocity = { 1, (ballVelocity.y > 0 ? 1 : -1) };
        }

        if (ball.x >= SCREEN_WIDTH - 1) {
            // The ball exited the right side: left player scores.
            scoreLeft++;
            ball = { SCREEN_WIDTH / 2, SCREEN_HEIGHT / 2 };
            ballVelocity = { -1, (ballVelocity.y > 0 ? 1 : -1) };
        }

        // Clear the frame buffer and draw the next frame from scratch.
        // This is simpler than trying to erase only changed characters.
        ClearBuffer(screenBuffer);

        std::wstring title = L"Win32 Console Ping-Pong";
        std::wstring controls = L"W/S = Left racket, Up/Down = Right racket, Esc = Quit";
        std::wstring scoreLine = L"Score: " + std::to_wstring(scoreLeft) + L" - " + std::to_wstring(scoreRight);

        WORD headerAttr = FOREGROUND_GREEN | FOREGROUND_INTENSITY;
        WORD ballAttr = FOREGROUND_RED | FOREGROUND_INTENSITY;
        WORD paddleAttr = FOREGROUND_GREEN | FOREGROUND_INTENSITY;

        for (size_t i = 0; i < title.size() && i < SCREEN_WIDTH; ++i) {
            auto& cell = screenBuffer[i];
            cell.Char.UnicodeChar = title[i];
            cell.Attributes = headerAttr;
        }
        for (size_t i = 0; i < controls.size() && i < SCREEN_WIDTH; ++i) {
            auto& cell = screenBuffer[SCREEN_WIDTH + static_cast<int>(i)];
            cell.Char.UnicodeChar = controls[i];
            cell.Attributes = headerAttr;
        }
        for (size_t i = 0; i < scoreLine.size() && i < SCREEN_WIDTH; ++i) {
            auto& cell = screenBuffer[2 * SCREEN_WIDTH + static_cast<int>(i)];
            cell.Char.UnicodeChar = scoreLine[i];
            cell.Attributes = headerAttr;
        }

        // Draw the paddles as vertical bars at the current paddle positions.
        for (int y = 0; y < PADDLE_HEIGHT; ++y) {
            int leftIndex = (leftPaddle.y + y) * SCREEN_WIDTH + leftPaddle.x;
            int rightIndex = (rightPaddle.y + y) * SCREEN_WIDTH + rightPaddle.x;
            auto& leftCell = screenBuffer[leftIndex];
            auto& rightCell = screenBuffer[rightIndex];
            leftCell.Char.UnicodeChar = PADDLE_CHAR;
            rightCell.Char.UnicodeChar = PADDLE_CHAR;
            leftCell.Attributes = paddleAttr;
            rightCell.Attributes = paddleAttr;
        }

        // Draw the ball at its current location. A single CHAR_INFO cell
        // contains both the character and its color attribute.
        int ballIndex = ball.y * SCREEN_WIDTH + ball.x;
        auto& ballCell = screenBuffer[ballIndex];
        ballCell.Char.UnicodeChar = BALL_CHAR;
        ballCell.Attributes = ballAttr;

        // Commit the frame to the console in one write operation.
        WriteConsoleBuffer(hConsole, screenBuffer);

        // Wait a short time before the next frame to control game speed.
        // The fixed delay keeps the ball movement consistent on fast systems.
        std::this_thread::sleep_for(std::chrono::milliseconds(frameDelayMs));
    }

    // Restore the console cursor before exiting so the terminal behaves normally.
    SetConsoleCursorVisible(hConsole, true);
    return 0;
}
