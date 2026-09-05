# workspace - layer 2. Composes project + session state; owns open/close ordering.

keys_add_module(workspace
    SOURCES
        FileTreeModel.cpp
        RecentProjects.cpp
        Workspace.cpp
    HEADERS
        FileTreeModel.h
        RecentProjects.h
        Workspace.h
    DEPENDS
        core
        config
        editor
        filesystem
        project
)
