#pragma once

namespace missionaries_cannibals_game {

// Keyboard-driven river-crossing puzzle. The implementation keeps puzzle state,
// history, hint generation, and its turn-based Win32 UI together in one session.
class MissionariesCannibalsGame {
public:
    int Run();
};

}
