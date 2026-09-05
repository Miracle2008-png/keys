# search - layer 2. File index, fuzzy matching, project-wide text search.

keys_add_module(search
    SOURCES
        FileIndex.cpp
        FuzzyMatch.cpp
        TextSearch.cpp
    HEADERS
        FileIndex.h
        FuzzyMatch.h
        TextSearch.h
    DEPENDS
        core
        filesystem
        project
)
