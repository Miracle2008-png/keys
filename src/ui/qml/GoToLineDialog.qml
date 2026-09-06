import QtQuick
import QtQuick.Controls
import Keys.Ui

/// Asks for a line number.
///
/// Its own dialog rather than a palette mode: a number is not a search, and
/// making people type a prefix to reach it would be a puzzle rather than a
/// shortcut. Ctrl+G is what every editor binds, and it should go straight here.
KeysDialog {
    id: root

    width: 260
    title: qsTr("Go to Line")

    /// Emitted with a one-based line number, which is how editors are read and
    /// how the status bar reports position.
    signal lineAccepted(int line)

    header: Text {
        text: root.title
        color: Theme.textPrimary
        font.family: Fonts.ui
        font.pointSize: Metrics.fontSizeMedium
        font.weight: Font.DemiBold
        padding: Metrics.spacingMedium
    }

    contentItem: Rectangle {
        implicitHeight: 30
        radius: Metrics.radiusSmall
        color: Theme.bgSurface
        border.width: 1
        border.color: input.activeFocus ? Theme.accent : Theme.border

        TextInput {
            id: input

            anchors.fill: parent
            anchors.leftMargin: Metrics.spacingSmall
            anchors.rightMargin: Metrics.spacingSmall
            verticalAlignment: TextInput.AlignVCenter
            color: Theme.textPrimary
            font.family: Fonts.mono
            font.pointSize: Metrics.fontSizeBody
            selectByMouse: true
            selectionColor: Theme.accentSoft
            selectedTextColor: Theme.textPrimary

            // Digits only. A validator is kinder than accepting anything and
            // then refusing it: the field simply will not take a wrong key.
            validator: IntValidator { bottom: 1; top: 9999999 }

            onAccepted: root.accept()
        }
    }

    footer: Row {
        spacing: Metrics.spacingSmall
        padding: Metrics.spacingMedium
        layoutDirection: Qt.RightToLeft

        DialogButton {
            text: qsTr("Go")
            primary: true
            onClicked: root.accept()
        }

        DialogButton {
            text: qsTr("Cancel")
            onClicked: root.close()
        }
    }

    function open() {
        input.text = "";
        root.visible = true;
        input.forceActiveFocus();
    }

    function accept() {
        const line = parseInt(input.text, 10);
        if (!isNaN(line) && line > 0) {
            root.lineAccepted(line);
        }
        root.close();
    }
}
