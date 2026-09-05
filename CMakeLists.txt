cmake_minimum_required(VERSION 3.28)

project(Keys
    VERSION 0.1.0
    DESCRIPTION "A fast, focused editor for people who build software for a living"
    LANGUAGES CXX
)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(KeysOptions)
include(KeysCompiler)
include(KeysModule)

find_package(Qt6 6.8 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 QuickDialogs2 Svg Concurrent)

qt_standard_project_setup(REQUIRES 6.8)

# Layers 0-2: Qt Core only. The linker enforces the rule that these never touch UI.
# Modules appear here as they are implemented; see docs/ARCHITECTURE.md section 10
# for the milestone each is scheduled in.
add_subdirectory(src/core)
add_subdirectory(src/filesystem)
add_subdirectory(src/config)
add_subdirectory(src/project)
add_subdirectory(src/workspace)

# Layer 3-4: the UI and the composition root.
add_subdirectory(src/ui)
add_subdirectory(src/app)

add_subdirectory(tools)

if(KEYS_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()

# Packaging last: the install rules it defines refer to targets declared above.
include(KeysPackaging)

message(STATUS "")
message(STATUS "Keys ${PROJECT_VERSION}")
message(STATUS "  Qt              ${Qt6_VERSION}")
message(STATUS "  Compiler        ${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS "  Build type      ${CMAKE_BUILD_TYPE}")
message(STATUS "  Tests           ${KEYS_BUILD_TESTS}")
message(STATUS "  Warnings->errors ${KEYS_WARNINGS_AS_ERRORS}")
message(STATUS "")
