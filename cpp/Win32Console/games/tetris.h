#pragma once

namespace tetris_game {

// Falling-block puzzle session. Tetris polls key state inside a timed loop and
// uses the shared console screen abstraction to restore terminal state on exit.
class TetrisGame {
public:
    int Run();
};

}
