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
