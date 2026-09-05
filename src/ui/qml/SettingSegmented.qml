import QtQuick
import Keys.Ui

/// A small set of mutually exclusive choices, shown side by side.
///
/// Used instead of a dropdown because every setting that has allowed values has
/// two or three of them: a dropdown would hide the options behind a click and
/// cost a popup, where segments show the whole choice and the current state at
/// once.
Item {
    id: root

    required property var choices        ///< list of value strings
    required property string value

    signal chosen(string value)

    /// Values are identifiers ("sourceControl"); labels are what a person reads.
    /// Splitting camelCase and capitalising covers every current case without a
    /// per-setting translation table.
    function labelFor(raw) {
        const spaced = String(raw).replace(/([a-z])([A-Z])/g, "$1 $2");
        return spaced.charAt(0).toUpperCase() + spaced.slice(1);
    }

    implicitWidth: row.implicitWidth + 6
    implicitHeight: 28

    Rectangle {
        anchors.fill: parent
        radius: Metrics.radiusSmall
        color: Theme.bgChrome
        border.width: 1
        border.color: Theme.border
    }

    Row {
        id: row

        anchors.centerIn: parent
        spacing: 2

        Repeater {
            model: root.choices

            delegate: Rectangle {
                id: segment

                required property string modelData

                readonly property bool selected: segment.modelData === root.value

                width: label.implicitWidth + 22
                height: 22
                radius: Metrics.radiusSmall - 1
                color: segment.selected ? Theme.accent
                     : segmentMouse.containsMouse ? Theme.bgHover
                     : "transparent"

                Behavior on color {
                    ColorAnimation { duration: App.fastAnimationDuration }
                }

                Text {
                    id: label

                    anchors.centerIn: parent
                    text: root.labelFor(segment.modelData)
                    color: segment.selected ? "#ffffff" : Theme.textSecondary
                    font.family: Fonts.ui
                    font.pointSize: Metrics.fontSizeSmall
                }

                MouseArea {
                    id: segmentMouse

                    anchors.fill: parent
                    hoverEnabled: true
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.chosen(segment.modelData)
                }
            }
        }
    }
}
