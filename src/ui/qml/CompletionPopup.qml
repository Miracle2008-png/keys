import QtQuick
import QtQuick.Controls
import Keys.Ui

/// The completion list, anchored under the caret.
///
/// Deliberately compact: it covers the code being written, so it shows as few
/// rows as are useful and gets out of the way as soon as something is chosen.
Rectangle {
    id: root

    /// Where the caret is, in editor pixels. The popup positions itself from
    /// this rather than anchoring, because it must flip above the caret when
    /// there is no room below.
    property real caretX: 0
    property real caretY: 0
    property real lineHeight: 18

    // Driven by opacity rather than by `visible` alone, so the popup can fade
    // rather than blink. It appears while the user is typing - the one place in
    // the editor where a hard snap is most noticeable and least wanted.
    readonly property bool shown: Language.completionVisible && Language.count > 0

    visible: opacity > 0
    opacity: shown ? 1 : 0

    Behavior on opacity {
        NumberAnimation {
            duration: App.fastAnimationDuration
            easing.type: Easing.OutCubic
        }
    }

    width: 340
    height: Math.min(240, list.contentHeight + 8)

    color: Theme.bgElevated
    border.width: 1
    border.color: Theme.borderStrong
    radius: Metrics.radiusMedium
    clip: true
    z: 50

    x: Math.max(4, Math.min(caretX, (parent ? parent.width : 0) - width - 4))

    // Below the caret, or above it when the popup would fall off the bottom.
    y: {
        const below = caretY + lineHeight + 4;
        const available = parent ? parent.height : 0;
        return below + height > available ? Math.max(4, caretY - height - 4) : below;
    }

    ListView {
        id: list

        anchors.fill: parent
        anchors.margins: 4

        model: Language
        clip: true
        reuseItems: true
        boundsBehavior: Flickable.StopAtBounds
        currentIndex: Language.selectedIndex
        highlightFollowsCurrentItem: true
        highlightMoveDuration: App.fastAnimationDuration
        preferredHighlightBegin: 0
        preferredHighlightEnd: height
        highlightRangeMode: ListView.ApplyRange

        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

        delegate: Rectangle {
            id: row

            required property int index
            required property string label
            required property string detail
            required property string kindLabel

            width: list.width
            height: 24
            radius: 5
            color: row.index === Language.selectedIndex ? Theme.selection
                 : rowHover.hovered ? Theme.bgHover
                 : "transparent"

            HoverHandler {
                id: rowHover
                cursorShape: Qt.PointingHandCursor
            }

            Text {
                id: kind

                anchors.left: parent.left
                anchors.leftMargin: 8
                anchors.verticalCenter: parent.verticalCenter
                width: 34
                text: row.kindLabel
                color: Theme.textTertiary
                font.family: Fonts.mono
                font.pointSize: Metrics.fontSizeLabel
                elide: Text.ElideRight
            }

            Text {
                id: labelText

                anchors.left: kind.right
                anchors.leftMargin: 4
                anchors.verticalCenter: parent.verticalCenter
                text: row.label
                color: row.index === Language.selectedIndex ? Theme.textPrimary
                                                            : Theme.textSecondary
                font.family: Fonts.mono
                font.pointSize: Metrics.fontSizeSmall
                elide: Text.ElideRight
                width: Math.min(implicitWidth, parent.width - 130)
            }

            Text {
                anchors.left: labelText.right
                anchors.leftMargin: 8
                anchors.right: parent.right
                anchors.rightMargin: 8
                anchors.verticalCenter: parent.verticalCenter
                visible: row.detail.length > 0
                text: row.detail
                color: Theme.textTertiary
                font.family: Fonts.mono
                font.pointSize: Metrics.fontSizeLabel
                elide: Text.ElideRight
                horizontalAlignment: Text.AlignRight
            }

            MouseArea {
                anchors.fill: parent
                onClicked: {
                    Language.selectedIndex = row.index;
                    Language.acceptCompletion();
                }
            }
        }
    }
}
