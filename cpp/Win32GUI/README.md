# Win32 GUI Games

This directory contains a native Win32/GDI window version of the console games.

Build from this directory:

```powershell
cmake -S . -B build
cmake --build build
```

Run:

```powershell
.\build\Debug\win32_gui_games.exe
```

The launcher opens one window with all games available from the number keys or the mouse:

- `1` Ping Pong
- `2` Tic-Tac-Toe
- `3` Snake
- `4` Shooter
- `5` Tank War
- `6` Missionaries & Cannibals
- `7` Tetris

Common controls: `Esc` returns to the menu, and `R` restarts the active game.

## Source layout

- `main.cpp` owns the Win32 window and message loop.
- `app.cpp` routes menu selection, input, updates, and drawing.
- `arcade.cpp` provides shared GDI drawing helpers and common game types.
- `games.cpp` maps menu screens to game factories.
- `games/` contains one implementation file per game.
