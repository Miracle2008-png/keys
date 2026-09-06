import QtQuick
import QtQuick.Controls
import Keys.Ui

/// The base every dialog in Keys derives from.
///
/// **Motion is shared, not repeated.** A dialog that appears instantly reads as
/// a screenshot rather than a window; one that fades and lifts reads as the
/// application responding. Putting that here means every dialog moves the same
/// way, and a change to the timing is one edit rather than five.
///
/// **Short, and never in the way.** The enter is quick enough that a fast typist
/// is not held up, and the exit is quicker still - waiting to dismiss something
/// is far more irritating than waiting to see it. Both read their duration from
/// the animation policy, so "off" is genuinely off rather than merely fast.
///
/// **It comes from the top.** A dialog that materialises in the middle of the
/// screen reads as an alert the system interrupted you with. Sliding down from
/// the window's own top edge reads as this window producing it, which is what
/// it is - and it leaves the eye somewhere predictable, since every dialog
/// arrives from the same direction rather than from wherever it happens to sit.
///
/// **Scale from near, not from nothing.** Growing from 0.98 rather than 0 keeps
/// the dialog legible for the whole transition; a dialog that springs from a
/// point draws attention to the animation instead of to what it asks.
Dialog {
    id: root

    parent: Overlay.overlay

    // Near the top rather than centred. A file name prompt belongs where the
    // file tree and the tab bar already are; centring it puts the question as
    // far as possible from the thing it is about.
    x: Math.round((parent.width - width) / 2)
    y: Math.round(Math.min(parent.height * 0.16, 140))
    modal: true
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    padding: Metrics.spacingMedium

    background: Rectangle {
        color: Theme.bgElevated
        border.width: 1
        border.color: Theme.borderStrong
        radius: Metrics.radiusLarge
    }

    // The backdrop dims the workbench so the dialog is plainly the thing being
    // answered, and fades with it rather than snapping.
    Overlay.modal: Rectangle {
        color: Qt.rgba(0, 0, 0, 0.45)

        Behavior on opacity {
            NumberAnimation {
                duration: App.animationDuration
                easing.type: Easing.OutQuad
            }
        }
    }

    enter: Transition {
        // Down from above, decelerating into place. OutCubic is what makes it
        // read as arriving rather than as being dropped: fast at first, then
        // settling, the way something with weight comes to rest.
        NumberAnimation {
            property: "y"
            from: root.y - 28
            to: root.y
            duration: App.animationDuration
            easing.type: Easing.OutCubic
        }
        NumberAnimation {
            property: "opacity"
            from: 0.0
            to: 1.0
            duration: App.animationDuration
            easing.type: Easing.OutCubic
        }
        NumberAnimation {
            property: "scale"
            from: 0.98
            to: 1.0
            duration: App.animationDuration
            easing.type: Easing.OutCubic
        }
    }

    exit: Transition {
        // Back up the way it came, and quicker: waiting to dismiss something is
        // far more irritating than waiting to see it.
        NumberAnimation {
            property: "y"
            from: root.y
            to: root.y - 20
            duration: App.fastAnimationDuration
            easing.type: Easing.InCubic
        }
        NumberAnimation {
            property: "opacity"
            from: 1.0
            to: 0.0
            duration: App.fastAnimationDuration
            easing.type: Easing.InCubic
        }
    }
}
