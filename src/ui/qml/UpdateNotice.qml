import QtQuick
import Keys.Ui

/// A quiet notice that a newer release exists.
///
/// **It waits, it does not interrupt.** An update is not urgent and the user is
/// in the middle of something; a modal for it would be an imposition. This sits
/// in the corner until it is acted on or dismissed, which is what VS Code and
/// every well-behaved application does.
///
/// **Nothing happens automatically.** The notice links to the release; Keys
/// does not download or run anything. Fetching an executable and executing it
/// needs signature checking and a way back when the replacement fails, and none
/// of that is worth building before someone has asked for it.
Rectangle {
    id: root

    property string version: ""

    width: 340
    height: content.implicitHeight + Metrics.spacingMedium * 2
    radius: Metrics.radiusMedium
    color: Theme.bgElevated
    border.width: 1
    border.color: Theme.border
    visible: opacity > 0
    opacity: 0

    Behavior on opacity {
        NumberAnimation {
            duration: App.animationDuration
            easing.type: Easing.OutCubic
        }
    }

    // Rises as it appears - a small movement that reads as arriving rather than
    // blinking into place.
    transform: Translate {
        y: root.opacity > 0 ? 0 : 8

        Behavior on y {
            NumberAnimation {
                duration: App.animationDuration
                easing.type: Easing.OutCubic
            }
        }
    }

    Column {
        id: content

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        anchors.leftMargin: Metrics.spacingMedium
        anchors.rightMargin: Metrics.spacingMedium
        spacing: 4

        Text {
            text: qsTr("Keys %1 is available").arg(root.version)
            color: Theme.textPrimary
            font.family: Fonts.ui
            font.pointSize: Metrics.fontSizeBody
            font.weight: Font.Medium
        }

        Text {
            text: qsTr("You are running %1.").arg(Updates.currentVersion)
            color: Theme.textTertiary
            font.family: Fonts.ui
            font.pointSize: Metrics.fontSizeSmall
        }

        Item { width: 1; height: 6 }

        Row {
            spacing: Metrics.spacingSmall

            WelcomeButton {
                text: qsTr("View release")
                primary: true
                onClicked: {
                    Updates.openReleasePage();
                    root.dismiss();
                }
            }

            WelcomeButton {
                text: qsTr("Later")
                onClicked: root.dismiss()
            }
        }
    }

    function show(version) {
        root.version = version;
        root.opacity = 1;
    }

    function dismiss() {
        root.opacity = 0;
    }
}
