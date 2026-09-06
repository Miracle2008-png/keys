# update - layer 2. Asks GitHub whether a newer release exists.
#
# Its own module rather than a corner of core: it is the only part of Keys that
# touches the network, and keeping that in one place makes the claim easy to
# verify rather than something a reader has to take on trust.

keys_add_module(update
    SOURCES
        UpdateChecker.cpp
    HEADERS
        UpdateChecker.h
    DEPENDS
        core
        config
    QT
        Network
)

target_compile_definitions(keys_update PRIVATE KEYS_VERSION="${PROJECT_VERSION}")
