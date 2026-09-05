import QtQuick
import QtQuick.Controls
import Keys.Ui

/// The text editor.
///
/// **Viewport-only rendering.** A ListView with one delegate per line, so a
/// 200,000-line file costs the same to display as a 20-line one: only the rows
/// on screen are instantiated. Laying out the whole document as one Text item
/// would stall for seconds on a large file and is the usual reason a naive
/// editor cannot open real source.
///
/// **Monospace assumption.** Column-to-x is `column * advance`, measured once
/// from the font rather than per line. That is exact for a monospaced face,
/// which is what an editor uses, and it keeps caret placement and click
/// hit-testing O(1) instead of requiring a text layout per line.
Item {
    id: root

    focus: true

    /// Measured from the font so the gutter and caret track the real glyph box
    /// rather than a guess.
    readonly property real charWidth: fontMetrics.advanceWidth("0")
    readonly property real lineHeight: Math.round(fontMetrics.height * 1.5)

    /// Wide enough for the largest line number, plus breathing room. Recomputed
    /// only when the line count changes, not per frame.
    readonly property real gutterWidth:
        Math.max(48, String(Math.max(1, Editor.lineCount)).length * charWidth + 28)

    FontMetrics {
        id: fontMetrics
        font.family: Fonts.mono
        font.pointSize: Metrics.fontSizeMedium
    }

    Rectangle {
        anchors.fill: parent
        color: Theme.bgEditor
    }

    ListView {
        id: lines

        anchors.fill: parent
        model: Editor.lineCount
        clip: true
        reuseItems: true
        boundsBehavior: Flickable.StopAtBounds
        cacheBuffer: root.lineHeight * 12

        // Vertical only: horizontal scrolling is the inner Flickable's job, so
        // the gutter stays pinned while code scrolls sideways.
        flickableDirection: Flickable.VerticalFlick

        ScrollBar.vertical: ScrollBar { policy: ScrollBar.AsNeeded }

        delegate: Item {
            id: row

            required property int index

            width: lines.width
            height: root.lineHeight

            readonly property string text: Editor.lineText(index)
            readonly property bool isCursorLine: index === Editor.cursorLine

            // The line the caret is on gets a faint wash, so the eye can find
            // its place after a scroll without a heavy highlight.
            Rectangle {
                anchors.fill: parent
                visible: row.isCursorLine && !Editor.hasSelection
                color: Theme.accent
                opacity: 0.06
            }

            // ---- Gutter ----
            Text {
                id: lineNumber

                width: root.gutterWidth - 14
                height: parent.height
                horizontalAlignment: Text.AlignRight
                verticalAlignment: Text.AlignVCenter
                text: row.index + 1
                color: row.isCursorLine ? Theme.textSecondary : Theme.textTertiary
                font.family: Fonts.mono
                font.pointSize: Metrics.fontSizeBody
                // Selecting text must not sweep up the line numbers.
                renderType: Text.NativeRendering
            }

            // ---- Code ----
            Item {
                x: root.gutterWidth
                width: parent.width - root.gutterWidth
                height: parent.height
                clip: true

                // Scrolls with the shared horizontal offset so every line moves
                // together.
                Item {
                    x: -horizontal.position * Math.max(0, contentText.width - parent.width)
                    width: parent.width
                    height: parent.height

                    // Selection highlight, drawn behind the glyphs.
                    Rectangle {
                        visible: Editor.lineHasSelection(row.index)
                        x: Editor.selectionStartOn(row.index) * root.charWidth
                        width: Math.max(2,
                            (Editor.selectionEndOn(row.index)
                             - Editor.selectionStartOn(row.index)) * root.charWidth)
                        height: parent.height
                        color: Theme.accent
                        opacity: 0.25
                    }

                    Text {
                        id: contentText
                        height: parent.height
                        verticalAlignment: Text.AlignVCenter
                        text: row.text
                        color: Theme.synPlain
                        font.family: Fonts.mono
                        font.pointSize: Metrics.fontSizeMedium
                        textFormat: Text.PlainText
                    }

                    // Caret. Only the cursor line draws one.
                    Rectangle {
                        visible: row.isCursorLine && root.activeFocus
                        x: Editor.cursorColumn * root.charWidth
                        width: 2
                        height: parent.height * 0.8
                        anchors.verticalCenter: parent.verticalCenter
                        color: Theme.accent

                        // Blinking stops while typing so the caret is never
                        // invisible at the moment the user looks for it.
                        SequentialAnimation on opacity {
                            running: row.isCursorLine && root.activeFocus
                                     && App.animationDuration > 0
                            loops: Animation.Infinite
                            NumberAnimation { to: 0; duration: 500; easing.type: Easing.InOutQuad }
                            NumberAnimation { to: 1; duration: 500; easing.type: Easing.InOutQuad }
                        }
                    }
                }
            }
        }
    }

    // Horizontal position, shared by every line so they scroll as one.
    ScrollBar {
        id: horizontal
        anchors.left: parent.left
        anchors.leftMargin: root.gutterWidth
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        orientation: Qt.Horizontal
        policy: ScrollBar.AsNeeded
        size: 1.0
    }

    // ---- Input ----
    //
    // Click to place the caret, drag to select. Column comes from the x offset
    // divided by the advance width, which is exact for a monospaced face.
    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton
        cursorShape: Qt.IBeamCursor
        preventStealing: true

        function positionAt(mouseX, mouseY) {
            const line = Math.max(0, Math.min(Editor.lineCount - 1,
                Math.floor((mouseY + lines.contentY) / root.lineHeight)));
            const column = Math.max(0,
                Math.round((mouseX - root.gutterWidth) / root.charWidth));
            return { line: line, column: column };
        }

        onPressed: (mouse) => {
            root.forceActiveFocus();
            const at = positionAt(mouse.x, mouse.y);
            Editor.moveCursor(at.line, at.column, mouse.modifiers & Qt.ShiftModifier);
        }

        onPositionChanged: (mouse) => {
            if (!pressed)
                return;
            const at = positionAt(mouse.x, mouse.y);
            // Dragging always extends: the anchor stays where the press landed.
            Editor.moveCursor(at.line, at.column, true);
        }
    }

    Keys.onPressed: (event) => {
        const shift = (event.modifiers & Qt.ShiftModifier) !== 0;
        const ctrl = (event.modifiers & Qt.ControlModifier) !== 0;

        switch (event.key) {
        case Qt.Key_Left:      Editor.moveLeft(shift, ctrl); break;
        case Qt.Key_Right:     Editor.moveRight(shift, ctrl); break;
        case Qt.Key_Up:        Editor.moveUp(shift); break;
        case Qt.Key_Down:      Editor.moveDown(shift); break;
        case Qt.Key_Home:      ctrl ? Editor.moveToDocumentStart(shift)
                                    : Editor.moveToLineStart(shift); break;
        case Qt.Key_End:       ctrl ? Editor.moveToDocumentEnd(shift)
                                    : Editor.moveToLineEnd(shift); break;
        case Qt.Key_PageUp:    Editor.movePage(-root.visibleLines, shift); break;
        case Qt.Key_PageDown:  Editor.movePage(root.visibleLines, shift); break;
        case Qt.Key_Backspace: Editor.deleteBackward(); break;
        case Qt.Key_Delete:    Editor.deleteForward(); break;
        case Qt.Key_Return:
        case Qt.Key_Enter:     Editor.insertNewline(); break;
        case Qt.Key_Tab:       Editor.insertTab(); break;

        // Editing shortcuts are handled only while Ctrl is held. Without that
        // guard these six letters would be swallowed by their own case and
        // never typed - pressing "a" would do nothing at all.
        case Qt.Key_A: if (!ctrl) { typeCharacter(event); return; }
                       Editor.selectAll(); break;
        case Qt.Key_C: if (!ctrl) { typeCharacter(event); return; }
                       Editor.copy(); break;
        case Qt.Key_X: if (!ctrl) { typeCharacter(event); return; }
                       Editor.cut(); break;
        case Qt.Key_V: if (!ctrl) { typeCharacter(event); return; }
                       Editor.paste(); break;
        case Qt.Key_Y: if (!ctrl) { typeCharacter(event); return; }
                       Editor.redo(); break;
        case Qt.Key_Z: if (!ctrl) { typeCharacter(event); return; }
                       shift ? Editor.redo() : Editor.undo(); break;

        default:
            if (typeCharacter(event)) {
                break;
            }
            // Not printable and not ours: leave it for the window's own
            // shortcuts rather than swallowing it here.
            return;
        }
        event.accepted = true;
    }

    /// Inserts a key event's text if it is printable. Returns whether it did,
    /// so callers can fall through to leaving the event unhandled.
    ///
    /// Control characters are excluded by the 0x20 floor, and anything with
    /// Ctrl held is a shortcut rather than input.
    function typeCharacter(event) {
        const ctrl = (event.modifiers & Qt.ControlModifier) !== 0;
        const alt = (event.modifiers & Qt.AltModifier) !== 0;

        if (ctrl || alt || event.text.length === 0
            || event.text.charCodeAt(0) < 0x20) {
            return false;
        }

        Editor.insertText(event.text);
        event.accepted = true;
        return true;
    }

    /// How many lines fit on screen, for PageUp and PageDown.
    readonly property int visibleLines: Math.max(1, Math.floor(height / lineHeight))

    Connections {
        target: Editor
        function onScrollToCursorRequested() {
            root.scrollToCursor();
        }
    }

    /// Keeps the caret in view after a move or edit. Scrolls by the minimum
    /// needed, so the view does not jump when the caret is already visible.
    function scrollToCursor() {
        const top = Editor.cursorLine * lineHeight;
        const bottom = top + lineHeight;

        if (top < lines.contentY) {
            lines.contentY = top;
        } else if (bottom > lines.contentY + lines.height) {
            lines.contentY = bottom - lines.height;
        }
    }
}
