#pragma once

#include "editor/TextBuffer.h"

#include <QString>

#include <vector>

namespace keys::editor {

class TextDocument;

/// How a find is run. The same three toggles every editor offers, and the same
/// ones `search::SearchOptions` carries for the project-wide case - a find bar
/// and the search panel should not disagree about what "whole word" means.
struct FindOptions {
    QString query;
    bool caseSensitive = false;
    bool wholeWord = false;
    bool regularExpression = false;
};

/// Find and replace within one document.
///
/// **Line by line, not one big string.** Building the whole file into a single
/// QString to run a regex over would allocate a copy of the document on every
/// keystroke in the find field. Matching per line costs nothing extra and keeps
/// a 200,000-line file responsive while the user is still typing the query.
///
/// **Every match, then navigate.** Matches are collected once and then stepped
/// through, so "3 of 47" can be shown and Enter can wrap from the last match to
/// the first. A find that only ever looked forward from the caret could not say
/// how many there were, and a count is most of what makes a find bar useful.
///
/// **Positions, not offsets.** Matches are reported as ranges in line/column
/// terms, the same coordinates the buffer, the caret and the language server
/// use. Anything else would need converting at every use site.
class DocumentSearch {
public:
    /// Recomputes every match in `document`. Safe to call on each keystroke.
    ///
    /// An empty or invalid query clears the results rather than being an error:
    /// a half-typed regex is the normal state of a find field, not a failure.
    void search(const TextDocument& document, const FindOptions& options);

    /// Drops all results, for when the find bar closes.
    void clear();

    [[nodiscard]] const std::vector<Range>& matches() const { return m_matches; }
    [[nodiscard]] int count() const { return static_cast<int>(m_matches.size()); }
    [[nodiscard]] bool isEmpty() const { return m_matches.empty(); }

    /// Which match is current, or -1 when there are none. One-based for display
    /// is the view's business; this is an index.
    [[nodiscard]] int currentIndex() const { return m_current; }

    /// The current match, or an empty range when there is none.
    [[nodiscard]] Range current() const;

    /// Moves to the next or previous match, wrapping at either end. Wrapping is
    /// what people expect from Enter in a find field; stopping at the last
    /// match reads as the find having broken.
    void next();
    void previous();

    /// Selects the first match at or after `from`, so opening the find bar
    /// continues from where the caret already is rather than jumping to the top
    /// of the file.
    void selectNearest(const Position& from);

    /// Whether the query could be compiled. A malformed regex is reported so
    /// the field can say so, rather than silently matching nothing.
    [[nodiscard]] bool isQueryValid() const { return m_queryValid; }

private:
    std::vector<Range> m_matches;
    int m_current = -1;
    bool m_queryValid = true;
};

} // namespace keys::editor
