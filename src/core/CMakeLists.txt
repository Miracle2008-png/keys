# core - layer 0. Depends on nothing but Qt Core.

keys_add_module(core
    SOURCES
        CommandRegistry.cpp
        Log.cpp
        TaskScheduler.cpp
        Trace.cpp
    HEADERS
        Cancellation.h
        CommandRegistry.h
        Log.h
        Result.h
        TaskScheduler.h
        Trace.h
)
