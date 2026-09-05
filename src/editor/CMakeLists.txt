# editor - layer 2. Text buffer, cursors, undo. Knows nothing about rendering.

keys_add_module(editor
    SOURCES
        TextBuffer.cpp
        TextDocument.cpp
        UndoStack.cpp
    HEADERS
        TextBuffer.h
        TextDocument.h
        UndoStack.h
    DEPENDS
        core
)
