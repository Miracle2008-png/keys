find_package(Qt6 REQUIRED COMPONENTS Test)

keys_add_test(core       SOURCES CoreTests.cpp       DEPENDS core)
keys_add_test(config     SOURCES ConfigTests.cpp     DEPENDS config)
keys_add_test(filesystem SOURCES FileSystemTests.cpp DEPENDS filesystem)
keys_add_test(project    SOURCES ProjectTests.cpp    DEPENDS workspace)
keys_add_test(filetree   SOURCES FileTreeTests.cpp   DEPENDS workspace)
keys_add_test(theme      SOURCES ThemeTests.cpp      DEPENDS ui QT Gui Quick)
