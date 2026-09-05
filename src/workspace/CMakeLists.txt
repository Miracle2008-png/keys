# workspace - layer 2. Composes project + session state; owns open/close ordering.

keys_add_module(workspace
    SOURCES
        RecentProjects.cpp
        Workspace.cpp
    HEADERS
        RecentProjects.h
        Workspace.h
    DEPENDS
        core
        config
        filesystem
        project
)
