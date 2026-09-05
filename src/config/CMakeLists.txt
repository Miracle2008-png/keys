# config - layer 1. Settings schema, layered resolution, persistence.

keys_add_module(config
    SOURCES
        AnimationPolicy.cpp
        Settings.cpp
        SettingsSchema.cpp
        SettingsStore.cpp
    HEADERS
        AnimationPolicy.h
        Settings.h
        SettingsSchema.h
        SettingsStore.h
    DEPENDS
        core
)
