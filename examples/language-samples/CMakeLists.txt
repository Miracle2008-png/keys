# Real CMake: targets, generator expressions, conditionals.
cmake_minimum_required(VERSION 3.28)
project(sample VERSION 1.2.3 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
find_package(Qt6 REQUIRED COMPONENTS Core Quick)

add_library(sample_core STATIC
    src/core.cpp
    src/core.h
)

target_link_libraries(sample_core PUBLIC Qt6::Core)
target_compile_options(sample_core PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W4 /WX>
    $<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-Wall -Wextra -Werror>
)

if(WIN32 AND NOT MINGW)
    target_compile_definitions(sample_core PRIVATE UNICODE _UNICODE)
endif()
