cmake_minimum_required(VERSION 3.20)
project(sbc-over-codec2-sender LANGUAGES C CXX)

# Use C++20
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# FetchContent module
include(FetchContent)

# --- Fetch Codec2 if not installed ---
FetchContent_Declare(
    codec2
    GIT_REPOSITORY https://github.com/drowe67/codec2.git
    GIT_TAG main
    GIT_SHALLOW ON
    GIT_PROGRESS ON
)

set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE) # Build static library
set(CODEC2_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE) # Don't build examples
set(CODEC2_BUILD_PROGRAMS OFF CACHE BOOL "" FORCE) # Don't build programs
set(CODEC2_BUILD_TESTS OFF CACHE BOOL "" FORCE) # Don't build tests

FetchContent_MakeAvailable(codec2)

# --- Find PipeWire ---
find_package(PkgConfig REQUIRED)
pkg_check_modules(PIPEWIRE REQUIRED libpipewire-0.3)

# --- Compiler flags ---
add_compile_options(${PIPEWIRE_CFLAGS_OTHER})

# --- Source files ---
set(SOURCES
    src/main.cc
    src/pw-stream.cc
    src/encoder.cc
    src/wav_file.cc
)

# --- Executable ---
add_executable(sender ${SOURCES})

# --- Include directories ---
target_include_directories(sender PUBLIC
    ${PIPEWIRE_INCLUDE_DIRS}
    ${codec2_SOURCE_DIR}
    ${codec2_BINARY_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# --- Link libraries ---
target_link_libraries(sender
    ${PIPEWIRE_LIBRARIES}
    codec2
    m      # math library
)

# --- Optional: RPATH fix for Mac/Linux if needed ---
