cmake_minimum_required(VERSION 3.15)
project(snes_emu)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find SDL2
find_package(SDL2 CONFIG REQUIRED)

# Source files
set(SOURCES
    src/main.cpp
    src/cpu/cpu.cpp
    src/memory/memory.cpp
    src/ppu/ppu.cpp
    src/apu/apu.cpp
    src/input/input.cpp
    src/debug/logger.cpp
)

# Create executable
add_executable(snes_emu ${SOURCES})

# Link SDL2
target_link_libraries(snes_emu PRIVATE SDL2::SDL2 SDL2::SDL2main)

# Set output directory
set_target_properties(snes_emu PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/$<CONFIG>"
)

# Copy SDL2 DLL to output directory on Windows
if(WIN32)
    add_custom_command(TARGET snes_emu POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:SDL2::SDL2>
        $<TARGET_FILE_DIR:snes_emu>
    )
endif()
