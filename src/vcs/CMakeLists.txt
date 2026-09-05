# vcs - layer 2. Git, driven through the real git binary (docs/ARCHITECTURE.md 5.3).

keys_add_module(vcs
    SOURCES
        GitClient.cpp
        GitTypes.cpp
        Repository.cpp
    HEADERS
        GitClient.h
        GitTypes.h
        Repository.h
    DEPENDS
        core
        process
)
