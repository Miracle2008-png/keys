#pragma once

#include <QString>

#include <vector>

namespace keys::search {

/// The outcome of matching a query against a candidate.
struct FuzzyResult {
    /// Higher is better. Only meaningful relative to other results for the same
    /// query — there is no absolute scale.
    int score = 0;

    /// Which characters of the candidate the query matched, for highlighting.
    /// Empty when the query is empty.
    std::vector<int> positions;

    [[nodiscard]] bool matched() const { return score > 0; }
};

/// Subsequence matching with quality scoring, as used by quick open.
///
/// **What "fuzzy" means here.** Every character of the query must appear in the
/// candidate, in order, but not necessarily adjacently. So `fzm` matches
/// `FuzzyMatch.h`. This is the behaviour every editor's quick open has converged
/// on, because it lets a developer type the shape of a name rather than its
/// spelling.
///
/// **Scoring is what makes it usable.** A plain subsequence test would rank
/// `src/foundation/zoo/mixer.cpp` alongside `FuzzyMatch.h` for `fzm`. The score
/// therefore rewards the things that indicate the user meant this file:
/// consecutive matches, matches at word boundaries (camelCase humps, after `_`,
/// `-` or `/`), a match at the very start, and matching in the file name rather
/// than deep in a directory. Distance from the start is penalised so an early
/// match beats a late one.
///
/// **Speed matters more than perfection.** This runs against every file in the
/// project on each keystroke, so it is a single left-to-right pass with a small
/// lookahead rather than the full dynamic-programming optimum. The greedy pass
/// occasionally picks a worse alignment than a Smith-Waterman search would, but
/// it is O(n) rather than O(n·m) and the difference is not visible in a ranked
/// list — whereas a 50-millisecond pause per keystroke would be.
class FuzzyMatch {
public:
    /// Matches `query` against `candidate`, case-insensitively.
    ///
    /// An empty query matches everything with a score of 1, so an unfiltered
    /// quick open shows the whole list.
    [[nodiscard]] static FuzzyResult match(const QString& query, const QString& candidate);

    /// Matches against a path, scoring the file name far more heavily than the
    /// directories leading to it.
    ///
    /// Typing `main` should surface `src/main.cpp` above
    /// `src/main/util/helper.cpp`, even though the latter also contains "main" —
    /// the user is naming a file, not a folder.
    [[nodiscard]] static FuzzyResult matchPath(const QString& query, const QString& path);

private:
    /// True if `candidate[index]` begins a word: the first character, or one
    /// following a separator, or an uppercase letter after a lowercase one.
    [[nodiscard]] static bool isWordBoundary(const QString& candidate, int index);
};

} // namespace keys::search
