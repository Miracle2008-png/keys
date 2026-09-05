#include "editor/TextBuffer.h"

#include <algorithm>

namespace keys::editor {

TextBuffer::TextBuffer()
{
    rebuildLineIndex();
}

TextBuffer::TextBuffer(const QString& initialText)
{
    setText(initialText);
}

void TextBuffer::setText(const QString& text)
{
    m_original = text;
    m_added.clear();
    m_pieces.clear();

    if (!text.isEmpty()) {
        m_pieces.push_back(Piece{Source::Original, 0, static_cast<int>(text.size())});
    }

    m_length = static_cast<int>(text.size());
    rebuildLineIndex();
}

QString TextBuffer::text() const
{
    QString result;
    result.reserve(m_length);
    for (const Piece& piece : m_pieces) {
        const QString& source =
            piece.source == Source::Original ? m_original : m_added;
        result.append(QStringView(source).mid(piece.start, piece.length));
    }
    return result;
}

void TextBuffer::rebuildLineIndex()
{
    m_lineStarts.clear();

    // A document always has a first line, even when empty: an editor with zero
    // lines has nowhere to put the caret.
    m_lineStarts.push_back(0);

    int offset = 0;
    for (const Piece& piece : m_pieces) {
        const QString& source =
            piece.source == Source::Original ? m_original : m_added;

        for (int i = 0; i < piece.length; ++i) {
            if (source.at(piece.start + i) == QLatin1Char('\n')) {
                // The next line begins after the newline.
                m_lineStarts.push_back(offset + i + 1);
            }
        }
        offset += piece.length;
    }
}

int TextBuffer::lineCount() const
{
    return static_cast<int>(m_lineStarts.size());
}

QString TextBuffer::line(int index) const
{
    if (index < 0 || index >= lineCount()) {
        return {};
    }

    const int start = m_lineStarts.at(static_cast<size_t>(index));
    const int end = index + 1 < lineCount()
                        // -1 to drop the newline the next line's start follows.
                        ? m_lineStarts.at(static_cast<size_t>(index) + 1) - 1
                        : m_length;

    return read(start, std::max(0, end - start));
}

int TextBuffer::lineLength(int index) const
{
    if (index < 0 || index >= lineCount()) {
        return 0;
    }

    const int start = m_lineStarts.at(static_cast<size_t>(index));
    const int end = index + 1 < lineCount()
                        ? m_lineStarts.at(static_cast<size_t>(index) + 1) - 1
                        : m_length;
    return std::max(0, end - start);
}

std::pair<int, int> TextBuffer::pieceAt(int offset) const
{
    int consumed = 0;
    for (size_t i = 0; i < m_pieces.size(); ++i) {
        const int end = consumed + m_pieces[i].length;

        // `<` rather than `<=` so an offset landing exactly on a boundary
        // belongs to the following piece, which keeps splits unambiguous.
        if (offset < end) {
            return {static_cast<int>(i), offset - consumed};
        }
        consumed = end;
    }

    // Past the end: the insertion point after the final piece.
    return {static_cast<int>(m_pieces.size()), 0};
}

QString TextBuffer::read(int offset, int length) const
{
    if (length <= 0 || offset < 0 || offset >= m_length) {
        return {};
    }
    length = std::min(length, m_length - offset);

    QString result;
    result.reserve(length);

    auto [pieceIndex, withinPiece] = pieceAt(offset);
    int remaining = length;

    while (remaining > 0 && pieceIndex < static_cast<int>(m_pieces.size())) {
        const Piece& piece = m_pieces[static_cast<size_t>(pieceIndex)];
        const QString& source =
            piece.source == Source::Original ? m_original : m_added;

        const int available = piece.length - withinPiece;
        const int take = std::min(available, remaining);

        result.append(QStringView(source).mid(piece.start + withinPiece, take));

        remaining -= take;
        ++pieceIndex;
        withinPiece = 0;   // subsequent pieces are read from their start
    }

    return result;
}

int TextBuffer::offsetOf(const Position& position) const
{
    const Position clamped = clamp(position);
    return m_lineStarts.at(static_cast<size_t>(clamped.line)) + clamped.column;
}

Position TextBuffer::positionOf(int offset) const
{
    offset = std::clamp(offset, 0, m_length);

    // The first line start greater than `offset` is the line after the one we
    // want, so step back one. Binary search keeps this O(log n).
    const auto it = std::upper_bound(m_lineStarts.begin(), m_lineStarts.end(), offset);
    const int line = static_cast<int>(std::distance(m_lineStarts.begin(), it)) - 1;

    return Position{line, offset - m_lineStarts.at(static_cast<size_t>(line))};
}

Position TextBuffer::clamp(const Position& position) const
{
    Position result = position;
    result.line = std::clamp(result.line, 0, lineCount() - 1);
    result.column = std::clamp(result.column, 0, lineLength(result.line));
    return result;
}

Position TextBuffer::endPosition() const
{
    const int last = lineCount() - 1;
    return Position{last, lineLength(last)};
}

Position TextBuffer::insert(const Position& position, const QString& text)
{
    if (text.isEmpty()) {
        return clamp(position);
    }

    const int offset = offsetOf(position);

    // Every insertion is appended to the added buffer and never moved, so
    // existing pieces keep pointing at valid text.
    const int addedStart = static_cast<int>(m_added.size());
    m_added.append(text);

    const Piece inserted{Source::Added, addedStart, static_cast<int>(text.size())};

    auto [pieceIndex, withinPiece] = pieceAt(offset);

    if (pieceIndex >= static_cast<int>(m_pieces.size())) {
        // Appending at the very end.
        m_pieces.push_back(inserted);
    } else if (withinPiece == 0) {
        // Landing on a boundary needs no split.
        m_pieces.insert(m_pieces.begin() + pieceIndex, inserted);
    } else {
        // Split the piece and put the insertion between the halves.
        const Piece original = m_pieces[static_cast<size_t>(pieceIndex)];
        const Piece head{original.source, original.start, withinPiece};
        const Piece tail{original.source,
                         original.start + withinPiece,
                         original.length - withinPiece};

        m_pieces[static_cast<size_t>(pieceIndex)] = head;
        m_pieces.insert(m_pieces.begin() + pieceIndex + 1, {inserted, tail});
    }

    m_length += static_cast<int>(text.size());

    // The line index only changes from the insertion point onward, but the
    // pieces have shifted, so it is rebuilt. Milestone 15 revisits this if
    // profiling shows it matters on large files.
    rebuildLineIndex();

    return positionOf(offset + static_cast<int>(text.size()));
}

Position TextBuffer::remove(const Range& range)
{
    const Range ordered = range.normalized();
    const int start = offsetOf(ordered.start);
    const int end = offsetOf(ordered.end);

    if (start >= end) {
        return positionOf(start);
    }

    // Rebuild the piece list, keeping only the parts outside the removed span.
    // Pieces are immutable, so trimming means emitting a shorter piece rather
    // than editing one in place.
    std::vector<Piece> rebuilt;
    rebuilt.reserve(m_pieces.size() + 1);

    int offset = 0;
    for (const Piece& piece : m_pieces) {
        const int pieceStart = offset;
        const int pieceEnd = offset + piece.length;
        offset = pieceEnd;

        // Entirely outside the removed span.
        if (pieceEnd <= start || pieceStart >= end) {
            rebuilt.push_back(piece);
            continue;
        }

        // The part before the removal.
        if (pieceStart < start) {
            rebuilt.push_back(Piece{piece.source, piece.start, start - pieceStart});
        }

        // The part after it.
        if (pieceEnd > end) {
            const int skip = end - pieceStart;
            rebuilt.push_back(
                Piece{piece.source, piece.start + skip, piece.length - skip});
        }
    }

    m_pieces = std::move(rebuilt);
    m_length -= end - start;
    rebuildLineIndex();

    return positionOf(start);
}

QString TextBuffer::textIn(const Range& range) const
{
    const Range ordered = range.normalized();
    const int start = offsetOf(ordered.start);
    const int end = offsetOf(ordered.end);
    return read(start, end - start);
}

} // namespace keys::editor
