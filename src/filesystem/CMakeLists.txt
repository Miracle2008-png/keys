# filesystem - layer 1. Async file I/O, directory listing, watching.

keys_add_module(filesystem
    SOURCES
        FileSystem.cpp
        FileWatcher.cpp
    HEADERS
        FileSystem.h
        FileWatcher.h
    DEPENDS
        core
)
