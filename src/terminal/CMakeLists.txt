# terminal - layer 2. VT parsing and the screen model. Draws nothing.

keys_add_module(terminal
    SOURCES
        TerminalScreen.cpp
        TerminalSession.cpp
        VtParser.cpp
    HEADERS
        TerminalScreen.h
        TerminalSession.h
        VtParser.h
    DEPENDS
        core
        process
)
