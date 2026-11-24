cmake_minimum_required(VERSION 3.20)
project(CircuitRunner VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(CR_BUILD_GAME "Build the raylib front-end" ON)
option(CR_BUILD_TESTS "Build unit tests (GoogleTest)" ON)
option(CR_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(CR_SANITIZE "Build with AddressSanitizer + UBSan (GCC/Clang)" OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

include(FetchContent)

# ---------------------------------------------------------------- warnings
add_library(cr_options INTERFACE)
if(MSVC)
  target_compile_options(cr_options INTERFACE /W4 /permissive- /utf-8)
  if(CR_WARNINGS_AS_ERRORS)
    target_compile_options(cr_options INTERFACE /WX)
  endif()
else()
  target_compile_options(cr_options INTERFACE -Wall -Wextra -Wpedantic -Wshadow -Wnon-virtual-dtor)
  if(CR_WARNINGS_AS_ERRORS)
    target_compile_options(cr_options INTERFACE -Werror)
  endif()
  if(CR_SANITIZE)
    target_compile_options(cr_options INTERFACE -fsanitize=address,undefined -fno-omit-frame-pointer)
    target_link_options(cr_options INTERFACE -fsanitize=address,undefined)
  endif()
endif()

# ---------------------------------------------------------------- core (no graphics)
add_library(cr_core STATIC
  src/core/AutoPilot.cpp
  src/core/Game.cpp
  src/core/HighScores.cpp
  src/core/LevelConfig.cpp
  src/core/LevelLoader.cpp
  src/core/Maze.cpp
  src/core/MazeGenerator.cpp
  src/core/Replay.cpp
)
target_include_directories(cr_core PUBLIC src)
target_link_libraries(cr_core PUBLIC cr_options)

add_executable(circuit_runner_sim tools/sim_main.cpp)
target_link_libraries(circuit_runner_sim PRIVATE cr_core)

# ---------------------------------------------------------------- game (raylib)
if(CR_BUILD_GAME)
  find_package(raylib 5.0 QUIET)
  if(NOT raylib_FOUND)
    message(STATUS "raylib not found locally - fetching raylib 5.5")
    set(BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(BUILD_GAMES OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(raylib
      GIT_REPOSITORY https://github.com/raysan5/raylib.git
      GIT_TAG 5.5
      GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(raylib)
  endif()

  add_executable(circuit_runner
    src/app/main.cpp
    src/app/App.cpp
    src/app/Audio.cpp
    src/app/Renderer.cpp
  )
  target_link_libraries(circuit_runner PRIVATE cr_core raylib)
  if(APPLE)
    target_link_libraries(circuit_runner PRIVATE "-framework IOKit" "-framework Cocoa" "-framework OpenGL")
  endif()
  if(WIN32 AND CMAKE_BUILD_TYPE STREQUAL "Release")
    set_target_properties(circuit_runner PROPERTIES WIN32_EXECUTABLE ON)
    if(MSVC)
      target_link_options(circuit_runner PRIVATE /ENTRY:mainCRTStartup)
    endif()
  endif()
  add_custom_command(TARGET circuit_runner POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/assets $<TARGET_FILE_DIR:circuit_runner>/assets)
endif()

# ---------------------------------------------------------------- tests
if(CR_BUILD_TESTS)
  enable_testing()
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
  set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.15.2
    GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(googletest)

  add_executable(cr_tests
    tests/test_ai.cpp
    tests/test_game.cpp
    tests/test_generator.cpp
    tests/test_highscores.cpp
    tests/test_maze.cpp
    tests/test_movement.cpp
    tests/test_pathfinding.cpp
    tests/test_replay.cpp
  )
  target_link_libraries(cr_tests PRIVATE cr_core GTest::gtest_main)
  target_compile_definitions(cr_tests PRIVATE CR_LEVEL_DIR="${CMAKE_CURRENT_SOURCE_DIR}/assets/levels")
  include(GoogleTest)
  gtest_discover_tests(cr_tests)
endif()
