import QtQuick
import QtQuick.Controls
import Keys.Ui

/// Installed extensions and what they are allowed to do.
///
/// The capability list is the point of this view: an extension declares what it
/// needs, the user reads that in plain language and approves it or does not.
/// Nothing runs before that.
Item {
    id: root

    /// Picks the folder to install from. An extension is a folder holding a
    /// `keys-extension.json`; there is no archive format because Keys would
    /// have to carry an unpacker to read one, and a folder is what a developer
    /// building an extension already has.
    /// Uninstalling deletes files. Nothing else in this panel does, so it is
    /// the one action here that asks first.
    UninstallConfirmDialog {
        id: confirmUninstall

        onConfirmed: (extensionId) => Extensions.uninstall(extensionId)
    }

    FolderPicker {
        id: installPicker

        title: qsTr("Choose an extension folder")
        onFolderAccepted: (path) => Extensions.installFromFolder(path)
    }

    Column {
        anchors.fill: parent
        anchors.leftMargin: Metrics.spacingMedium
        anchors.rightMargin: Metrics.spacingMedium
        spacing: Metrics.spacingSmall

        // The install control, always present. It used to say only where the
        // folder was, which left copying files by hand as the sole way to
        // install anything.
        Item {
            width: parent.width
            height: 34

            PrimaryButton {
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                text: qsTr("Install from Folder")
                onClicked: installPicker.open()
            }
        }

        Text {
            width: parent.width
            visible: Extensions.count === 0
            text: qsTr("No extensions installed.\n\nAn extension is a folder with a "
                       + "keys-extension.json in it. Installing copies it to:\n%1")
                      .arg(Extensions.directory)
            color: Theme.textTertiary
            font.family: Fonts.ui
            font.pointSize: Metrics.fontSizeBody
            wrapMode: Text.Wrap
            topPadding: Metrics.spacingSmall
        }

        ListView {
            id: list

            width: parent.width
            height: parent.height - y
            visible: Extensions.count > 0

            model: Extensions
            clip: true
            spacing: 6
            boundsBehavior: Flickable.StopAtBounds

            ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

            delegate: Rectangle {
                id: card

                required property string extensionId
                required property string name
                required property string version
                required property string description
                required property bool running
                required property bool granted
                required property string error
                required property var capabilities

                width: list.width
                height: content.implicitHeight + 16
                radius: Metrics.radiusMedium
                color: Theme.bgChrome
                border.width: 1
                border.color: Theme.border

                Column {
                    id: content

                    anchors.left: parent.left
                    anchors.right: parent.right
                    anchors.top: parent.top
                    anchors.margins: 8
                    spacing: 4

                    Row {
                        width: parent.width
                        spacing: 6

                        Text {
                            text: card.name
                            color: Theme.textPrimary
                            font.family: Fonts.ui
                            font.pointSize: Metrics.fontSizeBody
                            font.weight: Font.Medium
                        }

                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            visible: card.version.length > 0
                            text: card.version
                            color: Theme.textTertiary
                            font.family: Fonts.mono
                            font.pointSize: Metrics.fontSizeLabel
                        }

                        // Running is a fact worth showing: a granted extension
                        // that is not running means something went wrong.
                        Rectangle {
                            anchors.verticalCenter: parent.verticalCenter
                            visible: card.running
                            width: 6
                            height: 6
                            radius: 3
                            color: Theme.green
                        }
                    }

                    Text {
                        width: parent.width
                        visible: card.description.length > 0
                        text: card.description
                        color: Theme.textTertiary
                        font.family: Fonts.ui
                        font.pointSize: Metrics.fontSizeSmall
                        wrapMode: Text.WordWrap
                    }

                    // A manifest that would not load says why, rather than the
                    // extension vanishing from the list.
                    Text {
                        width: parent.width
                        visible: card.error.length > 0
                        text: card.error
                        color: Theme.red
                        font.family: Fonts.ui
                        font.pointSize: Metrics.fontSizeSmall
                        wrapMode: Text.WordWrap
                    }

                    Item { width: 1; height: 2; visible: card.error.length === 0 }

                    SectionLabel {
                        visible: card.error.length === 0 && card.capabilities.length > 0
                        text: qsTr("This extension can")
                    }

                    Repeater {
                        model: card.error.length === 0 ? card.capabilities : []

                        delegate: Row {
                            required property var modelData

                            width: content.width
                            spacing: 6

                            Text {
                                anchors.verticalCenter: parent.verticalCenter
                                // A dot for granted, a hollow ring for not, so
                                // the state reads without colour alone.
                                text: modelData.granted ? "\u25cf" : "\u25cb"
                                color: modelData.granted ? Theme.green
                                     : modelData.sensitive ? Theme.yellow
                                     : Theme.textTertiary
                                font.pointSize: Metrics.fontSizeLabel
                            }

                            Text {
                                width: parent.width - 20
                                text: modelData.description
                                color: modelData.sensitive ? Theme.textSecondary
                                                           : Theme.textTertiary
                                font.family: Fonts.ui
                                font.pointSize: Metrics.fontSizeSmall
                                wrapMode: Text.WordWrap
                            }
                        }
                    }

                    Item { width: 1; height: 4 }

                    Row {
                        spacing: 6

                        DialogButton {
                            visible: card.error.length === 0 && !card.granted
                            primary: true
                            text: qsTr("Allow and run")
                            onClicked: Extensions.grantAll(card.extensionId)
                        }

                        DialogButton {
                            visible: card.error.length === 0 && card.granted
                            destructive: true
                            text: qsTr("Revoke")
                            onClicked: Extensions.revokeAll(card.extensionId)
                        }

                        // Shown even for an extension that failed to load -
                        // especially then. A broken extension is exactly the
                        // one a user wants to remove, and it was previously
                        // the only one they could not.
                        DialogButton {
                            destructive: true
                            text: qsTr("Uninstall")
                            onClicked: confirmUninstall.ask(card.extensionId,
                                                            card.name)
                        }
                    }
                }
            }
        }
    }
}
