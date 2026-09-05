import QtQuick
import Keys.Ui

/// One setting: its label and description on the left, its control on the right.
///
/// The control is chosen by the model from the setting's declared type, not
/// hard-coded here - so adding a setting to the schema adds a working row rather
/// than requiring a matching edit in QML.
Item {
    id: root

    required property string settingKey
    required property string title
    required property string description
    required property int control
    required property var value
    required property var choices
    required property real minimum
    required property real maximum
    required property bool isInteger
    required property bool isModified

    /// Shared by every row so the controls form a column. Wide enough for the
    /// widest control the schema produces (the three-way animation choice).
    readonly property int controlColumnWidth: 220

    implicitHeight: Math.max(44, text.implicitHeight + 20)

    Item {
        id: text

        anchors.left: parent.left
        anchors.verticalCenter: parent.verticalCenter
        // Leaves the control room on the right without either overlapping.
        width: parent.width - trailing.width - Metrics.spacingLarge

        implicitHeight: titleText.implicitHeight
                        + (root.description.length > 0
                           ? descriptionText.implicitHeight + 2 : 0)
        height: implicitHeight

        Text {
            id: titleText

            width: parent.width
            text: root.title
            color: Theme.textPrimary
            font.family: Fonts.ui
            font.pointSize: Metrics.fontSizeBody
            elide: Text.ElideRight
        }

        Text {
            id: descriptionText

            anchors.top: titleText.bottom
            anchors.topMargin: 2
            width: parent.width
            visible: root.description.length > 0
            text: root.description
            color: Theme.textTertiary
            font.family: Fonts.ui
            font.pointSize: Metrics.fontSizeSmall
            wrapMode: Text.WordWrap
        }
    }

    // Reset and control sit in one right-aligned row, so a wide control (the
    // segmented choices) cannot collide with the reset affordance the way
    // independently anchored items did.
    Row {
        id: trailing

        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        spacing: Metrics.spacingSmall
        layoutDirection: Qt.LeftToRight

        Item {
            id: resetButton

            width: 18
            height: 18
            anchors.verticalCenter: parent.verticalCenter
            // Occupies its slot whether or not it is shown, so the control does
            // not shift sideways the moment a setting is changed.
            opacity: root.isModified ? 1 : 0
            enabled: root.isModified

            Behavior on opacity {
                NumberAnimation { duration: App.fastAnimationDuration }
            }

            Text {
                anchors.centerIn: parent
                text: "↺"
                color: resetMouse.containsMouse ? Theme.textPrimary : Theme.textTertiary
                font.pointSize: Metrics.fontSizeBody
            }

            MouseArea {
                id: resetMouse

                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: SettingsList.resetValue(root.settingKey)
            }
        }

        // A fixed column rather than each control's own width. Right-aligning
        // controls of different widths leaves a ragged edge, which reads as
        // carelessness at a glance; a shared column lines every control up and
        // lets the eye scan down them.
        Item {
            id: controlArea

            anchors.verticalCenter: parent.verticalCenter
            width: root.controlColumnWidth
            height: loader.item ? loader.item.implicitHeight : 0

            Loader {
                id: loader

                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                width: item ? item.implicitWidth : 0
                height: item ? item.implicitHeight : 0
                sourceComponent: {
                    switch (root.control) {
                    case SettingsList.Toggle:  return toggleComponent;
                    case SettingsList.Choice:  return choiceComponent;
                    case SettingsList.Slider:  return sliderComponent;
                    default:                   return textComponent;
                    }
                }
            }
        }
    }

    Component {
        id: toggleComponent

        SettingSwitch {
            checked: root.value === true
            onToggled: (next) => SettingsList.setValue(root.settingKey, next)
        }
    }

    Component {
        id: choiceComponent

        SettingSegmented {
            choices: root.choices
            value: String(root.value)
            onChosen: (next) => SettingsList.setValue(root.settingKey, next)
        }
    }

    Component {
        id: sliderComponent

        SettingSlider {
            // Spans the column: a slider's length is its scale, and two sliders
            // of different lengths would misrepresent their ranges relative to
            // each other.
            implicitWidth: root.controlColumnWidth
            value: Number(root.value)
            minimum: root.minimum
            maximum: root.maximum
            isInteger: root.isInteger
            onMoved: (next) => SettingsList.setValue(root.settingKey, next)
        }
    }

    Component {
        id: textComponent

        SettingTextField {
            implicitWidth: root.controlColumnWidth
            value: String(root.value)
            placeholder: qsTr("Platform default")
            onCommitted: (next) => SettingsList.setValue(root.settingKey, next)
        }
    }
}
