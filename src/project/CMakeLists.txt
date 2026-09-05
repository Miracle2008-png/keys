# project - layer 1. Root detection, metadata, ignore rules.

keys_add_module(project
    SOURCES
        IgnoreRules.cpp
        Project.cpp
    HEADERS
        IgnoreRules.h
        Project.h
    DEPENDS
        core
        filesystem
)
