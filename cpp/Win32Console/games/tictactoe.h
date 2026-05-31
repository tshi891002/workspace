#pragma once

namespace tictactoe_game {

// Turn-based Tic-Tac-Toe session. Unlike the arcade games, this class waits for
// structured Win32 key events because the board changes only after user input.
class TicTacToeGame {
public:
    int Run();
};

}
