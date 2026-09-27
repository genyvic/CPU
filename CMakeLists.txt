cmake_minimum_required(VERSION 3.16)
project(rv32i_sim CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if (MSVC)
    add_compile_options(/W4 /permissive-)
else()
    add_compile_options(-Wall -Wextra)
endif()

# The CPU model itself: everything under src/ except main.cpp, so both the
# simulator CLI and the test runner can link against it.
add_library(rv32i_core STATIC
    src/RegisterFile.cpp
    src/Memory.cpp
    src/Decoder.cpp
    src/Disassembler.cpp
    src/CPU.cpp
    src/Tracer.cpp
    src/Coverage.cpp
    src/ProgramLoader.cpp
)
target_include_directories(rv32i_core PUBLIC src)

add_executable(rv32i-sim src/main.cpp)
target_link_libraries(rv32i-sim PRIVATE rv32i_core)

enable_testing()
add_subdirectory(tests)
