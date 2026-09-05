import QtQuick
import Keys.Ui

/// The 46px application bar: identity and project on the left, the search field
/// centred, run/focus/settings controls on the right.
Rectangle {
    id: root

    height: Metrics.topBarHeight
    color: Theme.bgChrome

    // Hairline separator. A Rectangle rather than a border so only the bottom
    // edge is drawn.
    Rectangle {
        anchors.bottom: parent.bottom
        width: parent.width
        height: 1
        color: Theme.border
    }

    Row {
        anchors.left: parent.left
        anchors.leftMargin: Metrics.spacingMedium
        anchors.verticalCenter: parent.verticalCenter
        spacing: Metrics.spacingMedium

        Row {
            anchors.verticalCenter: parent.verticalCenter
            spacing: Metrics.spacingSmall

            // The Keys mark: an accent square with the chrome color punched out.
            Rectangle {
                anchors.verticalCenter: parent.verticalCenter
                width: 18
                height: 18
                radius: 5
                color: Theme.accent

                Rectangle {
                    x: 5
                    y: 5
                    width: 8
                    height: 8
                    radius: 2
                    color: Theme.bgChrome
                }
            }

            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("Keys")
                color: Theme.textPrimary
                font.pointSize: Metrics.fontSizeMedium
                font.weight: Font.DemiBold
            }
        }

        Rectangle {
            anchors.verticalCenter: parent.verticalCenter
            width: 1
            height: 16
            color: Theme.border
            visible: projectLabel.visible
        }

        // Hidden rather than showing a placeholder name when nothing is open —
        // the UI must not imply a project exists.
        Text {
            id: projectLabel
            anchors.verticalCenter: parent.verticalCenter
            text: App.projectName
            visible: text.length > 0
            color: Theme.textPrimary
            font.pointSize: Metrics.fontSizeBody
            font.weight: Font.Medium
        }
    }

    // Command palette entry point. Reads as a search field but is a button: it
    // opens the palette rather than accepting inline input.
    Rectangle {
        id: searchField

        anchors.centerIn: parent
        width: Metrics.searchFieldWidth
        height: 30
        radius: Metrics.radiusMedium
        color: Theme.bgSurface
        border.width: 1
        border.color: searchMouse.containsMouse ? Theme.borderStrong : Theme.border

        Behavior on border.color {
            ColorAnimation { duration: App.fastAnimationDuration }
        }

        Row {
            anchors.fill: parent
            anchors.leftMargin: 12
            anchors.rightMargin: 10
            spacing: 10

            Icon {
                anchors.verticalCenter: parent.verticalCenter
                source: Icons.search
                size: 15
                color: Theme.textTertiary
            }

            Text {
                anchors.verticalCenter: parent.verticalCenter
                width: parent.width - 15 - keycap.width - parent.spacing * 2
                text: qsTr("Search files, symbols, commands")
                color: Theme.textTertiary
                font.pointSize: Metrics.fontSizeBody
                elide: Text.ElideRight
            }

            Keycap {
                id: keycap
                anchors.verticalCenter: parent.verticalCenter
                text: "Ctrl+K"
            }
        }

        // The palette itself arrives in milestone 8. Until then this reads as a
        // hint about the shortcut rather than offering a button that does nothing.
        MouseArea {
            id: searchMouse
            anchors.fill: parent
            hoverEnabled: true
        }
    }

    Row {
        anchors.right: parent.right
        anchors.rightMargin: Metrics.spacingMedium
        anchors.verticalCenter: parent.verticalCenter
        spacing: Metrics.spacingSmall

        IconButton {
            anchors.verticalCenter: parent.verticalCenter
            source: Theme.mode === Theme.Dark ? Icons.sun : Icons.moon
            tooltip: qsTr("Toggle theme")
            onClicked: App.invokeCommand("workbench.toggleTheme")
        }
    }
}
