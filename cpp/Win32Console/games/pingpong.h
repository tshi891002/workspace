#pragma once

namespace pingpong_game {

// Real-time two-player paddle game. Run() owns one complete play session and
// returns when Escape is pressed, allowing either a standalone executable or
// the combined launcher to create the game through the same interface.
class PingPongGame {
public:
    int Run();
};

}
