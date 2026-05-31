#include "games.h"

// Concrete classes stay private inside their implementation files. Each module
// exposes one tiny constructor function so this central registry can create the
// game selected by the menu without publishing seven class definitions.
std::unique_ptr<Game> CreatePingPongGame();
std::unique_ptr<Game> CreateTicTacToeGame();
std::unique_ptr<Game> CreateSnakeGame();
std::unique_ptr<Game> CreateShooterGame();
std::unique_ptr<Game> CreateTankWarGame();
std::unique_ptr<Game> CreateMissionariesGame();
std::unique_ptr<Game> CreateTetrisGame();

std::unique_ptr<Game> CreateGame(Screen screen) {
    switch (screen) {
    case Screen::PingPong: return CreatePingPongGame();
    case Screen::TicTacToe: return CreateTicTacToeGame();
    case Screen::Snake: return CreateSnakeGame();
    case Screen::Shooter: return CreateShooterGame();
    case Screen::TankWar: return CreateTankWarGame();
    case Screen::Missionaries: return CreateMissionariesGame();
    case Screen::Tetris: return CreateTetrisGame();
    default: return nullptr;
    }
}
