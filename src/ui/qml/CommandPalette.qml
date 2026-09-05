import QtQuick
import QtQuick.Controls
import Keys.Ui

/// The command palette.
///
/// Follows the design: 560 wide, capped at 420 tall, dimmed backdrop, grouped
/// results with the first selected so Enter runs the best match without any
/// navigation.
Item {
    id: root

    anchors.fill: parent
    visible: opacity > 0
    opacity: 0
    z: 100

    readonly property bool isOpen: opacity > 0

    // Fades rather than appearing abruptly, and honours the animation setting -
    // at "off" it simply appears.
    Behavior on opacity {
        NumberAnimation {
            duration: App.animationDuration
            easing.type: Easing.OutCubic
        }
    }

    // The backdrop dims the workbench so the palette is unambiguously the thing
    // with focus. Clicking it dismisses, which is what a dimmed overlay implies.
    Rectangle {
        anchors.fill: parent
        color: "#000000"
        opacity: 0.45

        MouseArea {
            anchors.fill: parent
            onClicked: root.close()
        }
    }

    Rectangle {
        id: panel

        anchors.horizontalCenter: parent.horizontalCenter
        y: Math.round(parent.height * 0.1)
        width: Metrics.paletteWidth
        height: Math.min(Metrics.paletteMaxHeight, header.height + list.contentHeight + 16)

        color: Theme.bgElevated
        border.width: 1
        border.color: Theme.borderStrong
        radius: Metrics.radiusLarge
        clip: true

        // Rises slightly as it appears - a small movement that reads as the
        // panel arriving rather than blinking into place.
        transform: Translate {
            y: root.isOpen ? 0 : -8
            Behavior on y {
                NumberAnimation {
                    duration: App.animationDuration
                    easing.type: Easing.OutCubic
                }
            }
        }

        Item {
            id: header

            width: parent.width
            height: 50

            TextInput {
                id: input

                anchors.fill: parent
                anchors.leftMargin: 16
                anchors.rightMargin: 16
                verticalAlignment: TextInput.AlignVCenter

                color: Theme.textPrimary
                font.family: Fonts.ui
                font.pointSize: Metrics.fontSizeLarge
                selectByMouse: true
                selectionColor: Theme.accentSoft
                selectedTextColor: Theme.textPrimary

                onTextChanged: Palette.query = text

                Text {
                    anchors.verticalCenter: parent.verticalCenter
                    visible: input.text.length === 0
                    text: Palette.placeholder
                    color: Theme.textTertiary
                    font.family: Fonts.ui
                    font.pointSize: Metrics.fontSizeLarge
                }

                // Navigation is handled here rather than on the list, because
                // the input keeps focus throughout - the user types and steers
                // without ever tabbing away.
                Keys.onDownPressed: Palette.selectNext()
                Keys.onUpPressed: Palette.selectPrevious()
                Keys.onEscapePressed: root.close()
                Keys.onReturnPressed: root.accept()
                Keys.onEnterPressed: root.accept()
            }

            Rectangle {
                anchors.bottom: parent.bottom
                width: parent.width
                height: 1
                color: Theme.border
            }
        }

        ListView {
            id: list

            anchors.top: header.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: 8

            model: Palette
            clip: true
            reuseItems: true
            boundsBehavior: Flickable.StopAtBounds
            currentIndex: Palette.selectedIndex

            // Keeps the selection visible as the user arrows through a list
            // longer than the panel.
            highlightFollowsCurrentItem: true
            highlightMoveDuration: App.fastAnimationDuration
            preferredHighlightBegin: 0
            preferredHighlightEnd: height
            highlightRangeMode: ListView.ApplyRange

            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

            delegate: Column {
                id: row

                required property int index
                required property string title
                required property string subtitle
                required property string group
                required property bool isGroupStart
                required property bool enabled

                width: list.width

                // The group heading, drawn by the first row of each group
                // rather than as its own model row - which would complicate
                // selection and keyboard navigation for no visual gain.
                Text {
                    visible: row.isGroupStart
                    height: visible ? implicitHeight + 10 : 0
                    leftPadding: 10
                    topPadding: 6
                    bottomPadding: 4
                    text: row.group
                    color: Theme.textTertiary
                    font.family: Fonts.ui
                    font.pointSize: Metrics.fontSizeLabel
                    font.capitalization: Font.AllUppercase
                    font.letterSpacing: 0.5
                }

                Rectangle {
                    width: parent.width
                    height: 34
                    radius: 7
                    color: row.index === Palette.selectedIndex ? Theme.accentSoft
                         : rowMouse.containsMouse ? Theme.bgHover
                         : "transparent"

                    Row {
                        anchors.left: parent.left
                        anchors.leftMargin: 10
                        anchors.right: parent.right
                        anchors.rightMargin: 10
                        anchors.verticalCenter: parent.verticalCenter
                        spacing: 8

                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            text: row.title
                            color: !row.enabled ? Theme.textTertiary
                                 : row.index === Palette.selectedIndex ? Theme.textPrimary
                                 : Theme.textSecondary
                            font.family: Fonts.ui
                            font.pointSize: Metrics.fontSizeMedium
                            elide: Text.ElideMiddle
                            width: Math.min(implicitWidth, parent.width - 140)
                        }

                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            visible: row.subtitle.length > 0
                            text: row.subtitle
                            color: Theme.textTertiary
                            font.family: Fonts.ui
                            font.pointSize: Metrics.fontSizeBody
                            elide: Text.ElideMiddle
                            width: Math.min(implicitWidth, 200)
                        }
                    }

                    MouseArea {
                        id: rowMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            Palette.selectedIndex = row.index;
                            root.accept();
                        }
                    }
                }
            }
        }

        // Shown only when a query genuinely found nothing, so an empty palette
        // on opening does not read as a failed search.
        Text {
            anchors.centerIn: parent
            anchors.verticalCenterOffset: header.height / 2
            visible: Palette.count === 0 && input.text.length > 0
            text: qsTr("No matching commands or files")
            color: Theme.textTertiary
            font.family: Fonts.ui
            font.pointSize: Metrics.fontSizeBody
        }
    }

    function open(initialQuery) {
        input.text = initialQuery !== undefined ? initialQuery : "";
        Palette.query = input.text;
        opacity = 1;
        input.forceActiveFocus();
        input.selectAll();
    }

    function close() {
        opacity = 0;
        input.text = "";
    }

    function accept() {
        if (Palette.acceptSelected()) {
            close();
        }
    }

    Connections {
        target: Palette
        function onAccepted() { root.close(); }
    }
}
