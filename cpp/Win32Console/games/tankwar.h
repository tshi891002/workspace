#pragma once

namespace tankwar_game {

// Console battlefield game with player input, enemy simulation, and buffered
// rendering. A fresh object represents a fresh session selected by the launcher.
class TankWarGame {
public:
    int Run();
};

}
