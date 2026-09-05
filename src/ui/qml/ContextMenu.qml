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

    implicitWidth: 190
    topPadding: 6
    bottomPadding: 6

    background: Rectangle {
        color: Theme.bgElevated
        border.width: 1
        border.color: Theme.borderStrong
        radius: Metrics.radiusMedium
    }

    delegate: MenuItem {
        id: item

        implicitHeight: 30
        leftPadding: 12
        rightPadding: 12

        contentItem: Text {
            text: item.text
            color: item.enabled ? (item.highlighted ? Theme.textPrimary
                                                    : Theme.textSecondary)
                                : Theme.textTertiary
            font.family: Fonts.ui
            font.pointSize: Metrics.fontSizeBody
            verticalAlignment: Text.AlignVCenter
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
