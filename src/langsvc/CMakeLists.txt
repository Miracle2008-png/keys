# langsvc - layer 2. LSP client: transport, lifecycle, capability negotiation.

keys_add_module(langsvc
    SOURCES
        JsonRpc.cpp
        LanguageClient.cpp
        LanguageServiceManager.cpp
        LspTypes.cpp
    HEADERS
        JsonRpc.h
        LanguageClient.h
        LanguageServiceManager.h
        LspTypes.h
    DEPENDS
        core
)
