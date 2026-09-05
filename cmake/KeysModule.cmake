# keys_add_module(<name>
#     SOURCES  <files...>
#     HEADERS  <files...>          # public headers, for IDE integration
#     DEPENDS  <keys modules...>   # other keys_* targets
#     QT       <Qt components...>  # Qt6::Core is implicit
# )
#
# Creates a static library keys_<name> whose public include directory is the
# module's parent (src/), so consumers write #include <core/Result.h> and the
# module a header belongs to is visible at every use site.
#
# Layering (see docs/ARCHITECTURE.md §2) is enforced here: only modules listed in
# KEYS_UI_LAYER_MODULES may link Qt GUI components. Any other module that asks for
# Gui/Quick/QuickControls2 fails configuration rather than quietly creating a
# dependency that would be painful to unwind later.

set(KEYS_UI_LAYER_MODULES ui app CACHE INTERNAL "Modules permitted to link Qt GUI")
set(KEYS_GUI_COMPONENTS Gui Quick QuickControls2 QuickTest Widgets
    CACHE INTERNAL "Qt components restricted to the UI layer")

function(keys_add_module name)
    cmake_parse_arguments(ARG "" "" "SOURCES;HEADERS;DEPENDS;QT" ${ARGN})

    if(NOT ARG_SOURCES)
        message(FATAL_ERROR "keys_add_module(${name}): SOURCES is required")
    endif()

    if(NOT name IN_LIST KEYS_UI_LAYER_MODULES)
        foreach(component IN LISTS ARG_QT)
            if(component IN_LIST KEYS_GUI_COMPONENTS)
                message(FATAL_ERROR
                    "Layering violation: module '${name}' requested Qt6::${component}.\n"
                    "Only the UI layer (${KEYS_UI_LAYER_MODULES}) may link Qt GUI components.\n"
                    "See docs/ARCHITECTURE.md section 2.")
            endif()
        endforeach()
    endif()

    set(target keys_${name})
    add_library(${target} STATIC ${ARG_SOURCES} ${ARG_HEADERS})
    add_library(Keys::${name} ALIAS ${target})

    target_include_directories(${target} PUBLIC "${CMAKE_SOURCE_DIR}/src")

    set(qt_targets Qt6::Core)
    foreach(component IN LISTS ARG_QT)
        list(APPEND qt_targets Qt6::${component})
    endforeach()

    set(keys_targets "")
    foreach(dep IN LISTS ARG_DEPENDS)
        list(APPEND keys_targets Keys::${dep})
    endforeach()

    target_link_libraries(${target} PUBLIC ${qt_targets} ${keys_targets})

    set_target_properties(${target} PROPERTIES
        AUTOMOC ON
        POSITION_INDEPENDENT_CODE ON
        FOLDER "Modules"
    )

    keys_apply_compile_options(${target})
endfunction()


# keys_add_test(<name> SOURCES <files...> DEPENDS <keys modules...>)
#
# One test binary per module, registered with CTest.
function(keys_add_test name)
    cmake_parse_arguments(ARG "" "" "SOURCES;DEPENDS;QT" ${ARGN})

    set(target keys_test_${name})
    add_executable(${target} ${ARG_SOURCES})

    set(qt_targets Qt6::Core Qt6::Test)
    foreach(component IN LISTS ARG_QT)
        list(APPEND qt_targets Qt6::${component})
    endforeach()

    set(keys_targets "")
    foreach(dep IN LISTS ARG_DEPENDS)
        list(APPEND keys_targets Keys::${dep})
    endforeach()

    target_link_libraries(${target} PRIVATE ${qt_targets} ${keys_targets})
    set_target_properties(${target} PROPERTIES AUTOMOC ON FOLDER "Tests")
    keys_apply_compile_options(${target})

    add_test(NAME ${name} COMMAND ${target})

    # Qt DLLs live outside the build tree on Windows; put them on PATH for the run.
    #
    # ENVIRONMENT_MODIFICATION rather than ENVIRONMENT: the latter *replaces* PATH
    # with whatever the configuring shell happened to have, freezing it into the
    # test. That silently removed git from the path of the vcs tests, which then
    # skipped themselves and reported as passing - a test that cannot run must
    # not look like a test that ran.
    if(WIN32)
        get_target_property(qt_core_location Qt6::Core IMPORTED_LOCATION_RELEASE)
        if(NOT qt_core_location)
            get_target_property(qt_core_location Qt6::Core IMPORTED_LOCATION_DEBUG)
        endif()
        if(qt_core_location)
            get_filename_component(qt_bin_dir "${qt_core_location}" DIRECTORY)
            set_tests_properties(${name} PROPERTIES
                ENVIRONMENT_MODIFICATION "PATH=path_list_prepend:${qt_bin_dir}")
        endif()
    endif()
endfunction()
