import QtQuick
import QtQuick.Controls
import Keys.Ui

/// The start screen, built on CLion's layout.
///
/// **A named left rail, a titled pane.** The rail lists what the page can show
/// and the pane fills with it. This is the shape every JetBrains IDE uses and
/// it is the right one: the page has somewhere to grow as sections are added,
/// and the user always knows which of them they are looking at.
///
/// **The list is the page.** Everything else - the title, the actions, the
/// search - sits in one band above it. A start screen exists to get someone
/// into a project, so the projects occupy the space and nothing competes.
///
/// **Rows carry what tells projects apart.** A coloured square with the
/// project's initials, its name, its full path, and when it was last opened.
/// Two folders called `client` are indistinguishable by name alone, and the
/// path is what settles it.
Item {
    id: root

    /// Below this the rail and the list would each be too narrow, so the rail
    /// collapses and the list takes the width.
    readonly property bool wide: width > 720

    property string section: "projects"

    /// The recent list, filtered by the search box. Held as a property so the
    /// empty state can tell "nothing yet" from "nothing matches".
    readonly property var filtered: {
        const all = App.recentProjects;
        const needle = searchInput.text.trim().toLowerCase();
        if (needle.length === 0) {
            return all;
        }
        const result = [];
        for (const entry of all) {
            if (entry.name.toLowerCase().includes(needle)
                || entry.path.toLowerCase().includes(needle)) {
                result.push(entry);
            }
        }
        return result;
    }

    signal newProjectRequested()

    Rectangle {
        anchors.fill: parent
        color: Theme.bgEditor
    }

    // ---- The rail ----------------------------------------------------------

    Rectangle {
        id: rail

        width: root.wide ? 200 : 0
        height: parent.height
        visible: root.wide
        color: Theme.bgSurface

        Rectangle {
            anchors.right: parent.right
            width: 1
            height: parent.height
            color: Theme.border
        }

        Column {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            anchors.topMargin: 28
            anchors.leftMargin: 16
            anchors.rightMargin: 12
            spacing: 22

            Row {
                spacing: 10

                // The mark itself, not a letter standing in for it. The
                // rendered PNG rather than the SVG so it matches the taskbar
                // and the installer exactly.
                Image {
                    width: 26
                    height: 26
                    anchors.verticalCenter: parent.verticalCenter
                    source: "qrc:/branding/generated/keys-256.png"
                    sourceSize.width: 52
                    sourceSize.height: 52
                    smooth: true
                }

                Text {
                    text: qsTr("Keys")
                    color: Theme.textPrimary
                    anchors.verticalCenter: parent.verticalCenter
                    font.family: Fonts.ui
                    font.pointSize: Metrics.fontSizeMedium
                    font.weight: Font.DemiBold
                }
            }

            Column {
                width: parent.width
                spacing: 1

                Repeater {
                    model: [
                        { id: "projects",  label: qsTr("Projects") },
                        { id: "shortcuts", label: qsTr("Shortcuts") },
                        { id: "about",     label: qsTr("About") }
                    ]

                    delegate: Item {
                        required property var modelData

                        width: parent.width
                        height: 30

                        readonly property bool current: root.section === modelData.id

                        Rectangle {
                            anchors.fill: parent
                            radius: Metrics.radiusSmall
                            color: parent.current ? Theme.selection
                                 : navMouse.containsMouse ? Theme.bgHover
                                 : "transparent"

                            Behavior on color {
                                ColorAnimation { duration: App.fastAnimationDuration }
                            }
                        }

                        Text {
                            anchors.left: parent.left
                            anchors.leftMargin: 10
                            anchors.verticalCenter: parent.verticalCenter
                            text: parent.modelData.label
                            color: parent.current ? Theme.textPrimary
                                 : navMouse.containsMouse ? Theme.textSecondary
                                 : Theme.textTertiary
                            font.family: Fonts.ui
                            font.pointSize: Metrics.fontSizeBody

                            Behavior on color {
                                ColorAnimation { duration: App.fastAnimationDuration }
                            }
                        }

                        MouseArea {
                            id: navMouse

                            anchors.fill: parent
                            hoverEnabled: true
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.section = parent.modelData.id
                        }
                    }
                }
            }
        }

        // Theme toggle, pinned low where a setting belongs.
        Row {
            anchors.left: parent.left
            anchors.leftMargin: 16
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 18
            spacing: 8

            Rectangle {
                width: 26
                height: 26
                radius: Metrics.radiusSmall
                color: themeMouse.containsMouse ? Theme.bgHover : "transparent"

                Behavior on color {
                    ColorAnimation { duration: App.fastAnimationDuration }
                }

                Icon {
                    anchors.centerIn: parent
                    source: Theme.mode === Theme.Dark ? Icons.sun : Icons.moon
                    size: 15
                    color: themeMouse.containsMouse ? Theme.textSecondary
                                                    : Theme.textTertiary
                }

                MouseArea {
                    id: themeMouse

                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: Theme.toggleMode()
                }
            }

            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("Theme")
                color: Theme.textTertiary
                font.family: Fonts.ui
                font.pointSize: Metrics.fontSizeSmall
            }
        }
    }

    // ---- The pane ----------------------------------------------------------

    Item {
        id: pane

        anchors.left: rail.right
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        anchors.leftMargin: 32
        anchors.rightMargin: 32
        anchors.topMargin: 28
        anchors.bottomMargin: 20

        // ---- Projects ------------------------------------------------------

        Item {
            anchors.fill: parent
            visible: root.section === "projects"

            Text {
                id: title

                anchors.left: parent.left
                anchors.top: parent.top
                text: qsTr("Welcome to Keys")
                color: Theme.textPrimary
                font.family: Fonts.ui
                font.pointSize: Metrics.fontSizeTitle
                font.weight: Font.DemiBold
                font.letterSpacing: -0.4
            }

            // Search on the left, actions on the right - the band CLion puts
            // above its project list.
            Item {
                id: controls

                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: title.bottom
                anchors.topMargin: 22
                height: 30

                Rectangle {
                    id: searchBox

                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    width: Math.min(260, parent.width * 0.4)
                    height: 30
                    radius: Metrics.radiusSmall
                    color: Theme.bgElevated
                    border.width: 1
                    border.color: searchInput.activeFocus ? Theme.focusRing : Theme.border

                    Behavior on border.color {
                        ColorAnimation { duration: App.fastAnimationDuration }
                    }

                    Icon {
                        id: searchIcon

                        anchors.left: parent.left
                        anchors.leftMargin: 9
                        anchors.verticalCenter: parent.verticalCenter
                        source: Icons.search
                        size: 14
                        color: Theme.textTertiary
                    }

                    TextInput {
                        id: searchInput

                        anchors.left: searchIcon.right
                        anchors.leftMargin: 8
                        anchors.right: parent.right
                        anchors.rightMargin: 9
                        anchors.verticalCenter: parent.verticalCenter
                        verticalAlignment: TextInput.AlignVCenter
                        color: Theme.textPrimary
                        font.family: Fonts.ui
                        font.pointSize: Metrics.fontSizeBody
                        clip: true
                        selectByMouse: true
                        selectionColor: Theme.selection
                        selectedTextColor: Theme.textPrimary

                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: qsTr("Search projects")
                            color: Theme.textTertiary
                            font: parent.font
                            visible: parent.text.length === 0
                        }

                        Keys.onEscapePressed: searchInput.text = ""
                    }
                }

                Row {
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 8

                    WelcomeButton {
                        text: qsTr("New Folder")
                        onClicked: root.newProjectRequested()
                    }

                    WelcomeButton {
                        text: qsTr("Open")
                        primary: true
                        onClicked: root.browseForProject()
                    }
                }
            }

            // ---- The list ---------------------------------------------------

            ListView {
                id: projectList

                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: controls.bottom
                anchors.topMargin: 20
                anchors.bottom: parent.bottom
                clip: true
                spacing: 2
                model: root.filtered
                boundsBehavior: Flickable.StopAtBounds
                visible: root.filtered.length > 0

                ScrollBar.vertical: ScrollBar {
                    readonly property bool needed:
                        projectList.contentHeight > projectList.height

                    policy: needed ? ScrollBar.AsNeeded : ScrollBar.AlwaysOff
                    interactive: needed
                    visible: needed
                    width: needed ? implicitWidth : 0
                }

                delegate: ProjectRow {
                    // `modelData` has to be declared required for the delegate
                    // to see it at all; without this it is undefined and every
                    // field in the row reads off nothing.
                    required property var modelData

                    width: projectList.width
                    entry: modelData
                }
            }

            // ---- Empty states ------------------------------------------------

            Column {
                anchors.centerIn: parent
                width: Math.min(360, parent.width)
                spacing: 10
                visible: root.filtered.length === 0

                Text {
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    text: App.recentProjects.length === 0
                          ? qsTr("No projects yet")
                          : qsTr("Nothing matches “%1”").arg(searchInput.text.trim())
                    color: Theme.textSecondary
                    font.family: Fonts.ui
                    font.pointSize: Metrics.fontSizeMedium
                    font.weight: Font.Medium
                }

                Text {
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                    text: App.recentProjects.length === 0
                          ? qsTr("Open a folder to start. Keys will remember it here.")
                          : qsTr("Try a different name, or clear the search.")
                    color: Theme.textTertiary
                    font.family: Fonts.ui
                    font.pointSize: Metrics.fontSizeBody
                }

                Item { width: 1; height: 6 }

                WelcomeButton {
                    anchors.horizontalCenter: parent.horizontalCenter
                    text: App.recentProjects.length === 0 ? qsTr("Open Project")
                                                          : qsTr("Clear search")
                    primary: App.recentProjects.length === 0
                    onClicked: {
                        if (App.recentProjects.length === 0) {
                            root.browseForProject();
                        } else {
                            searchInput.text = "";
                        }
                    }
                }
            }
        }

        // ---- Shortcuts ------------------------------------------------------

        Item {
            anchors.fill: parent
            visible: root.section === "shortcuts"

            Text {
                id: shortcutsTitle

                anchors.left: parent.left
                anchors.top: parent.top
                text: qsTr("Shortcuts")
                color: Theme.textPrimary
                font.family: Fonts.ui
                font.pointSize: Metrics.fontSizeTitle
                font.weight: Font.DemiBold
                font.letterSpacing: -0.4
            }

            Column {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: shortcutsTitle.bottom
                anchors.topMargin: 22
                spacing: 1

                Repeater {
                    // Only bindings that work. A shortcut list is a promise,
                    // and naming a key that does nothing is the fastest way to
                    // look unfinished.
                    model: [
                        { keys: "Ctrl+O",       label: qsTr("Open project") },
                        { keys: "Ctrl+N",       label: qsTr("New file") },
                        { keys: "Ctrl+S",       label: qsTr("Save") },
                        { keys: "Ctrl+F",       label: qsTr("Find in file") },
                        { keys: "Ctrl+H",       label: qsTr("Replace") },
                        { keys: "Ctrl+P",       label: qsTr("Go to file") },
                        { keys: "Ctrl+G",       label: qsTr("Go to line") },
                        { keys: "Ctrl+K",       label: qsTr("Command palette") },
                        { keys: "Ctrl+B",       label: qsTr("Toggle sidebar") },
                        { keys: "Ctrl+Alt+↑↓",  label: qsTr("Add caret") },
                        { keys: "Shift+F6",     label: qsTr("Rename symbol") },
                        { keys: "Ctrl+,",       label: qsTr("Settings") }
                    ]

                    delegate: Item {
                        required property var modelData

                        width: parent.width
                        height: 32

                        Rectangle {
                            anchors.fill: parent
                            radius: Metrics.radiusSmall
                            color: keyMouse.containsMouse ? Theme.bgHover : "transparent"

                            Behavior on color {
                                ColorAnimation { duration: App.fastAnimationDuration }
                            }
                        }

                        Rectangle {
                            id: cap

                            anchors.left: parent.left
                            anchors.leftMargin: 8
                            anchors.verticalCenter: parent.verticalCenter
                            width: 116
                            height: 22
                            radius: Metrics.radiusSmall
                            color: Theme.bgElevated
                            border.width: 1
                            border.color: Theme.border

                            Text {
                                anchors.centerIn: parent
                                text: parent.parent.modelData.keys
                                color: Theme.textSecondary
                                font.family: Fonts.mono
                                font.pointSize: Metrics.fontSizeLabel
                            }
                        }

                        Text {
                            anchors.left: cap.right
                            anchors.leftMargin: 14
                            anchors.verticalCenter: parent.verticalCenter
                            text: parent.modelData.label
                            color: Theme.textSecondary
                            font.family: Fonts.ui
                            font.pointSize: Metrics.fontSizeBody
                        }

                        MouseArea {
                            id: keyMouse
                            anchors.fill: parent
                            hoverEnabled: true
                        }
                    }
                }
            }
        }

        // ---- About -----------------------------------------------------------

        Item {
            anchors.fill: parent
            visible: root.section === "about"

            Text {
                id: aboutTitle

                anchors.left: parent.left
                anchors.top: parent.top
                text: qsTr("About")
                color: Theme.textPrimary
                font.family: Fonts.ui
                font.pointSize: Metrics.fontSizeTitle
                font.weight: Font.DemiBold
                font.letterSpacing: -0.4
            }

            Column {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: aboutTitle.bottom
                anchors.topMargin: 22
                spacing: 8

                Text {
                    text: qsTr("Keys %1").arg(App.version)
                    color: Theme.textPrimary
                    font.family: Fonts.ui
                    font.pointSize: Metrics.fontSizeMedium
                    font.weight: Font.Medium
                }

                Text {
                    width: Math.min(parent.width, 460)
                    wrapMode: Text.WordWrap
                    text: qsTr("A fast, focused editor for people who build "
                               + "software for a living.")
                    color: Theme.textSecondary
                    font.family: Fonts.ui
                    font.pointSize: Metrics.fontSizeBody
                }

                Text {
                    text: qsTr("Built with Qt %1").arg(App.qtVersion)
                    color: Theme.textTertiary
                    font.family: Fonts.mono
                    font.pointSize: Metrics.fontSizeSmall
                }
            }
        }
    }

    FolderPicker {
        id: folderDialog
        onFolderAccepted: (path) => App.openProject(path)
    }

    function browseForProject() {
        folderDialog.open();
    }
}
