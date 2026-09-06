import QtQuick
import Keys.Ui

/// A free-text value.
///
/// Commits on Enter or on losing focus rather than per keystroke: writing
/// "Cascadia Mono" a character at a time would send the editor through nine
/// invalid font names on the way.
Rectangle {
    id: root

    required property string value
    property string placeholder: ""

    signal committed(string value)

    implicitWidth: 200
    implicitHeight: 28

    radius: Metrics.radiusSmall
    color: Theme.bgChrome
    border.width: 1
    border.color: input.activeFocus ? Theme.focusRing : Theme.border

    Behavior on border.color {
        ColorAnimation { duration: App.fastAnimationDuration }
    }

    TextInput {
        id: input

        anchors.fill: parent
        anchors.leftMargin: Metrics.spacingSmall
        anchors.rightMargin: Metrics.spacingSmall
        verticalAlignment: TextInput.AlignVCenter

        text: root.value
        color: Theme.textPrimary
        font.family: Fonts.ui
        font.pointSize: Metrics.fontSizeBody
        selectByMouse: true
        selectionColor: Theme.selection
        selectedTextColor: Theme.textPrimary
        clip: true

        onEditingFinished: root.committed(text)

        Text {
            anchors.verticalCenter: parent.verticalCenter
            visible: input.text.length === 0 && !input.activeFocus
            text: root.placeholder
            color: Theme.textTertiary
            font.family: Fonts.ui
            font.pointSize: Metrics.fontSizeBody
        }
    }

    /// The value can change from elsewhere - a reset, a workspace file loading -
    /// and the field must follow unless the user is mid-edit.
    Connections {
        target: root
        function onValueChanged() {
            if (!input.activeFocus) {
                input.text = root.value;
            }
        }
    }
}
