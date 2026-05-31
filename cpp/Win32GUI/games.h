#pragma once

#include "arcade.h"

#include <memory>

// The factory hides concrete game classes from App. This keeps app.cpp stable
// as game implementations evolve and gives every game its own translation unit.
std::unique_ptr<Game> CreateGame(Screen screen);
