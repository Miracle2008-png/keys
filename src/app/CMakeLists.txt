# app - layer 4. The composition root: builds the object graph and runs the loop.

qt_add_executable(keys
    main.cpp
)

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
if(WIN32)
    qt_generate_deploy_qml_app_script(
        TARGET keys
        OUTPUT_SCRIPT KEYS_DEPLOY_SCRIPT
        MACOS_BUNDLE_POST_BUILD
        NO_UNSUPPORTED_PLATFORM_ERROR
        DEPLOY_USER_QML_MODULES_ON_UNSUPPORTED_PLATFORM
    )
    install(SCRIPT ${KEYS_DEPLOY_SCRIPT})

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
