# extensions - layer 2. Out-of-process extension host (docs/ARCHITECTURE.md 5.7).
#
# Depends on langsvc for the message framing only. Extensions speak JSON-RPC over
# stdio, which is the same wire format the language client uses.

keys_add_module(extensions
    SOURCES
        ExtensionHost.cpp
        ExtensionRegistry.cpp
        Manifest.cpp
    HEADERS
        ExtensionHost.h
        ExtensionRegistry.h
        Manifest.h
    DEPENDS
        core
        config
        filesystem
        langsvc
)
