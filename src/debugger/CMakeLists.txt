# debugger - layer 2. DAP client: breakpoints, stack, variables, stepping.
#
# Depends on langsvc for the message framing only: DAP and LSP share the
# Content-Length header but not the envelope, so the codec is reused and the
# message shapes are read separately.

keys_add_module(debugger
    SOURCES
        DapTypes.cpp
        DebugSession.cpp
    HEADERS
        DapTypes.h
        DebugSession.h
    DEPENDS
        core
        langsvc
)
