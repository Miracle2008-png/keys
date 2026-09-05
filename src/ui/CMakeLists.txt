# ui - layer 3. The only module permitted to link Qt GUI (see KeysModule.cmake).

qt_add_library(keys_ui STATIC)
add_library(Keys::ui ALIAS keys_ui)

qt_add_qml_module(keys_ui
    URI Keys.Ui
    VERSION 1.0
    OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/qml/Keys/Ui

    SOURCES
        AppController.cpp
        AppController.h
        CommandPaletteModel.cpp
        CommandPaletteModel.h
        EditorBindings.cpp
        EditorBindings.h
        EditorViewModel.cpp
        EditorViewModel.h
        TabBarModel.cpp
        TabBarModel.h
        Metrics.h
        OklchColor.cpp
        OklchColor.h
        Theme.cpp
        Theme.h

    QML_FILES
        qml/Main.qml
        qml/ActivityRail.qml
        qml/CodeEditor.qml
        qml/CommandPalette.qml
        qml/EditorPane.qml
        qml/EditorTabBar.qml
        qml/ContextMenu.qml
        qml/DeleteConfirmDialog.qml
        qml/DialogButton.qml
        qml/EditorArea.qml
        qml/ErrorToast.qml
        qml/ExplorerContextMenu.qml
        qml/ExplorerPanel.qml
        qml/FolderPicker.qml
        qml/Icon.qml
        qml/IconButton.qml
        qml/Keycap.qml
        qml/NameDialog.qml
        qml/PrimaryButton.qml
        qml/SectionLabel.qml
        qml/Sidebar.qml
        qml/StatusBar.qml
        qml/TopBar.qml
        qml/WelcomeView.qml
)

# QML-declared singletons must be marked as such, or the module registers them as
# ordinary types and every reference resolves to undefined at runtime.
set_source_files_properties(
    qml/Fonts.qml
    qml/Icons.qml
    PROPERTIES QT_QML_SINGLETON_TYPE TRUE
)

qt_target_qml_sources(keys_ui
    QML_FILES
        qml/Fonts.qml
        qml/Icons.qml
)

# The mark ships inside the binary so the window icon and the QML logo come from
# one source and cannot drift apart, and so a stray file on disk cannot change
# the application's identity.
qt_add_resources(keys_ui "keys_branding"
    PREFIX "/branding"
    BASE "${CMAKE_SOURCE_DIR}/resources/icons"
    FILES
        "${CMAKE_SOURCE_DIR}/resources/icons/keys-mark.svg"
        "${CMAKE_SOURCE_DIR}/resources/icons/generated/keys-256.png"
)

target_include_directories(keys_ui PUBLIC "${CMAKE_SOURCE_DIR}/src")

target_link_libraries(keys_ui PUBLIC
    Qt6::Core
    Qt6::Gui
    Qt6::Quick
    Qt6::QuickControls2
    Qt6::QuickDialogs2
    Qt6::Svg
    Keys::core
    Keys::config
    Keys::search
    Keys::workspace
)

set_target_properties(keys_ui PROPERTIES FOLDER "Modules")
keys_apply_compile_options(keys_ui)
