# workspace - layer 2. Composes project + session state; owns open/close ordering.

keys_add_module(workspace
    SOURCES
        EditorGroup.cpp
        EditorLayout.cpp
        FileTreeModel.cpp
        RecentProjects.cpp
        Workspace.cpp
    HEADERS
        EditorGroup.h
        EditorLayout.h
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
