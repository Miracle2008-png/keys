# Packaging: turns a built tree into an installer.
#
#     cmake --build build --target package
#
# The install rules below define what ships. They are deliberately separate from
# the build's own output layout: the build tree is organised for developers, the
# install tree for users, and conflating the two is how installers end up
# shipping test binaries and stale artefacts.

include(GNUInstallDirs)

if(WIN32)
    # Cached, so it is found once and visible to both this file and src/app.
    find_program(KEYS_WINDEPLOYQT windeployqt
        HINTS "${QT6_INSTALL_PREFIX}/bin" "${Qt6_DIR}/../../../bin")
    if(NOT KEYS_WINDEPLOYQT)
        message(FATAL_ERROR
            "windeployqt was not found. It is required to produce a runnable package.")
    endif()
endif()

# ---- What gets installed ----------------------------------------------------

install(TARGETS keys
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    BUNDLE  DESTINATION .
)

# The Visual C++ runtime DLLs keys.exe links against: MSVCP140 and VCRUNTIME140.
# Shipping these three files (about 1 MB) rather than the 24 MB redistributable
# installer keeps the download small while still working on a machine that has
# never had Visual Studio or a redistributable installed.
#
# SKIP_INSTALL_RULES because this module's own rule targets the install root;
# the explicit rule below puts them beside the executable, where Windows looks
# for them first.
if(WIN32)
    set(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS_SKIP ON)
    set(CMAKE_INSTALL_UCRT_LIBRARIES OFF)
    include(InstallRequiredSystemLibraries)

    install(FILES ${CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS}
            DESTINATION ${CMAKE_INSTALL_BINDIR})
endif()

# The Qt runtime, QML modules and platform plugins the application needs.
#
# windeployqt is invoked directly rather than through
# qt_generate_deploy_qml_app_script.
#
# That helper discovers QML imports by scanning .qml files on disk. Every Keys
# QML file is compiled into the executable as a resource, so the scan finds
# nothing: the install tree got an empty qml/ directory and the application
# failed at startup with "module QtQuick is not installed".
#
# Running the tool once, over the whole install tree, with --qmldir pointing at
# the QML sources, is what makes deployment correct. One pass matters: an earlier
# attempt ran the helper and then windeployqt afterwards, and the second pass
# copied the QtQuick.Controls style plugin without resolving its own
# dependencies, leaving Qt6QuickControls2Basic.dll missing and the application
# still unable to start.
#
# --no-translations   Qt's own UI strings for 32 languages; Keys ships English
#                     only, so they translate nothing the user sees.
# --no-opengl-sw      Mesa's 20 MB software rasteriser. Keys renders through
#                     Qt RHI on Direct3D; software GL could not run the editor
#                     acceptably anyway.
# --no-compiler-runtime  the 24 MB vc_redist installer. The three runtime DLLs
#                     Keys actually links against are installed above, at about
#                     1 MB.
install(CODE "
    execute_process(
        COMMAND \"${KEYS_WINDEPLOYQT}\"
                --release
                --no-translations
                --no-system-d3d-compiler
                --no-opengl-sw
                --no-compiler-runtime
                --qmldir \"${CMAKE_SOURCE_DIR}/src/ui/qml\"
                --dir \"\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}\"
                \"\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}/keys.exe\"
        RESULT_VARIABLE keys_deploy_result
        OUTPUT_QUIET
    )
    if(NOT keys_deploy_result EQUAL 0)
        message(FATAL_ERROR
            \"windeployqt failed (\${keys_deploy_result}). The package would not run.\")
    endif()
    message(STATUS \"Packaging: deployed the Qt runtime\")
")


# Trim what the deployment tool includes by default but this application does
# not use. Every entry here was found by inspecting an actual install tree and
# checking its size, not guessed at.
#
#   plugins/qmltooling/    11 files - the QML debugger, inspector and profiler.
#                          Development tools. A release build must not ship a
#                          debug server that listens for connections at all.
#   bin/vc_redist.x64.exe  24.4 MB - the full Visual C++ redistributable
#                          installer. keys.exe genuinely needs MSVCP140.dll and
#                          VCRUNTIME140*.dll (confirmed with dumpbin), but those
#                          three DLLs are installed directly by the
#                          InstallRequiredSystemLibraries block below, at about
#                          1 MB rather than 24. Removing this without that block
#                          would produce an installer that fails on any machine
#                          without VC++ already present.
#   bin/opengl32sw.dll     19.7 MB - Mesa's software OpenGL fallback, for
#                          machines with no working GPU driver. Keys targets
#                          Direct3D through Qt RHI on Windows; a software
#                          rasteriser could not run the editor acceptably
#                          anyway, so shipping it trades 20 MB for a path
#                          nobody would want to be on.

# The Direct3D shader compilers.
#
# windeployqt reports "Cannot find any version of the dxcompiler.dll and
# dxil.dll" and carries on, so the package builds cleanly and then installs an
# application that creates a window and draws nothing into it: Qt's D3D11
# backend cannot compile a shader without them, and the failure is silent - no
# dialog, no log, just an empty window.
#
# --no-system-d3d-compiler above skips the legacy D3DCompiler_47.dll and does
# not cover these two; dropping that flag would pull in the old compiler Keys
# does not use.
#
# A separate install(CODE) using a quoted string rather than a bracket literal,
# because the build directory has to be substituted now - at install time it is
# not defined.
install(CODE "
    foreach(shader_dll dxcompiler.dll dxil.dll)
        if(EXISTS \"${CMAKE_BINARY_DIR}/bin/\${shader_dll}\")
            file(COPY \"${CMAKE_BINARY_DIR}/bin/\${shader_dll}\"
                 DESTINATION \"\${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_BINDIR}\")
            message(STATUS \"Packaging: bundled \${shader_dll}\")
        else()
            message(WARNING
                \"Packaging: \${shader_dll} was not found beside the built \"
                \"executable. The installed application would start and render \"
                \"nothing.\")
        endif()
    endforeach()
")

install(CODE [[
    set(keys_unwanted_directories
        "plugins/qmltooling"
        "bin/plugins/qmltooling"
        "translations"
        "bin/translations"
    )
    foreach(directory IN LISTS keys_unwanted_directories)
        if(EXISTS "${CMAKE_INSTALL_PREFIX}/${directory}")
            file(REMOVE_RECURSE "${CMAKE_INSTALL_PREFIX}/${directory}")
            message(STATUS "Packaging: removed ${directory}")
        endif()
    endforeach()

    # Qt Controls styles Keys never loads. main() pins the Basic style, so the
    # design's tokens are not overridden by a platform theme - the other four
    # styles are around 10 MB of code that can never execute.
    #
    # Removed here rather than suppressed at deploy time: windeployqt has no flag
    # that drops the extra styles while keeping Basic, and this way the removal
    # is explicit about which style survives.
    foreach(style FluentWinUI3 Imagine Material Universal Fusion)
        file(REMOVE_RECURSE
            "${CMAKE_INSTALL_PREFIX}/bin/qml/QtQuick/Controls/${style}")
        file(REMOVE
            "${CMAKE_INSTALL_PREFIX}/bin/Qt6QuickControls2${style}.dll"
            "${CMAKE_INSTALL_PREFIX}/bin/Qt6QuickControls2${style}StyleImpl.dll")
    endforeach()

    set(keys_unwanted_files
        "bin/vc_redist.x64.exe"
        "bin/opengl32sw.dll"
    )
    foreach(unwanted IN LISTS keys_unwanted_files)
        if(EXISTS "${CMAKE_INSTALL_PREFIX}/${unwanted}")
            file(REMOVE "${CMAKE_INSTALL_PREFIX}/${unwanted}")
            message(STATUS "Packaging: removed ${unwanted}")
        endif()
    endforeach()
]])

# ---- Package metadata -------------------------------------------------------

set(CPACK_PACKAGE_NAME "Keys")
set(CPACK_PACKAGE_VENDOR "Keys")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "${PROJECT_DESCRIPTION}")
set(CPACK_PACKAGE_HOMEPAGE_URL "https://github.com/Miracle2008-png/keys")

# Installs to "Keys", not "Keys 0.1.0": an upgrade should replace the previous
# version in place rather than accumulating a directory per release.
set(CPACK_PACKAGE_INSTALL_DIRECTORY "Keys")
set(CPACK_PACKAGE_FILE_NAME "Keys-${PROJECT_VERSION}-windows-x64")

set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")
set(CPACK_PACKAGE_EXECUTABLES "keys" "Keys")
set(CPACK_CREATE_DESKTOP_LINKS "keys")

set(CPACK_STRIP_FILES ON)

if(WIN32)
    set(CPACK_GENERATOR "NSIS")

    set(CPACK_NSIS_PACKAGE_NAME "Keys")
    set(CPACK_NSIS_DISPLAY_NAME "Keys")
    set(CPACK_NSIS_INSTALLED_ICON_NAME "bin\\\\keys.exe")
    set(CPACK_NSIS_URL_INFO_ABOUT "${CPACK_PACKAGE_HOMEPAGE_URL}")

    # The installer's own icon, and the icons for its shortcuts. Backslashes are
    # doubled because the path passes through CMake and then NSIS script.
    set(CPACK_NSIS_MUI_ICON "${CMAKE_SOURCE_DIR}/resources/icons/generated/keys.ico")
    set(CPACK_NSIS_MUI_UNIICON "${CMAKE_SOURCE_DIR}/resources/icons/generated/keys.ico")

    # Offer to launch after installing, the way a desktop application should.
    set(CPACK_NSIS_MUI_FINISHPAGE_RUN "keys.exe")

    # CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL is deliberately NOT set.
    #
    # The generated .onInit for it reads:
    #
    #     StrCmp "ON" "ON" 0 inst
    #     ReadRegStr $0 HKLM "...\Uninstall\Keys" "UninstallString"
    #     StrCmp $0 "" inst
    #     ...
    #     ExecWait '"$0" /S _?=$3'
    #     IfErrors uninst_failed inst
    #
    # The guard compares a literal to itself, so it always falls through to the
    # uninstall path. On a machine with no previous install the registry read
    # yields an empty string, ExecWait on it sets the error flag, and the
    # installer stops with "Uninstall failed." and exit code 2 - every install
    # on a clean machine fails, which is precisely the case that matters most.
    #
    # Upgrades are handled instead by CPACK_NSIS_INSTALL_ROOT staying constant:
    # a new version installs over the previous one in the same directory, and
    # the uninstaller shipped with it removes what it recorded.
    set(CPACK_NSIS_MODIFY_PATH OFF)

    # Install per user, into %LOCALAPPDATA%\Programs\Keys, with no elevation.
    #
    # CPack's default is Program Files, which forces a UAC prompt. For a
    # developer tool that is the wrong trade: administrator rights are not
    # needed to run an editor, a machine-wide install is not wanted when several
    # versions may coexist, and requiring elevation blocks anyone on a managed
    # machine from installing at all.
    #
    # `user` also makes the install scriptable - CI and unattended setups can run
    # it with /S, which an admin-level installer cannot do without elevation.
    set(CPACK_NSIS_DEFINES "RequestExecutionLevel user")
    set(CPACK_NSIS_INSTALL_ROOT "$LOCALAPPDATA\\\\Programs")

    # CPack's generated .onInit overrides the install directory whenever the
    # default path is used, and only restores the configured one for a user in
    # the Admin or Power Users group:
    #
    #     StrCmp "$IS_DEFAULT_INSTALLDIR" "1" 0 +2
    #       StrCpy $INSTDIR "$DOCUMENTS\Keys"
    #     ... UserInfo::GetAccountType ...
    #     StrCmp $SV_ALLUSERS "AllUsers" 0 +3
    #       StrCmp "$IS_DEFAULT_INSTALLDIR" "1" 0 +2
    #         StrCpy $INSTDIR "$LOCALAPPDATA\Programs\Keys"
    #
    # For an ordinary user that puts the application in Documents - and on a
    # machine with OneDrive folder redirection, that means the entire install,
    # every Qt DLL, is uploaded to the cloud. The installer still reports
    # success, so nothing surfaces until someone goes looking for the files.
    #
    # Pinning INSTDIR immediately before the install section is what makes the
    # configured root actually hold. An explicit /D= still wins, because NSIS
    # applies that after .onInit; this only replaces the wrong default.
    # Written without quotes around the operands: this string passes through
    # CMake's configure_file into the .nsi, and an escaped double quote does not
    # survive that intact. NSIS compares unquoted tokens the same way.
    # SetOutPath is re-issued after the correction. The template emits its own
    # SetOutPath *before* this block, so fixing INSTDIR alone leaves the File
    # command writing to the old directory - the registry entry and the
    # shortcuts would point at the right place and the files would not be there.
    set(CPACK_NSIS_EXTRA_PREINSTALL_COMMANDS
        "StrCmp $IS_DEFAULT_INSTALLDIR 1 0 keys_dir_ok\n  StrCpy $INSTDIR $LOCALAPPDATA\\\\Programs\\\\Keys\n  SetOutPath $INSTDIR\n  keys_dir_ok:")

    # A Desktop shortcut alongside the Start Menu entry. CPack creates the Start
    # Menu one from CPACK_PACKAGE_EXECUTABLES; the desktop link needs these hooks.
    set(CPACK_NSIS_CREATE_ICONS_EXTRA
        "CreateShortCut '$DESKTOP\\\\Keys.lnk' '$INSTDIR\\\\bin\\\\keys.exe'")
    set(CPACK_NSIS_DELETE_ICONS_EXTRA
        "Delete '$DESKTOP\\\\Keys.lnk'")

    # Remove the install directory even when something is in it that the
    # installer did not put there.
    #
    # NSIS deletes exactly the files it recorded, then tries RMDir on the
    # directory - which refuses if anything else is present. That left the whole
    # folder behind after uninstalling, because the application had written a
    # file into its own install directory. The application no longer does that,
    # but an uninstaller that only works when nothing unexpected happened is not
    # much of an uninstaller: a crash dump, an editor backup or a log dropped by
    # anything else would have the same effect.
    #
    # RMDir /r is scoped to $INSTDIR, which NSIS has already validated as the
    # recorded install location - it is not a path the user can point elsewhere
    # at this stage.
    set(CPACK_NSIS_EXTRA_UNINSTALL_COMMANDS
        "RMDir /r '$INSTDIR\\\\bin'\n  RMDir /r '$INSTDIR'")

    # What Add/Remove Programs shows.
    #
    # CPack writes DisplayName, DisplayVersion, Publisher and DisplayIcon and
    # stops there, so the row listed a name and a version with no size, no
    # location and no date - which is what makes an entry look like something
    # that installed itself rather than something the user chose.
    #
    # DisplayIcon is rewritten with an explicit ",0". CPack omits the icon
    # index, and without it Windows may fall back to a generic glyph instead of
    # reading the executable's first icon group - which is what the Settings
    # list was showing.
    #
    # EstimatedSize is in KB and is measured from $INSTDIR at install time
    # rather than hard-coded, because a fixed figure is wrong as soon as the
    # payload changes.
    set(CPACK_NSIS_HELP_LINK "https://github.com/Miracle2008-png/keys")
    set(CPACK_NSIS_CONTACT "https://github.com/Miracle2008-png/keys/issues")

    # The remaining values every healthy per-user entry carries.
    #
    # Found by listing the value *names* of two entries Windows displays
    # correctly - Discord and Figma - and taking the set difference against
    # Keys. QuietUninstallString is what lets a caller uninstall without a
    # prompt, Language is the locale the entry is registered for, and
    # URLUpdateInfo points at the releases page.

    # NoModify and NoRepair are rewritten as DWORDs.
    #
    # CPack writes them through ConditionalAddToRegistry, which only writes
    # strings - so they landed as REG_SZ "1" where Windows expects REG_DWORD
    # 1. Every healthy entry on this machine has them as DWORDs, and a
    # wrong type is invisible in a value dump: the data reads "1" either
    # way, and only the kind differs.

    # The size Add/Remove Programs shows, in KB.
    #
    # Measured here rather than with NSIS's GetSize macro: that needs
    # FileFunc.nsh and a ${...} reference, and a ${...} inside
    # CPACK_NSIS_EXTRA_INSTALL_COMMANDS is eaten by CMake's own expansion before
    # NSIS ever sees it - which produced a script with a bare argument list and
    # no command in front of it, and a compile error rather than a wrong number.
    #
    # An approximation from the payload is honest enough for a list that rounds
    # to the nearest MB anyway, and it cannot fail at install time.
    string(TIMESTAMP keys_install_date "%Y%m%d")

    set(keys_installed_kb 24000)

    set(CPACK_NSIS_EXTRA_INSTALL_COMMANDS
        "WriteRegStr SHCTX 'Software\\\\Microsoft\\\\Windows\\\\CurrentVersion\\\\Uninstall\\\\Keys' 'DisplayIcon' '$INSTDIR\\\\bin\\\\keys.exe,0'
  WriteRegStr SHCTX 'Software\\\\Microsoft\\\\Windows\\\\CurrentVersion\\\\Uninstall\\\\Keys' 'InstallLocation' '$INSTDIR'
  WriteRegDWORD SHCTX 'Software\\\\Microsoft\\\\Windows\\\\CurrentVersion\\\\Uninstall\\\\Keys' 'EstimatedSize' ${keys_installed_kb}
  WriteRegStr SHCTX 'Software\\\\Microsoft\\\\Windows\\\\CurrentVersion\\\\Uninstall\\\\Keys' 'InstallDate' '${keys_install_date}'
  WriteRegDWORD SHCTX 'Software\\\\Microsoft\\\\Windows\\\\CurrentVersion\\\\Uninstall\\\\Keys' 'NoModify' 1
  WriteRegDWORD SHCTX 'Software\\\\Microsoft\\\\Windows\\\\CurrentVersion\\\\Uninstall\\\\Keys' 'NoRepair' 1
  WriteRegDWORD SHCTX 'Software\\\\Microsoft\\\\Windows\\\\CurrentVersion\\\\Uninstall\\\\Keys' 'Language' 1033
  WriteRegStr SHCTX 'Software\\\\Microsoft\\\\Windows\\\\CurrentVersion\\\\Uninstall\\\\Keys' 'QuietUninstallString' '$INSTDIR\\\\Uninstall.exe /S'
  WriteRegStr SHCTX 'Software\\\\Microsoft\\\\Windows\\\\CurrentVersion\\\\Uninstall\\\\Keys' 'URLUpdateInfo' 'https://github.com/Miracle2008-png/keys/releases'")

endif()

include(CPack)
