# process - layer 1. Child processes and pseudo-terminals.
#
# The ConPTY implementation is the only Windows-specific code in Keys. A Unix
# backend over forkpty implements the same Pty interface, so nothing above this
# module changes when one is added.

if(WIN32)
    set(KEYS_PTY_SOURCES PtyWindows.cpp)
    set(KEYS_PTY_HEADERS PtyWindows.h)
else()
    message(FATAL_ERROR
        "The process module has a Windows (ConPTY) backend only so far. "
        "A Unix backend over forkpty is the next platform step.")
endif()

keys_add_module(process
    SOURCES
        CommandRunner.cpp
        Pty.cpp
        ${KEYS_PTY_SOURCES}
    HEADERS
        CommandRunner.h
        Pty.h
        ${KEYS_PTY_HEADERS}
    DEPENDS
        core
)
