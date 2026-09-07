# Warning configuration applied to every Keys target via keys_apply_compile_options().
#
# The brief says do not ignore compiler warnings. The build makes that structural:
# warnings are errors by default, so a warning cannot survive to be ignored.

function(keys_apply_compile_options target)
    if(MSVC)
        target_compile_options(${target} PRIVATE
            /W4
            /permissive-        # strict conformance; rejects MS-specific laxness
            /Zc:__cplusplus     # report the real __cplusplus value
            /Zc:preprocessor    # conforming preprocessor
            /utf-8              # source and execution charset
            /MP                 # parallel compilation

            # Not in /W4, and worth having: a switch over an enum that does not
            # handle every enumerator. Adding a language to the highlighter left
            # the status bar's name lookup silently returning nothing, and the
            # build said nothing because this is off by default.
            /w14062             # unhandled enumerator in a switch

            # Suppressed because Qt's own headers trigger them, not Keys' code.
            # /external:W0 silences Qt headers included directly, but these fire
            # while instantiating Qt templates from our translation units, which
            # /external does not cover.
            #
            #   4127 - "conditional expression is constant": Qt macros expand to
            #          do{...}while(0) and to constant conditions.
            #   4702 - "unreachable code": Qt uses `if constexpr` chains with a
            #          trailing return that is dead for some instantiations
            #          (qjsengine.h fromScriptValue, qvariant.h fromValue).
            #
            # Both are legitimate patterns in template code. Nothing else is
            # relaxed: Keys' own code is still compiled at /W4 /WX.
            /wd4127
            /wd4702
        )
        if(KEYS_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE /WX)
        endif()
    else()
        target_compile_options(${target} PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wshadow
            -Wnon-virtual-dtor
            -Wcast-align
            -Wunused
            -Woverloaded-virtual
            -Wdouble-promotion

            # The GCC/Clang equivalent of MSVC's C4062: a switch over an enum
            # that does not handle every enumerator and has no default.
            -Wswitch-enum
        )
        if(KEYS_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()

    # Qt hygiene: catch string conversions and deprecated API at compile time
    # rather than discovering them at runtime.
    target_compile_definitions(${target} PRIVATE
        QT_NO_CAST_FROM_ASCII
        QT_NO_CAST_TO_ASCII
        QT_NO_URL_CAST_FROM_STRING
        QT_USE_QSTRINGBUILDER
        QT_DISABLE_DEPRECATED_UP_TO=0x060800
    )
endfunction()
