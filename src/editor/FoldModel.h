#pragma once

#include <QString>

#include <vector>

namespace keys::editor {

class TextDocument;

/// One foldable region: the line carrying the marker, and the last line hidden
/// when it is collapsed.
struct FoldRegion {
    int startLine = 0;   ///< the line with the opening brace or the parent indent
    int endLine = 0;     ///< the last line inside the region

    [[nodiscard]] bool contains(int line) const
    {
        return line > startLine && line <= endLine;
    }
};

/// Which regions of a document can fold, and which are folded.
///
/// **From indentation, not from a parser.** A language server offers folding
/// ranges and they are better, but they arrive late, only for languages with a
/// server, and never for the file someone opened before the server started.
/// Indentation is available immediately for every language, and it is right for
/// the overwhelming majority of code - a block that is indented deeper than its
/// opening line is exactly what people fold. When a server does offer ranges
/// they can replace these without the view knowing.
///
/// **Recomputed on edit, not maintained.** Keeping fold regions correct through
/// arbitrary edits means mapping every insertion and deletion onto every
/// region, which is a great deal of bookkeeping to get subtly wrong. Rebuilding
/// costs one pass over the line lengths, which is fast enough that the editor
/// does it on change and never has stale regions.
class FoldModel {
public:
    /// Recomputes every foldable region. Folded lines that still exist stay
    /// folded, so an edit elsewhere in the file does not spring the file open.
    void rebuild(const TextDocument& document);

    [[nodiscard]] const std::vector<FoldRegion>& regions() const { return m_regions; }

    /// The region starting exactly at `line`, or nullptr. This is what the
    /// gutter asks to decide whether to draw a marker.
    [[nodiscard]] const FoldRegion* regionAt(int line) const;

    [[nodiscard]] bool isFolded(int line) const;

    /// Whether `line` is inside a folded region, and so not drawn. A line
    /// inside two nested folds is hidden by either.
    [[nodiscard]] bool isHidden(int line) const;

    void toggle(int line);
    void foldAll();
    void unfoldAll();

    /// How many lines are actually drawn, and which document line a given
    /// visible row corresponds to. The view works in visible rows; everything
    /// else in the editor works in document lines.
    [[nodiscard]] int visibleLineCount(int documentLineCount) const;
    [[nodiscard]] int documentLineFor(int visibleRow, int documentLineCount) const;

    void clear();

private:
    std::vector<FoldRegion> m_regions;

    /// The start lines of folded regions. Held separately from the regions so
    /// that a rebuild can restore them by line number.
    std::vector<int> m_folded;
};

} // namespace keys::editor
