#pragma once

namespace snake_game {

// Grid-based real-time Snake session. The public class deliberately exposes
// only Run(); simulation details remain private to the implementation file so
// the launcher depends on a small, stable contract.
class SnakeGame {
public:
    int Run();
};

}
