# buildrun - layer 2. Runs build and run tasks and parses their output.

keys_add_module(buildrun
    SOURCES
        ProblemParser.cpp
        Task.cpp
        TaskRunner.cpp
    HEADERS
        ProblemParser.h
        Task.h
        TaskRunner.h
    DEPENDS
        core
        project
)
