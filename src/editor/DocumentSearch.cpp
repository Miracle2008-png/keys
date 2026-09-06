#include "editor/DocumentSearch.h"

#include "editor/TextDocument.h"

#include <QRegularExpression>

namespace keys::editor {
namespace {

/// The pattern for a query, honouring the options.
///
/// A plain query is escaped so that characters like `.` and `(` mean
/// themselves - a user typing `foo(` is looking for that text, not writing a
/// regex, and treating it as one would either match the wrong thing or fail to
/// compile.
QRegularExpression buildPattern(const FindOptions& options, bool& valid)
{
    QString pattern = options.regularExpression
                          ? options.query
                          : QRegularExpression::escape(options.query);

    if (options.wholeWord) {
        // Word boundaries around whatever the query became. Applied after
        // escaping so a literal query still gets them.
        pattern = QStringLiteral("\\b(?:") + pattern + QStringLiteral(")\\b");
    }

    QRegularExpression::PatternOptions flags =
        QRegularExpression::UseUnicodePropertiesOption;
    if (!options.caseSensitive) {
        flags |= QRegularExpression::CaseInsensitiveOption;
    }

    QRegularExpression expression(pattern, flags);
    valid = expression.isValid();
    return expression;
}

} // namespace

void DocumentSearch::search(const TextDocument& document, const FindOptions& options)
{
    m_matches.clear();
    m_current = -1;
    m_queryValid = true;

    if (options.query.isEmpty()) {
        return;
    }

    const QRegularExpression pattern = buildPattern(options, m_queryValid);
    if (!m_queryValid) {
        return;   // a half-typed regex; the field says so, nothing matches
    }

    const int lines = document.lineCount();
    for (int line = 0; line < lines; ++line) {
        const QString text = document.line(line);
        if (text.isEmpty()) {
            continue;
        }

        QRegularExpressionMatchIterator it = pattern.globalMatch(text);
        while (it.hasNext()) {
            const QRegularExpressionMatch match = it.next();

            // A pattern that can match nothing - `a*`, say - would otherwise
            // report one empty hit per character and never advance.
            if (match.capturedLength() == 0) {
                continue;
            }

            Range hit;
            hit.start = Position{line, static_cast<int>(match.capturedStart())};
            hit.end = Position{line, static_cast<int>(match.capturedEnd())};
            m_matches.push_back(hit);
        }
    }

    if (!m_matches.empty()) {
        m_current = 0;
    }
}

void DocumentSearch::clear()
{
    m_matches.clear();
    m_current = -1;
    m_queryValid = true;
}

Range DocumentSearch::current() const
{
    if (m_current < 0 || m_current >= static_cast<int>(m_matches.size())) {
        return Range{};
    }
    return m_matches[static_cast<size_t>(m_current)];
}

void DocumentSearch::next()
{
    if (m_matches.empty()) {
        return;
    }
    m_current = (m_current + 1) % static_cast<int>(m_matches.size());
}

void DocumentSearch::previous()
{
    if (m_matches.empty()) {
        return;
    }
    const int size = static_cast<int>(m_matches.size());
    m_current = (m_current - 1 + size) % size;
}

void DocumentSearch::selectNearest(const Position& from)
{
    if (m_matches.empty()) {
        m_current = -1;
        return;
    }

    // The first match at or after the caret. Opening the find bar should
    // continue from where the user is looking, not send them to the top of the
    // file and make them find their place again.
    for (size_t i = 0; i < m_matches.size(); ++i) {
        if (!(m_matches[i].start < from)) {
            m_current = static_cast<int>(i);
            return;
        }
    }

    // Every match is above the caret, so the next one going forward is the
    // first - the same wrap that `next()` performs.
    m_current = 0;
}

} // namespace keys::editor
