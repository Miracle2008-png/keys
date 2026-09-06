import QtQuick
import QtQuick.Controls
import Keys.Ui

/// What Keys is and what it is built on.
///
/// Carries the version and the Qt build, because those are the two things
/// somebody reporting a problem is asked for.
Dialog {
    id: root

    parent: Overlay.overlay
    anchors.centerIn: parent
    width: 380
    modal: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    padding: Metrics.spacingLarge

    background: Rectangle {
        color: Theme.bgElevated
        border.width: 1
        border.color: Theme.borderStrong
        radius: Metrics.radiusLarge
    }

    contentItem: Column {
        spacing: Metrics.spacingSmall

        Row {
            spacing: Metrics.spacingMedium

            Image {
                anchors.verticalCenter: parent.verticalCenter
                source: "qrc:/branding/generated/keys-256.png"
                sourceSize.width: 48
                sourceSize.height: 48
                width: 48
                height: 48
            }

            Column {
                anchors.verticalCenter: parent.verticalCenter
                spacing: 2

                Text {
                    text: qsTr("Keys")
                    color: Theme.textPrimary
                    font.family: Fonts.ui
                    font.pointSize: Metrics.fontSizeTitle
                }

                Text {
                    text: qsTr("Version %1").arg(App.version)
                    color: Theme.textTertiary
                    font.family: Fonts.mono
                    font.pointSize: Metrics.fontSizeSmall
                }
            }
        }

        Item { width: 1; height: Metrics.spacingSmall }

        Text {
            width: parent.width
            text: qsTr("A fast, focused editor for people who build software for a living.")
            color: Theme.textSecondary
            font.family: Fonts.ui
            font.pointSize: Metrics.fontSizeBody
            wrapMode: Text.WordWrap
        }

        Text {
            width: parent.width
            text: qsTr("Built with Qt %1").arg(App.qtVersion)
            color: Theme.textTertiary
            font.family: Fonts.ui
            font.pointSize: Metrics.fontSizeSmall
        }
    }

    footer: Item {
        implicitHeight: 56

        DialogButton {
            anchors.right: parent.right
            anchors.rightMargin: Metrics.spacingLarge
            anchors.verticalCenter: parent.verticalCenter
            text: qsTr("Close")
            primary: true
            onClicked: root.close()
        }
    }
}
