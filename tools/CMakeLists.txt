# Build-time tooling. Not part of the shipped application.

find_package(Qt6 REQUIRED COMPONENTS Gui Svg)

add_executable(keys_render_icons render-icons.cpp)
target_link_libraries(keys_render_icons PRIVATE Qt6::Core Qt6::Gui Qt6::Svg)
set_target_properties(keys_render_icons PROPERTIES FOLDER "Tools")
keys_apply_compile_options(keys_render_icons)

# Regenerates every raster icon from the SVG source.
#
# Run explicitly (cmake --build build --target icons) rather than on every build:
# the outputs are committed so a normal build needs neither the tool nor the
# SVG, and the icons only change when the mark itself does.
add_custom_target(icons
    COMMAND keys_render_icons
            "${CMAKE_SOURCE_DIR}/resources/icons/keys-mark.svg"
            "${CMAKE_SOURCE_DIR}/resources/icons/keys-mark-small.svg"
            "${CMAKE_SOURCE_DIR}/resources/icons/generated"
    DEPENDS keys_render_icons
    COMMENT "Rendering application icons from resources/icons/keys-mark.svg"
    VERBATIM
)

# A standalone check that ConPTY starts a real shell and its output reaches the
# parser. Not part of the application; run by hand when the PTY layer changes.
add_executable(keys_pty_check pty-check.cpp)
target_link_libraries(keys_pty_check PRIVATE Qt6::Core Keys::terminal)
set_target_properties(keys_pty_check PROPERTIES FOLDER "Tools")
keys_apply_compile_options(keys_pty_check)

# Renders the real SearchPanel against a real project. Catches the class of
# defect a model test cannot: a delegate missing a required property, a role
# name that does not match, a binding to something that does not exist - all
# silent at build time, all fatal at runtime.
add_executable(keys_search_panel_check search-panel-check.cpp)
# keys_uiplugin, not just keys_ui: the QML module is static, so the types are
# registered by the plugin object and an import without it resolves to nothing.
target_link_libraries(keys_search_panel_check PRIVATE
    Qt6::Core Qt6::Gui Qt6::Quick
    Keys::ui keys_uiplugin Keys::search Keys::project)
set_target_properties(keys_search_panel_check PROPERTIES FOLDER "Tools")
keys_apply_compile_options(keys_search_panel_check)
