import QtQuick
import QtQuick.Controls
import Keys.Ui

/// A themed popup menu.
///
/// QtQuick Controls' Basic style draws a flat black rectangle with square
/// corners and no hover feedback, which is visibly not the same product as the
/// rest of Keys. Rather than restyle Menu at every use site, every menu in the
/// application derives from this one.
Menu {
    id: root

    // Wide enough for a label and its shortcut without the two colliding. A
    // menu that resizes per item would jitter as it opens.
    implicitWidth: 240
    topPadding: 6
    bottomPadding: 6

    background: Rectangle {
        color: Theme.bgElevated
        border.width: 1
        border.color: Theme.borderStrong
        radius: Metrics.radiusMedium
    }

    // Quick, and from near rather than from nothing: a menu is on the way to
    // something else, so the motion confirms it opened without delaying the
    // choice. The exit is faster still - waiting to dismiss is worse than
    // waiting to see.
    enter: Transition {
        NumberAnimation {
            property: "opacity"
            from: 0.0
            to: 1.0
            duration: App.fastAnimationDuration
            easing.type: Easing.OutCubic
        }
        NumberAnimation {
            property: "scale"
            from: 0.97
            to: 1.0
            duration: App.fastAnimationDuration
            easing.type: Easing.OutCubic
        }
    }

    exit: Transition {
        NumberAnimation {
            property: "opacity"
            from: 1.0
            to: 0.0
            duration: App.fastAnimationDuration
            easing.type: Easing.InCubic
        }
    }

    delegate: MenuItem {
        id: item

        implicitHeight: 30
        leftPadding: 12
        rightPadding: 12

        contentItem: Item {
            implicitHeight: label.implicitHeight

            Text {
                id: label

                anchors.left: parent.left
                anchors.right: shortcut.left
                anchors.rightMargin: Metrics.spacingMedium
                anchors.verticalCenter: parent.verticalCenter
                text: item.text
                color: item.enabled ? (item.highlighted ? Theme.textPrimary
                                                        : Theme.textSecondary)
                                    : Theme.textTertiary
                font.family: Fonts.ui
                font.pointSize: Metrics.fontSizeBody
                elide: Text.ElideRight
            }

            // The shortcut, right-aligned and quieter than the label - it is a
            // reminder, not the thing being chosen. Blank for items that have
            // none, which is most of them.
            Text {
                id: shortcut

                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                text: ""
                color: Theme.textTertiary
                font.family: Fonts.mono
                font.pointSize: Metrics.fontSizeLabel
            }
        }

        background: Rectangle {
            // Inset so the highlight reads as a rounded row inside the menu
            // rather than a band touching its border.
            anchors.fill: parent
            anchors.leftMargin: 5
            anchors.rightMargin: 5
            radius: Metrics.radiusSmall
            color: item.highlighted ? Theme.accentSoft : "transparent"
        }
    }
}
