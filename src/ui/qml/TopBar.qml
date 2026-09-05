import QtQuick
import QtQuick.Controls
import Keys.Ui

/// The 46px application bar: identity and project on the left, the search field
/// centred, run/focus/settings controls on the right.
Rectangle {
    id: root

    height: Metrics.topBarHeight
    color: Theme.bgChrome

    /// The window owns the palette, so the bar reports the intent rather than
    /// reaching across the layout for it.
    signal paletteRequested()

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

            // The Keys mark, from the same SVG the application icon is rendered
            // from - so the logo in the window and the icon in the taskbar are
            // the same artwork rather than two drawings that can drift apart.
            Image {
                anchors.verticalCenter: parent.verticalCenter
                source: "qrc:/branding/keys-mark.svg"
                sourceSize.width: 18
                sourceSize.height: 18
                width: 18
                height: 18
                // Rendered at the device's real pixel density, not scaled up
                // from a logical-pixel raster.
                mipmap: true
                smooth: true
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
            visible: App.hasProject
            color: Theme.textPrimary
            font.family: Fonts.ui
            font.pointSize: Metrics.fontSizeBody
            font.weight: Font.Medium

            // The full path is the disambiguator when several projects share a
            // folder name, which is common ("client", "server", "web").
            ToolTip {
                text: App.projectRoot
                visible: projectMouse.containsMouse && App.hasProject
            }

            MouseArea {
                id: projectMouse
                anchors.fill: parent
                hoverEnabled: true
            }
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

        MouseArea {
            id: searchMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: root.paletteRequested()
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
