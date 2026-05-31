#pragma once

namespace shooter_game {

// Fixed-timestep arcade shooter. Run() performs setup, simulation, rendering,
// and terminal restoration for one session before returning to its caller.
class ShooterGame {
public:
    int Run();
};

}
