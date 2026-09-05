import QtQuick
import Keys.Ui

/// The 24px status bar.
///
/// The design paints it in the accent color, which makes it the strongest
/// horizontal line in the window - so it carries only state that is true. Nothing
/// here is populated until the subsystem behind it exists.
Rectangle {
    id: root

    height: Metrics.statusBarHeight
    color: Theme.accent

    Row {
        anchors.left: parent.left
        anchors.leftMargin: 12
        anchors.verticalCenter: parent.verticalCenter
        spacing: 14

        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: qsTr("Ready")
            color: "white"
            font.family: Fonts.ui
            font.pointSize: Metrics.fontSizeLabel
        }

        // The branch, and how far it has diverged. Shown only inside a
        // repository - the status bar carries state that is true, so a project
        // without git shows nothing here rather than a dash.
        Row {
            anchors.verticalCenter: parent.verticalCenter
            spacing: 5
            visible: SourceControl.hasRepository

            Icon {
                anchors.verticalCenter: parent.verticalCenter
                source: Icons.git
                size: 12
                color: "white"
            }

            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: SourceControl.branch
                color: "white"
                font.family: Fonts.ui
                font.pointSize: Metrics.fontSizeLabel
            }

            Text {
                anchors.verticalCenter: parent.verticalCenter
                visible: SourceControl.hasUpstream
                         && (SourceControl.ahead > 0 || SourceControl.behind > 0)
                text: (SourceControl.behind > 0 ? "↓" + SourceControl.behind : "")
                      + (SourceControl.ahead > 0 && SourceControl.behind > 0 ? " " : "")
                      + (SourceControl.ahead > 0 ? "↑" + SourceControl.ahead : "")
                color: "white"
                opacity: 0.85
                font.family: Fonts.ui
                font.pointSize: Metrics.fontSizeLabel
            }

            // Pending changes, so the count is visible without opening the panel.
            Text {
                anchors.verticalCenter: parent.verticalCenter
                visible: SourceControl.count > 0
                text: "· " + SourceControl.count
                color: "white"
                opacity: 0.85
                font.family: Fonts.ui
                font.pointSize: Metrics.fontSizeLabel
            }
        }
    }

    Row {
        anchors.right: parent.right
        anchors.rightMargin: 12
        anchors.verticalCenter: parent.verticalCenter
        spacing: 14

        Text {
            anchors.verticalCenter: parent.verticalCenter
            text: "UTF-8"
            color: "white"
            font.family: Fonts.ui
            font.pointSize: Metrics.fontSizeLabel
        }
    }
}
