# editor - layer 2. Text buffer, cursors, undo. Knows nothing about rendering.

keys_add_module(editor
    SOURCES
        DocumentSearch.cpp
        FoldModel.cpp
        SyntaxHighlighter.cpp
        TextBuffer.cpp
        TextDocument.cpp
        UndoStack.cpp
    HEADERS
        DocumentSearch.h
        FoldModel.h
        SyntaxHighlighter.h
        TextBuffer.h
        TextDocument.h
        UndoStack.h
    DEPENDS
        core
)
