cmake_minimum_required(VERSION 3.22)
project(Analog83 VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---- Sound engine: plain C++, no JUCE.
# drumsynth/ is the folder that goes into KICK at
# Kick/modules/engine/objects/drumsynth/ (compiled there through objects.cpp).
add_library(analog83_engine STATIC
    drumsynth/Analog83Kick.cpp
    drumsynth/Analog83HiHat.cpp
    drumsynth/Analog83Kit.cpp
    drumsynth/Analog83Record.cpp
)
target_include_directories(analog83_engine PUBLIC drumsynth)

# ---- Dev tools (stay in the fork, not in the KICK PR)
add_library(analog83_tools INTERFACE)   # Analog83DemoPattern.h: stands in for KICK's sequencer
target_include_directories(analog83_tools INTERFACE tools)

# ---- Test program: renders WAV files
add_executable(analog83_test tools/analog83_test.cpp)
target_link_libraries(analog83_test PRIVATE analog83_engine analog83_tools)

# ---- Test suite: feature checks, threads, allocations and render fingerprints
find_package(Threads REQUIRED)
add_executable(analog83_tests tools/analog83_tests.cpp)
target_link_libraries(analog83_tests PRIVATE analog83_engine analog83_tools Threads::Threads)
target_compile_definitions(analog83_tests PRIVATE
    A83_FINGERPRINT_FILE="${CMAKE_CURRENT_SOURCE_DIR}/tools/analog83_fingerprints.txt")

enable_testing()
add_test(NAME analog83_tests COMMAND analog83_tests)

# ---- Benchmark: per-stage and whole-kit CPU cost (build in Release)
add_executable(analog83_bench tools/analog83_bench.cpp)
target_link_libraries(analog83_bench PRIVATE analog83_engine analog83_tools)

if(MSVC)
    target_compile_options(analog83_engine PRIVATE /W4)
    target_compile_options(analog83_test PRIVATE /W4)
    target_compile_options(analog83_bench PRIVATE /W4)
    target_compile_options(analog83_tests PRIVATE /W4)
else()
    target_compile_options(analog83_engine PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(analog83_test PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(analog83_bench PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(analog83_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

# ---- Preview app: the kit page on screen (downloads JUCE on first configure)
option(A83_BUILD_APP "Build the JUCE kit page preview app" ON)

if(A83_BUILD_APP)
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        7.0.12   # same JUCE version the KICK app uses
        GIT_SHALLOW    ON)
    FetchContent_MakeAvailable(JUCE)
    add_subdirectory(app)
endif()
