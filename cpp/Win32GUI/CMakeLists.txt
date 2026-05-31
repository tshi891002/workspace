cmake_minimum_required(VERSION 3.10)
project(Win32GUIArcade LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(win32_gui_games WIN32
    main.cpp
    app.cpp
    arcade.cpp
    games.cpp
    games/ping_pong.cpp
    games/tic_tac_toe.cpp
    games/snake.cpp
    games/shooter.cpp
    games/tank_war.cpp
    games/missionaries.cpp
    games/tetris.cpp
)

target_include_directories(win32_gui_games PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

if (WIN32)
    target_compile_definitions(win32_gui_games PRIVATE UNICODE _UNICODE)
    target_link_libraries(win32_gui_games PRIVATE user32 gdi32)
endif()
