#include "editor/FoldModel.h"

#include "editor/TextDocument.h"

#include <algorithm>

namespace keys::editor {
namespace {

/// The indentation of a line, or -1 for a blank one.
///
/// Blank lines have no indentation of their own and must not end a region: a
/// blank line between two statements inside a function is part of the function,
/// not the end of it.
int indentOf(const QString& line)
{
    int indent = 0;
    for (const QChar character : line) {
        if (character == QLatin1Char(' ')) {
            ++indent;
        } else if (character == QLatin1Char('\t')) {
            indent += 4;   // a tab counts as one indent step for comparison
        } else {
            return indent;
        }
    }
    return -1;   // nothing but whitespace
}

} // namespace

void FoldModel::rebuild(const TextDocument& document)
{
    m_regions.clear();

    const int lines = document.lineCount();
    for (int line = 0; line < lines; ++line) {
        const int indent = indentOf(document.line(line));
        if (indent < 0) {
            continue;
        }

        // Look ahead for a run of lines indented deeper than this one. Blank
        // lines are skipped rather than ending the run, and the region ends at
        // the last non-blank line that is still deeper.
        int last = line;
        for (int next = line + 1; next < lines; ++next) {
            const int nextIndent = indentOf(document.line(next));
            if (nextIndent < 0) {
                continue;   // blank: might still be inside the block
            }
            if (nextIndent <= indent) {
                break;
            }
            last = next;
        }

        if (last > line) {
            m_regions.push_back(FoldRegion{line, last});
        }
    }

    // A region that no longer exists cannot stay folded; one that does keeps
    // its state, so editing elsewhere does not spring the whole file open.
    std::vector<int> stillFolded;
    for (const int start : m_folded) {
        if (regionAt(start)) {
            stillFolded.push_back(start);
        }
    }
    m_folded = std::move(stillFolded);
}

const FoldRegion* FoldModel::regionAt(int line) const
{
    for (const FoldRegion& region : m_regions) {
        if (region.startLine == line) {
            return &region;
        }
    }
    return nullptr;
}

bool FoldModel::isFolded(int line) const
{
    return std::find(m_folded.begin(), m_folded.end(), line) != m_folded.end();
}

bool FoldModel::isHidden(int line) const
{
    for (const int start : m_folded) {
        const FoldRegion* region = regionAt(start);
        if (region && region->contains(line)) {
            return true;
        }
    }
    return false;
}

void FoldModel::toggle(int line)
{
    if (!regionAt(line)) {
        return;   // nothing foldable here
    }

    const auto it = std::find(m_folded.begin(), m_folded.end(), line);
    if (it == m_folded.end()) {
        m_folded.push_back(line);
    } else {
        m_folded.erase(it);
    }
}

void FoldModel::foldAll()
{
    m_folded.clear();
    for (const FoldRegion& region : m_regions) {
        m_folded.push_back(region.startLine);
    }
}

void FoldModel::unfoldAll()
{
    m_folded.clear();
}

int FoldModel::visibleLineCount(int documentLineCount) const
{
    if (m_folded.empty()) {
        return documentLineCount;
    }

    int visible = 0;
    for (int line = 0; line < documentLineCount; ++line) {
        if (!isHidden(line)) {
            ++visible;
        }
    }
    return visible;
}

int FoldModel::documentLineFor(int visibleRow, int documentLineCount) const
{
    if (m_folded.empty()) {
        return visibleRow;   // the ordinary case, and it must cost nothing
    }

    int seen = 0;
    for (int line = 0; line < documentLineCount; ++line) {
        if (isHidden(line)) {
            continue;
        }
        if (seen == visibleRow) {
            return line;
        }
        ++seen;
    }
    return documentLineCount - 1;
}

void FoldModel::clear()
{
    m_regions.clear();
    m_folded.clear();
}

} // namespace keys::editor
