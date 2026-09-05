#include "search/FuzzyMatch.h"

#include <algorithm>

namespace keys::search {
namespace {

// Scoring weights. Tuned so that the ordering they produce matches what a
// developer expects, which the tests pin down with concrete examples rather than
// asserting the numbers themselves.
constexpr int kBaseMatch = 16;

/// A run of adjacent matches is the strongest signal that the user is typing a
/// real prefix rather than scattered letters.
constexpr int kConsecutiveBonus = 20;

/// Matching the start of a word — `fm` against `FuzzyMatch` — is how people
/// abbreviate, so it is weighted nearly as heavily as adjacency.
constexpr int kWordBoundaryBonus = 18;

/// A match at index 0 is a prefix, which is the clearest intent of all.
constexpr int kStartBonus = 25;

/// Each skipped character costs a little, so an early match outranks a late one.
constexpr int kSkipPenalty = 2;

/// Skipping is capped so a match deep in a very long path is not driven
/// negative: a late match is worse, not disqualifying.
constexpr int kMaxSkipPenalty = 40;

/// An exact case match is a weak signal on top of a case-insensitive one -
/// enough to break a tie, not enough to outrank structure.
constexpr int kCaseBonus = 4;

bool isSeparator(QChar character)
{
    return character == QLatin1Char('/') || character == QLatin1Char('\\')
           || character == QLatin1Char('_') || character == QLatin1Char('-')
           || character == QLatin1Char('.') || character == QLatin1Char(' ');
}


/// Whether `query` from `queryIndex` still exists as a subsequence of
/// `candidate` from `candidateIndex`.
///
/// This is what makes the word-boundary preference safe. Preferring a boundary
/// means jumping forward past earlier occurrences, and a jump that strands the
/// rest of the query turns a real match into no match at all: `view` against
/// `View Split Editor` would take the `E` of `Editor` for its `e` - a boundary -
/// and then find no `w` after it. A character count is not enough, because
/// which characters remain is what decides it.
///
/// Cost is bounded by the tail of the query, and it runs only when a boundary
/// candidate is actually found, so the common path is untouched.
bool remainderFits(const QString& query, int queryIndex,
                   const QString& candidate, int candidateIndex)
{
    int c = candidateIndex;
    for (int q = queryIndex; q < query.size(); ++q) {
        const QChar wanted = query.at(q).toLower();
        while (c < candidate.size() && candidate.at(c).toLower() != wanted) {
            ++c;
        }
        if (c >= candidate.size()) {
            return false;
        }
        ++c;
    }
    return true;
}

} // namespace

bool FuzzyMatch::isWordBoundary(const QString& candidate, int index)
{
    if (index <= 0) {
        return true;
    }

    const QChar previous = candidate.at(index - 1);
    if (isSeparator(previous)) {
        return true;
    }

    // A camelCase hump: an uppercase letter following a lowercase one. This is
    // what lets `fm` find `FuzzyMatch` and `tbm` find `TabBarModel`.
    const QChar current = candidate.at(index);
    return current.isUpper() && previous.isLower();
}

FuzzyResult FuzzyMatch::match(const QString& query, const QString& candidate)
{
    FuzzyResult result;

    // An empty query matches everything, so an unfiltered quick open shows the
    // whole list rather than nothing.
    if (query.isEmpty()) {
        result.score = 1;
        return result;
    }
    if (candidate.isEmpty() || query.size() > candidate.size()) {
        return result;
    }

    result.positions.reserve(static_cast<size_t>(query.size()));

    int score = 0;
    int candidateIndex = 0;
    int previousMatch = -1;

    for (int queryIndex = 0; queryIndex < query.size(); ++queryIndex) {
        const QChar wanted = query.at(queryIndex);
        const QChar wantedLower = wanted.toLower();

        // Find the next occurrence, preferring one that starts a word.
        //
        // The lookahead is what stops the greedy pass making an obviously bad
        // choice: given `fm` against `affirm Match`, taking the first `f` would
        // strand the `m`. Scanning ahead for a word-boundary match recovers the
        // intended alignment without the cost of a full dynamic-programming
        // search.
        //
        // Crucially the preference is only taken when the rest of the query is
        // still reachable from there. A boundary match that consumes too much of
        // the candidate leaves the remainder unsatisfiable: `srcutil` against
        // `src/util.cpp` would otherwise take the `c` of `.cpp` - a boundary,
        // because it follows a dot - and strand the `u`; `view` against
        // `View Split Editor` would take the `E` of `Editor` and strand the `w`.
        int found = -1;
        int firstAny = -1;

        for (int i = candidateIndex; i < candidate.size(); ++i) {
            if (candidate.at(i).toLower() != wantedLower) {
                continue;
            }
            if (firstAny < 0) {
                firstAny = i;
            }
            if (isWordBoundary(candidate, i)
                && remainderFits(query, queryIndex + 1, candidate, i + 1)) {
                found = i;
                break;
            }
            // Bounded so this stays linear overall: past a short window, a
            // word-boundary match is too far away to be what the user meant.
            if (i - firstAny > 24) {
                break;
            }
        }

        if (found < 0) {
            found = firstAny;
        }
        if (found < 0) {
            // A query character is missing entirely: not a match.
            return FuzzyResult{};
        }

        score += kBaseMatch;

        if (found == 0) {
            score += kStartBonus;
        }
        if (isWordBoundary(candidate, found)) {
            score += kWordBoundaryBonus;
        }
        if (previousMatch >= 0 && found == previousMatch + 1) {
            score += kConsecutiveBonus;
        }
        if (candidate.at(found) == wanted) {
            score += kCaseBonus;
        }

        const int skipped = found - std::max(0, previousMatch + 1);
        score -= std::min(skipped * kSkipPenalty, kMaxSkipPenalty);

        result.positions.push_back(found);
        previousMatch = found;
        candidateIndex = found + 1;
    }

    // A negative total would sort below non-matches, so the floor is 1: this did
    // match, however poorly.
    result.score = std::max(1, score);
    return result;
}

FuzzyResult FuzzyMatch::matchPath(const QString& query, const QString& path)
{
    if (query.isEmpty()) {
        FuzzyResult result;
        result.score = 1;
        return result;
    }

    const int lastSeparator = path.lastIndexOf(QLatin1Char('/'));
    const QString fileName = lastSeparator >= 0 ? path.mid(lastSeparator + 1) : path;

    // Try the file name first. The user is naming a file, so a match there is
    // what they meant - `main` should find `src/main.cpp` before
    // `src/main/util/helper.cpp`.
    FuzzyResult onName = match(query, fileName);
    if (onName.matched()) {
        // Shift positions back into the full path's coordinates so the caller
        // can highlight against what it displays.
        if (lastSeparator >= 0) {
            for (int& position : onName.positions) {
                position += lastSeparator + 1;
            }
        }

        // Doubled, so any file-name match outranks any whole-path match. The
        // two scales are otherwise comparable and would interleave.
        onName.score *= 2;
        return onName;
    }

    // Otherwise fall back to the whole path, which is what makes a query like
    // `srcutil` work across a directory boundary.
    return match(query, path);
}

} // namespace keys::search
