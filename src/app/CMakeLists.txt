# app - layer 4. The composition root: builds the object graph and runs the loop.

# The version in the comma-separated form VERSIONINFO requires.
set(KEYS_VERSION_RC "${PROJECT_VERSION_MAJOR},${PROJECT_VERSION_MINOR},${PROJECT_VERSION_PATCH},0")

qt_add_executable(keys
    main.cpp
    $<$<BOOL:${WIN32}>:${CMAKE_SOURCE_DIR}/resources/keys.rc>
)

# The icon and version block are compiled into the executable so Explorer, the
# taskbar and the installer can read them without running it.
if(WIN32)
    set_source_files_properties("${CMAKE_SOURCE_DIR}/resources/keys.rc"
        PROPERTIES COMPILE_DEFINITIONS
            "KEYS_VERSION_RC=${KEYS_VERSION_RC};KEYS_VERSION_STR=\"${PROJECT_VERSION}\""
    )
endif()

target_compile_definitions(keys PRIVATE
    KEYS_VERSION="${PROJECT_VERSION}"
)

target_link_libraries(keys PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::QuickDialogs2
    Keys::core
    Keys::config
    Keys::ui
    keys_uiplugin
)

set_target_properties(keys PROPERTIES
    WIN32_EXECUTABLE ON
    MACOSX_BUNDLE ON
    FOLDER "Application"
)

keys_apply_compile_options(keys)

# Copy the Qt runtime next to the executable so the build tree is directly
# runnable. Without this the app starts and immediately exits with a missing-DLL
# error that reports nothing useful, which is a poor first experience for anyone
# who has just cloned the repository.
# Install-time deployment belongs to packaging (cmake/KeysPackaging.cmake), which
# works out the real dependency set for the installer. This block exists only to
# keep the *build tree* directly runnable.
if(WIN32)
    # windeployqt at build time keeps `build\bin\keys.exe` runnable directly,
    # rather than only after an install step.
    find_program(KEYS_WINDEPLOYQT windeployqt HINTS "${QT6_INSTALL_PREFIX}/bin")
    if(KEYS_WINDEPLOYQT)
        add_custom_command(TARGET keys POST_BUILD
            COMMAND "${KEYS_WINDEPLOYQT}"
                    --no-translations
                    --no-system-d3d-compiler
                    --no-opengl-sw
                    --qmldir "${CMAKE_SOURCE_DIR}/src/ui/qml"
                    "$<TARGET_FILE:keys>"
            COMMENT "Deploying the Qt runtime next to keys.exe"
            VERBATIM
        )
    endif()
endif()
