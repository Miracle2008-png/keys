#pragma once

#include <QString>

#include <vector>

namespace keys::editor {

/// A position in the document: zero-based line and UTF-16 column.
///
/// Columns are indices into the line's QString, not grapheme clusters. That
/// matches Qt's own string model and LSP's default `utf-16` position encoding,
/// so positions cross the editor/language-server boundary without conversion.
struct Position {
    int line = 0;
    int column = 0;

    [[nodiscard]] bool operator==(const Position& other) const = default;

    [[nodiscard]] bool operator<(const Position& other) const
    {
        return line != other.line ? line < other.line : column < other.column;
    }

    [[nodiscard]] bool operator<=(const Position& other) const
    {
        return *this < other || *this == other;
    }
};

/// A half-open span of text: `start` inclusive, `end` exclusive.
struct Range {
    Position start;
    Position end;

    [[nodiscard]] bool isEmpty() const { return start == end; }

    /// Start and end in document order, regardless of which way round they were
    /// given. A selection dragged upwards has its anchor after its cursor.
    [[nodiscard]] Range normalized() const
    {
        return start <= end ? *this : Range{end, start};
    }
};

/// The document's text.
///
/// **Why a piece table.**
/// A typing session is overwhelmingly append-and-insert. A piece table never
/// moves the original text: it keeps the file as one immutable buffer, appends
/// every insertion to a second one, and represents the document as an ordered
/// list of spans into the two. An insert is therefore O(pieces) rather than
/// O(document size), and the original file is never copied. A gap buffer is
/// faster for edits clustered in one place but degrades badly when the cursor
/// jumps; a rope is asymptotically better on enormous files but carries far more
/// machinery than an editor of this size needs.
///
/// The table also gives undo almost for free, because a piece list is cheap to
/// snapshot compared with the text it describes.
///
/// **Line index.** Editors address text by line far more often than by offset -
/// rendering, cursors, diagnostics and LSP positions are all line-based. A
/// separate index of line start offsets is maintained alongside the pieces so
/// line lookup is O(log n) rather than a scan.
///
/// This class is deliberately free of cursors, selections and undo: those belong
/// to the document layer above. It knows only how to hold text and answer
/// questions about it.
class TextBuffer {
public:
    TextBuffer();
    explicit TextBuffer(const QString& initialText);

    /// Replaces the entire contents, as when a file is loaded.
    void setText(const QString& text);

    /// The whole document as one string. Materialises every piece, so it is for
    /// saving and testing rather than for rendering.
    [[nodiscard]] QString text() const;

    /// One line, without its terminator.
    [[nodiscard]] QString line(int index) const;

    /// Number of lines. A buffer always has at least one, so an empty document
    /// is one empty line rather than none.
    [[nodiscard]] int lineCount() const;

    /// Length of a line in UTF-16 code units, excluding its terminator.
    [[nodiscard]] int lineLength(int index) const;

    /// Total length in UTF-16 code units, including line terminators.
    [[nodiscard]] int length() const { return m_length; }

    [[nodiscard]] bool isEmpty() const { return m_length == 0; }

    /// Inserts `text` at `position`. Returns where the caret ends up, which is
    /// after the inserted text and accounts for any newlines it contained.
    Position insert(const Position& position, const QString& text);

    /// Removes `range`. Returns its start, which is where the caret belongs
    /// afterwards. An empty or reversed range is normalised first.
    Position remove(const Range& range);

    /// The text within `range`.
    [[nodiscard]] QString textIn(const Range& range) const;

    /// Converts between positions and absolute offsets. Out-of-range input is
    /// clamped to the document rather than being undefined, because callers
    /// routinely compute positions that a concurrent edit has invalidated.
    [[nodiscard]] int offsetOf(const Position& position) const;
    [[nodiscard]] Position positionOf(int offset) const;

    /// Clamps a position to somewhere that exists in this document.
    [[nodiscard]] Position clamp(const Position& position) const;

    /// The end of the document.
    [[nodiscard]] Position endPosition() const;

private:
    /// Which of the two immutable buffers a piece points into.
    enum class Source { Original, Added };

    /// A span of text. Pieces are never mutated, only split and replaced, which
    /// is what makes snapshotting the list cheap.
    struct Piece {
        Source source = Source::Original;
        int start = 0;   ///< offset into the source buffer
        int length = 0;
    };

    /// Rebuilds the line index from the current pieces. O(n) in document size,
    /// so it runs on a full replacement rather than on every edit.
    /// Rebuilds the whole index. Used when the document is replaced wholesale;
    /// an edit updates it in place instead.
    void rebuildLineIndex();

    /// Updates the index for an insertion of `text` at `offset`.
    ///
    /// An insertion adds line starts only for the newlines it contains, and
    /// shifts every later start by its length - both bounded by what actually
    /// changed rather than by the size of the document. Rebuilding instead is
    /// O(document) per keystroke, which is the difference between an editor
    /// that keeps up in a large file and one that does not.
    void updateLineIndexForInsert(int offset, const QString& text);

    /// Updates the index for a removal of `length` characters at `offset`.
    void updateLineIndexForRemove(int offset, int length);

    /// The index of the line containing `offset`. Used by the incremental
    /// updates to find where to splice.
    [[nodiscard]] int lineIndexForOffset(int offset) const;

    /// Locates the piece containing `offset`.
    /// Returns the piece index and how far into it the offset falls.
    [[nodiscard]] std::pair<int, int> pieceAt(int offset) const;

    /// Reads `length` code units starting at `offset`.
    [[nodiscard]] QString read(int offset, int length) const;

    /// The file as loaded. Never modified.
    QString m_original;

    /// Every insertion, appended. Never modified, only grown, so existing pieces
    /// stay valid as more text arrives.
    QString m_added;

    std::vector<Piece> m_pieces;

    /// Absolute offset of each line's first character. Always starts with 0, so
    /// its size is the line count.
    /// Offsets at which each line begins, so a line can be addressed without
    /// walking the pieces. Maintained incrementally: rebuilding it costs a pass
    /// over the whole document, which at 200k lines is well past a frame.
    std::vector<int> m_lineStarts;

    int m_length = 0;
};

} // namespace keys::editor
