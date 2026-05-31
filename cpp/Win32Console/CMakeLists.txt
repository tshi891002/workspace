cmake_minimum_required(VERSION 3.10)
project(Win32ConsoleGames LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(console_game_lib STATIC
    games/missionaries_cannibals.cpp
    games/pingpong.cpp
    games/shooter.cpp
    games/snake.cpp
    games/tankwar.cpp
    games/tetris.cpp
    games/tictactoe.cpp
)
target_include_directories(console_game_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

function(add_console_game target entry_source)
    add_executable(${target} ${entry_source})
    target_link_libraries(${target} PRIVATE console_game_lib)
endfunction()

add_executable(console_games
    console_games.cpp
    launcher/launcher.cpp
)
target_link_libraries(console_games PRIVATE console_game_lib)

if (WIN32)
    target_compile_definitions(console_game_lib PUBLIC UNICODE _UNICODE)
endif()
