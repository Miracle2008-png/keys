import QtQuick
import QtQuick.Controls
import Keys.Ui

/// Shown in the editor area when no project is open.
///
/// **A workbench, not a splash screen.** The first thing this has to do is get
/// the user into a project, so the project list is the page rather than a
/// footnote beside a logo. It is searchable, because a list worth keeping
/// twenty entries in is one worth filtering, and each row carries the path and
/// when it was last opened - the two things that tell two similarly-named
/// folders apart.
///
/// **Sections, not tabs.** The left rail names what the page can show. Only
/// sections that do something are listed: an empty "Plugins" pane would teach
/// the user the application is unfinished, which is worse than not offering it.
///
/// **Colour carries meaning.** The accent marks what is actionable and what is
/// pinned. Everything else is graphite, so the eye goes to the row it can act
/// on rather than to decoration.
Item {
    id: root

    /// Below this the rail and the list would each be too narrow to read, so
    /// the rail collapses and the list takes the width.
    readonly property bool wide: width > 780

    /// Which section the rail has selected.
    property string section: "projects"

    /// The recent list, filtered by the search box. Held as a property so the
    /// empty state can tell "nothing opened yet" from "nothing matches".
    readonly property var filtered: {
        const all = App.recentProjects;
        const needle = searchField.text.trim().toLowerCase();
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

    Rectangle {
        anchors.fill: parent
        color: Theme.bgEditor
    }

    Row {
        anchors.fill: parent

        // ---- The rail ------------------------------------------------------

        Item {
            width: root.wide ? 208 : 0
            height: parent.height
            visible: root.wide

            Rectangle {
                anchors.fill: parent
                color: Theme.bgSurface

                Rectangle {
                    anchors.right: parent.right
                    width: 1
                    height: parent.height
                    color: Theme.border
                }
            }

            Column {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                anchors.topMargin: 34
                anchors.leftMargin: Metrics.spacingLarge
                anchors.rightMargin: Metrics.spacingMedium
                spacing: 26

                // The mark and the name, small. This is a workbench the user
                // passes through, not a product page to linger on.
                Row {
                    spacing: Metrics.spacingSmall

                    Rectangle {
                        width: 22
                        height: 22
                        radius: Metrics.radiusSmall
                        color: Theme.accent
                        anchors.verticalCenter: parent.verticalCenter

                        Text {
                            anchors.centerIn: parent
                            text: "K"
                            color: "#ffffff"
                            font.family: Fonts.ui
                            font.pointSize: Metrics.fontSizeSmall
                            font.weight: Font.Bold
                        }
                    }

                    Text {
                        text: qsTr("Keys")
                        color: Theme.textPrimary
                        anchors.verticalCenter: parent.verticalCenter
                        font.family: Fonts.ui
                        font.pointSize: Metrics.fontSizeLarge
                        font.weight: Font.DemiBold
                    }
                }

                Column {
                    width: parent.width
                    spacing: 2

                    Repeater {
                        model: [
                            { id: "projects",  label: qsTr("Projects") },
                            { id: "shortcuts", label: qsTr("Shortcuts") },
                            { id: "about",     label: qsTr("About") },
                        ]

                        delegate: Rectangle {
                            required property var modelData

                            width: parent.width
                            height: 30
                            radius: Metrics.radiusSmall
                            color: root.section === modelData.id ? Theme.accentSoft
                                 : sectionHover.hovered ? Theme.bgHover
                                 : "transparent"

                            HoverHandler {
                                id: sectionHover
                                cursorShape: Qt.PointingHandCursor
                            }

                            Text {
                                anchors.left: parent.left
                                anchors.leftMargin: Metrics.spacingSmall
                                anchors.verticalCenter: parent.verticalCenter
                                text: parent.modelData.label
                                color: root.section === parent.modelData.id
                                       ? Theme.textPrimary : Theme.textSecondary
                                font.family: Fonts.ui
                                font.pointSize: Metrics.fontSizeBody
                            }

                            TapHandler {
                                onTapped: root.section = parent.modelData.id
                            }
                        }
                    }
                }
            }

            // The theme switch sits at the bottom of the rail, where a setting
            // belongs - reachable, but not competing with the project list.
            Row {
                anchors.left: parent.left
                anchors.bottom: parent.bottom
                anchors.leftMargin: Metrics.spacingLarge
                anchors.bottomMargin: Metrics.spacingLarge
                spacing: Metrics.spacingSmall

                Rectangle {
                    width: 26
                    height: 26
                    radius: Metrics.radiusSmall
                    color: themeHover.hovered ? Theme.bgHover : "transparent"

                    HoverHandler {
                        id: themeHover
                        cursorShape: Qt.PointingHandCursor
                    }

                    Text {
                        anchors.centerIn: parent
                        text: "◑"
                        color: Theme.textSecondary
                        font.pointSize: Metrics.fontSizeBody
                    }

                    TapHandler {
                        onTapped: Theme.toggleMode()
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

        // ---- The pane ------------------------------------------------------

        Item {
            width: parent.width - (root.wide ? 208 : 0)
            height: parent.height

            // ---- Projects --------------------------------------------------

            Item {
                anchors.fill: parent
                anchors.topMargin: 34
                anchors.leftMargin: 34
                anchors.rightMargin: 34
                anchors.bottomMargin: Metrics.spacingLarge
                visible: root.section === "projects"

                Column {
                    id: projectsHeader

                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    spacing: Metrics.spacingLarge

                    Row {
                        width: parent.width
                        spacing: Metrics.spacingMedium

                        Text {
                            text: qsTr("Projects")
                            color: Theme.textPrimary
                            anchors.verticalCenter: parent.verticalCenter
                            font.family: Fonts.ui
                            font.pointSize: Metrics.fontSizeTitle
                            font.weight: Font.DemiBold
                            font.letterSpacing: -0.3
                        }

                        Item {
                            width: parent.width - openButton.width
                                   - newButton.width - searchBox.width
                                   - Metrics.spacingMedium * 3
                                   - parent.children[0].implicitWidth
                            height: 1
                        }

                        // Search sits with the actions rather than above the
                        // list: filtering and opening are the same task.
                        Rectangle {
                            id: searchBox

                            width: 220
                            height: 32
                            radius: Metrics.radiusSmall
                            color: Theme.bgSurface
                            border.width: 1
                            border.color: searchField.activeFocus ? Theme.accent
                                                                  : Theme.border
                            anchors.verticalCenter: parent.verticalCenter

                            TextInput {
                                id: searchField

                                anchors.fill: parent
                                anchors.leftMargin: Metrics.spacingSmall
                                anchors.rightMargin: Metrics.spacingSmall
                                verticalAlignment: TextInput.AlignVCenter
                                color: Theme.textPrimary
                                font.family: Fonts.ui
                                font.pointSize: Metrics.fontSizeBody
                                clip: true
                                selectByMouse: true
                                selectionColor: Theme.accentSoft
                                selectedTextColor: Theme.textPrimary

                                Text {
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: qsTr("Search projects")
                                    color: Theme.textTertiary
                                    font: parent.font
                                    visible: parent.text.length === 0
                                }

                                Keys.onEscapePressed: searchField.text = ""
                            }
                        }

                        WelcomeButton {
                            id: newButton
                            text: qsTr("New Folder…")
                            anchors.verticalCenter: parent.verticalCenter
                            onClicked: root.newProjectRequested()
                        }

                        WelcomeButton {
                            id: openButton
                            text: qsTr("Open")
                            primary: true
                            anchors.verticalCenter: parent.verticalCenter
                            onClicked: root.browseForProject()
                        }
                    }
                }

                // ---- The list ----------------------------------------------

                ListView {
                    id: projectList

                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: projectsHeader.bottom
                    anchors.topMargin: Metrics.spacingLarge
                    anchors.bottom: parent.bottom
                    clip: true
                    spacing: 2
                    model: root.filtered
                    boundsBehavior: Flickable.StopAtBounds
                    visible: root.filtered.length > 0

                    // Only interactive when there is something to scroll. A
                    // ScrollBar spans the full height of the view whether or
                    // not the content overflows, and an idle one still takes
                    // presses - which left a dead strip down the right edge
                    // where the pin and remove buttons live, so they could be
                    // seen and hovered but never clicked.
                    // Only present when it has something to scroll. An
                    // attached ScrollBar holds the right edge of the view even
                    // with its policy set to AlwaysOff, and takes the presses
                    // that land there - which is where a row keeps its own
                    // actions, so they drew but could not be clicked.
                    ScrollBar.vertical: ScrollBar {
                        readonly property bool needed:
                            projectList.contentHeight > projectList.height

                        policy: needed ? ScrollBar.AsNeeded : ScrollBar.AlwaysOff
                        interactive: needed
                        visible: needed
                        width: needed ? implicitWidth : 0
                    }

                    // Built the way EditorTabBar's tabs are, which is the one
                    // shape in this codebase already proven to carry a row-wide
                    // click *and* a small button that must not trigger it:
                    //
                    //  - hover comes from the MouseAreas themselves, never from
                    //    a HoverHandler. A handler yields the pointer to any
                    //    child MouseArea, so the row reads as unhovered the
                    //    moment the cursor reaches a button - which hid the
                    //    button being reached for.
                    //  - each action is a MouseArea inside its own positioned
                    //    Item, declared before the row-wide area. Nested items
                    //    take the press first; a sibling Row of buttons does
                    //    not, whatever its z.
                    delegate: Item {
                        id: projectRow

                        required property var modelData
                        required property int index

                        readonly property bool available:
                            App.pathExists(projectRow.modelData.path)

                        /// Width reserved on the right for the row's actions.
                        /// Every element that has to stay clear of them is
                        /// measured from this one number.
                        readonly property int actionStrip: 112

                        /// Any of the row's own areas holding the pointer. The
                        /// actions count, so moving onto one keeps them shown.
                        readonly property bool hovered: rowMouse.containsMouse
                                                        || pinMouse.containsMouse

                        width: projectList.width
                        height: 58

                        // The background is a child rather than the delegate
                        // itself, matching EditorTabBar. A Rectangle root with
                        // a radius clips what sits inside it, which is what
                        // kept the pin and remove buttons from receiving a
                        // press while still drawing them.
                        Rectangle {
                            anchors.fill: parent
                            radius: Metrics.radiusMedium
                            color: projectRow.hovered ? Theme.bgHover : "transparent"
                        }

                        // The project's initial, in the accent when pinned. A
                        // list of paths is hard to scan; a letter is not.
                        Rectangle {
                            id: badge

                            anchors.left: parent.left
                            anchors.leftMargin: Metrics.spacingMedium
                            anchors.verticalCenter: parent.verticalCenter
                            width: 34
                            height: 34
                            radius: Metrics.radiusSmall
                            color: projectRow.modelData.pinned ? Theme.accent
                                                               : Theme.bgElevated
                            border.width: projectRow.modelData.pinned ? 0 : 1
                            border.color: Theme.border
                            opacity: projectRow.available ? 1.0 : 0.5

                            Text {
                                anchors.centerIn: parent
                                text: projectRow.modelData.name.length > 0
                                      ? projectRow.modelData.name.charAt(0).toUpperCase()
                                      : "?"
                                color: projectRow.modelData.pinned ? "#ffffff"
                                                                   : Theme.textSecondary
                                font.family: Fonts.ui
                                font.pointSize: Metrics.fontSizeMedium
                                font.weight: Font.DemiBold
                            }
                        }

                        Column {
                            anchors.left: badge.right
                            anchors.leftMargin: Metrics.spacingMedium
                            anchors.right: parent.right
                            anchors.rightMargin: projectRow.actionStrip + 12
                            anchors.verticalCenter: parent.verticalCenter
                            spacing: 3

                            Text {
                                width: parent.width
                                text: projectRow.modelData.name
                                color: projectRow.available ? Theme.textPrimary
                                                            : Theme.textTertiary
                                font.family: Fonts.ui
                                font.pointSize: Metrics.fontSizeBody
                                font.weight: Font.Medium
                                elide: Text.ElideRight
                            }

                            Text {
                                width: parent.width
                                // The path is what tells two folders with the
                                // same name apart, so it is shown rather than
                                // hidden behind a tooltip.
                                text: projectRow.available
                                      ? projectRow.modelData.path
                                      : qsTr("%1 — not found")
                                            .arg(projectRow.modelData.path)
                                color: projectRow.available ? Theme.textTertiary
                                                            : Theme.red
                                font.family: Fonts.mono
                                font.pointSize: Metrics.fontSizeLabel
                                elide: Text.ElideMiddle
                            }
                        }

                        // When the row was last opened. Gives way to the
                        // actions on hover, so the row never changes width.
                        Text {
                            anchors.right: parent.right
                            anchors.rightMargin: Metrics.spacingMedium
                            anchors.verticalCenter: parent.verticalCenter
                            text: projectRow.modelData.when
                            color: Theme.textTertiary
                            font.family: Fonts.ui
                            font.pointSize: Metrics.fontSizeLabel
                            visible: !projectRow.hovered
                        }

                        // ---- The actions ----
                        //
                        // Each is its own anchored Item holding a MouseArea,
                        // declared before the row-wide area below. A negative
                        // margin widens the press target past the glyph: these
                        // are small, and a click that lands a pixel outside
                        // should still hit.

                        Item {
                            id: pinAction

                            anchors.right: parent.right
                            anchors.rightMargin: projectRow.actionStrip - 44
                            anchors.top: parent.top
                            anchors.bottom: parent.bottom
                            width: 24

                            // A pin stays lit once set: it is state the user
                            // chose, not an offer the row is making.
                            opacity: projectRow.hovered
                                     || projectRow.modelData.pinned ? 1 : 0

                            Rectangle {
                                anchors.centerIn: parent
                                width: 24
                                height: 24
                                radius: Metrics.radiusSmall
                                color: pinMouse.containsMouse ? Theme.bgElevated
                                                              : "transparent"
                            }

                            Text {
                                anchors.centerIn: parent
                                text: projectRow.modelData.pinned ? "★" : "☆"
                                color: pinMouse.containsMouse
                                       ? Theme.textPrimary
                                       : (projectRow.modelData.pinned ? Theme.accent
                                                                      : Theme.textTertiary)
                                font.family: Fonts.ui
                                font.pointSize: Metrics.fontSizeBody
                            }

                            MouseArea {
                                id: pinMouse

                                anchors.fill: parent
                                hoverEnabled: true
                                cursorShape: Qt.PointingHandCursor
                                onClicked: App.setProjectPinned(
                                               projectRow.modelData.path,
                                               !projectRow.modelData.pinned)
                            }

                        }

                        // Opens the project. Declared last and filling the row,
                        // so it takes any press the actions above did not.
                        MouseArea {
                            id: rowMouse

                            // Stops short of the action strip. Filling the row
                            // put this on top of the pin and remove buttons -
                            // it is declared last so that it takes any press
                            // they do not, and a MouseArea that covers them
                            // takes every press instead.
                            anchors.left: parent.left
                            anchors.top: parent.top
                            anchors.bottom: parent.bottom
                            anchors.right: parent.right
                            anchors.rightMargin: projectRow.actionStrip
                            hoverEnabled: true
                            // A missing folder cannot be opened, and offering a
                            // click that fails is worse than a row that says so.
                            cursorShape: projectRow.available ? Qt.PointingHandCursor
                                                              : Qt.ArrowCursor
                            onClicked: {
                                if (projectRow.available) {
                                    App.openProject(projectRow.modelData.path);
                                }
                            }
                        }
                    }
                }

                // ---- Empty states ------------------------------------------
                //
                // Two of them, because "you have not opened anything yet" and
                // "nothing matches what you typed" call for different replies.

                Column {
                    anchors.centerIn: parent
                    width: Math.min(380, parent.width)
                    spacing: Metrics.spacingMedium
                    visible: root.filtered.length === 0

                    Text {
                        width: parent.width
                        horizontalAlignment: Text.AlignHCenter
                        text: App.recentProjects.length === 0
                              ? qsTr("No projects yet")
                              : qsTr("Nothing matches “%1”").arg(searchField.text.trim())
                        color: Theme.textSecondary
                        font.family: Fonts.ui
                        font.pointSize: Metrics.fontSizeLarge
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

                    Item {
                        width: parent.width
                        height: Metrics.spacingSmall
                    }

                    WelcomeButton {
                        anchors.horizontalCenter: parent.horizontalCenter
                        text: App.recentProjects.length === 0 ? qsTr("Open Project")
                                                              : qsTr("Clear search")
                        primary: App.recentProjects.length === 0
                        onClicked: {
                            if (App.recentProjects.length === 0) {
                                root.browseForProject();
                            } else {
                                searchField.text = "";
                            }
                        }
                    }
                }
            }

            // ---- Shortcuts -------------------------------------------------

            Item {
                anchors.fill: parent
                anchors.topMargin: 34
                anchors.leftMargin: 34
                anchors.rightMargin: 34
                visible: root.section === "shortcuts"

                Column {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    spacing: Metrics.spacingLarge

                    Text {
                        text: qsTr("Shortcuts")
                        color: Theme.textPrimary
                        font.family: Fonts.ui
                        font.pointSize: Metrics.fontSizeTitle
                        font.weight: Font.DemiBold
                        font.letterSpacing: -0.3
                    }

                    Column {
                        width: parent.width
                        spacing: 1

                        Repeater {
                            // Only bindings that work. A shortcut list is a
                            // promise, and one that names a key doing nothing
                            // is the fastest way to look unfinished.
                            model: [
                                { keys: "Ctrl+O",       label: qsTr("Open project") },
                                { keys: "Ctrl+N",       label: qsTr("New file") },
                                { keys: "Ctrl+S",       label: qsTr("Save") },
                                { keys: "Ctrl+Shift+S", label: qsTr("Save as") },
                                { keys: "Ctrl+P",       label: qsTr("Go to file") },
                                { keys: "Ctrl+K",       label: qsTr("Command palette") },
                                { keys: "Ctrl+B",       label: qsTr("Toggle sidebar") },
                                { keys: "Ctrl+,",       label: qsTr("Settings") },
                            ]

                            delegate: Item {
                                required property var modelData

                                width: parent.width
                                height: 34

                                Rectangle {
                                    anchors.fill: parent
                                    radius: Metrics.radiusSmall
                                    color: shortcutHover.hovered ? Theme.bgHover
                                                                 : "transparent"
                                }

                                HoverHandler {
                                    id: shortcutHover
                                }

                                Rectangle {
                                    id: keycap

                                    anchors.left: parent.left
                                    anchors.leftMargin: Metrics.spacingSmall
                                    anchors.verticalCenter: parent.verticalCenter
                                    width: 108
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
                                    anchors.left: keycap.right
                                    anchors.leftMargin: Metrics.spacingMedium
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: parent.modelData.label
                                    color: Theme.textSecondary
                                    font.family: Fonts.ui
                                    font.pointSize: Metrics.fontSizeBody
                                }
                            }
                        }
                    }
                }
            }

            // ---- About -----------------------------------------------------

            Item {
                anchors.fill: parent
                anchors.topMargin: 34
                anchors.leftMargin: 34
                anchors.rightMargin: 34
                visible: root.section === "about"

                Column {
                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    spacing: Metrics.spacingLarge

                    Text {
                        text: qsTr("About")
                        color: Theme.textPrimary
                        font.family: Fonts.ui
                        font.pointSize: Metrics.fontSizeTitle
                        font.weight: Font.DemiBold
                        font.letterSpacing: -0.3
                    }

                    Column {
                        width: parent.width
                        spacing: Metrics.spacingSmall

                        Text {
                            width: parent.width
                            text: qsTr("Keys %1").arg(App.version)
                            color: Theme.textPrimary
                            font.family: Fonts.ui
                            font.pointSize: Metrics.fontSizeLarge
                            font.weight: Font.Medium
                        }

                        Text {
                            width: parent.width
                            wrapMode: Text.WordWrap
                            text: qsTr("A fast, focused editor for people who "
                                       + "build software for a living.")
                            color: Theme.textSecondary
                            font.family: Fonts.ui
                            font.pointSize: Metrics.fontSizeBody
                        }

                        Text {
                            width: parent.width
                            text: qsTr("Built with Qt %1").arg(App.qtVersion)
                            color: Theme.textTertiary
                            font.family: Fonts.mono
                            font.pointSize: Metrics.fontSizeLabel
                        }
                    }
                }
            }
        }
    }

    // ---- Dialogs -----------------------------------------------------------

    FolderPicker {
        id: folderDialog
        onFolderAccepted: (path) => App.openProject(path)
    }

    /// Raised when the user wants somewhere new to work. The window owns the
    /// name dialog, so the request goes up rather than being answered here.
    signal newProjectRequested()

    /// Opened by the Open button, the empty state, and the File menu.
    function browseForProject() {
        folderDialog.open();
    }
}
