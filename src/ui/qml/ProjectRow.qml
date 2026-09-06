import QtQuick
import QtQuick.Controls
import Keys.Ui

/// One project in the start screen's list.
///
/// **Its own file.** The row carries an avatar, two lines of text, a timestamp
/// and two actions, and every one of those needs its own hit area. Inline in
/// the list it was four hundred lines of nesting where a click meant for the
/// pin opened the project instead.
///
/// **The avatar is not decoration.** A list of paths is hard to scan; a
/// coloured square with the project's initials is findable at a glance, which
/// is why JetBrains uses one. The colour is derived from the name, so a project
/// keeps the same square every time - it is recognition, not randomness.
Rectangle {
    id: root

    /// One entry from App.recentProjects.
    required property var entry

    readonly property bool available: App.pathExists(entry.path)

    /// Any of this row's own areas holding the pointer. The actions count, so
    /// moving onto one does not make the row think the pointer left.
    readonly property bool hovered: rowMouse.containsMouse
                                    || pinMouse.containsMouse
                                    || forgetMouse.containsMouse

    /// Two letters from the project name, and a hue derived from it. Same name,
    /// same square, every launch.
    readonly property string initials: {
        const name = entry.name;
        if (name.length === 0) {
            return "?";
        }
        const parts = name.split(/[-_. ]+/).filter(p => p.length > 0);
        if (parts.length >= 2) {
            return (parts[0][0] + parts[1][0]).toUpperCase();
        }
        return name.substring(0, 2).toUpperCase();
    }

    readonly property color avatarColor: {
        let hash = 0;
        for (let i = 0; i < entry.name.length; ++i) {
            hash = (hash * 31 + entry.name.charCodeAt(i)) % 360;
        }
        // Held to the palette's own lightness and chroma so a row never shouts
        // louder than the interface around it.
        return Qt.hsla(hash / 360, 0.32, 0.42, 1.0);
    }

    height: 56
    radius: Metrics.radiusMedium
    color: hovered ? Theme.bgHover : "transparent"

    Behavior on color {
        ColorAnimation { duration: App.fastAnimationDuration }
    }

    // ---- Avatar ------------------------------------------------------------

    Rectangle {
        id: avatar

        anchors.left: parent.left
        anchors.leftMargin: 10
        anchors.verticalCenter: parent.verticalCenter
        width: 36
        height: 36
        radius: Metrics.radiusSmall
        color: root.entry.pinned ? Theme.accent : root.avatarColor
        opacity: root.available ? 1.0 : 0.45

        Behavior on color {
            ColorAnimation { duration: App.fastAnimationDuration }
        }

        Text {
            anchors.centerIn: parent
            text: root.initials
            color: "#ffffff"
            font.family: Fonts.ui
            font.pointSize: Metrics.fontSizeSmall
            font.weight: Font.DemiBold
        }
    }

    // ---- Name and path -----------------------------------------------------

    Column {
        anchors.left: avatar.right
        anchors.leftMargin: 12
        anchors.right: actions.left
        anchors.rightMargin: 12
        anchors.verticalCenter: parent.verticalCenter
        spacing: 2

        Text {
            width: parent.width
            text: root.entry.name
            color: root.available ? Theme.textPrimary : Theme.textTertiary
            font.family: Fonts.ui
            font.pointSize: Metrics.fontSizeBody
            font.weight: Font.Medium
            elide: Text.ElideRight
        }

        Text {
            width: parent.width
            // The path is what tells two projects with the same name apart, so
            // it is shown rather than hidden behind a tooltip.
            text: root.available ? root.entry.path
                                 : qsTr("%1 — not found").arg(root.entry.path)
            color: root.available ? Theme.textTertiary : Theme.red
            font.family: Fonts.mono
            font.pointSize: Metrics.fontSizeLabel
            elide: Text.ElideMiddle
        }
    }

    // ---- Timestamp and actions --------------------------------------------

    Item {
        id: actions

        anchors.right: parent.right
        anchors.rightMargin: 10
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: 132

        Text {
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            text: root.entry.when
            color: Theme.textTertiary
            font.family: Fonts.ui
            font.pointSize: Metrics.fontSizeLabel
            // Gives way to the buttons rather than the row changing width.
            opacity: root.hovered ? 0 : 1

            Behavior on opacity {
                NumberAnimation {
                    duration: App.fastAnimationDuration
                    easing.type: Easing.OutQuad
                }
            }
        }

        // Faded rather than hidden: a hidden item takes no pointer events, so
        // the strip it occupied became a gap where the row read as unhovered -
        // and the buttons vanished exactly as the cursor reached them.
        Item {
            id: forgetAction

            anchors.right: parent.right
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: 28
            opacity: root.hovered ? 1 : 0

            Behavior on opacity {
                NumberAnimation {
                    duration: App.fastAnimationDuration
                    easing.type: Easing.OutQuad
                }
            }

            Rectangle {
                anchors.centerIn: parent
                width: 24
                height: 24
                radius: Metrics.radiusSmall
                color: forgetMouse.containsMouse ? Theme.bgElevated : "transparent"
            }

            Text {
                anchors.centerIn: parent
                text: "×"
                color: forgetMouse.containsMouse ? Theme.textPrimary : Theme.textTertiary
                font.family: Fonts.ui
                font.pointSize: Metrics.fontSizeMedium
            }

            MouseArea {
                id: forgetMouse

                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: App.forgetProject(root.entry.path)
            }

            ToolTip.visible: forgetMouse.containsMouse
            ToolTip.text: qsTr("Remove from list")
            ToolTip.delay: 500
        }

        Item {
            id: pinAction

            anchors.right: forgetAction.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: 28
            // A pin stays lit once set: it is state the user chose, not an
            // offer the row is making.
            opacity: root.hovered || root.entry.pinned ? 1 : 0

            Behavior on opacity {
                NumberAnimation {
                    duration: App.fastAnimationDuration
                    easing.type: Easing.OutQuad
                }
            }

            Rectangle {
                anchors.centerIn: parent
                width: 24
                height: 24
                radius: Metrics.radiusSmall
                color: pinMouse.containsMouse ? Theme.bgElevated : "transparent"
            }

            Text {
                anchors.centerIn: parent
                text: root.entry.pinned ? "★" : "☆"
                color: root.entry.pinned ? Theme.accent
                     : pinMouse.containsMouse ? Theme.textPrimary
                     : Theme.textTertiary
                font.family: Fonts.ui
                font.pointSize: Metrics.fontSizeBody
            }

            MouseArea {
                id: pinMouse

                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: App.setProjectPinned(root.entry.path, !root.entry.pinned)
            }

            ToolTip.visible: pinMouse.containsMouse
            ToolTip.text: root.entry.pinned ? qsTr("Unpin") : qsTr("Pin to top")
            ToolTip.delay: 500
        }
    }

    // Opens the project. Stops where the actions begin - a MouseArea filling
    // the row sits under them and takes every press meant for a button.
    MouseArea {
        id: rowMouse

        anchors.left: parent.left
        anchors.right: actions.left
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        hoverEnabled: true
        enabled: root.available
        cursorShape: root.available ? Qt.PointingHandCursor : Qt.ArrowCursor
        onClicked: App.openProject(root.entry.path)
    }
}
