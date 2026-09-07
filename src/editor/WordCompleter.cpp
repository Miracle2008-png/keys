#include "editor/WordCompleter.h"

#include "editor/TextDocument.h"

#include <QHash>

#include <algorithm>
#include <cmath>

namespace keys::editor {
namespace {

bool isWordStart(QChar character)
{
    return character.isLetter() || character == QLatin1Char('_');
}

bool isWordPart(QChar character)
{
    return character.isLetterOrNumber() || character == QLatin1Char('_');
}

/// Buffer words are ranked by distance, keywords and types by a fixed bucket.
///
/// The buckets are chosen so a nearby buffer word outranks a keyword while a
/// distant one does not: what the user just wrote is usually what they mean,
/// but a word four hundred lines away is a worse guess than the language's own
/// vocabulary.
constexpr int kKeywordRank = 60;
constexpr int kTypeRank = 70;

} // namespace

QString WordCompleter::prefixAt(const QString& lineText, int column)
{
    const int clamped = std::clamp(column, 0, static_cast<int>(lineText.size()));

    int start = clamped;
    while (start > 0 && isWordPart(lineText.at(start - 1))) {
        --start;
    }

    // A run that begins with a digit is a number, not an identifier being
    // typed, and completing `42` against every word starting with 4 is noise.
    if (start < clamped && !isWordStart(lineText.at(start))) {
        return {};
    }

    return lineText.mid(start, clamped - start);
}

std::vector<std::pair<QString, int>> WordCompleter::wordsIn(const QStringList& lines)
{
    // The last line a word appeared on, so ranking can measure distance from
    // the caret. Last rather than first: a word being used now matters more
    // than where it was introduced.
    QHash<QString, int> lastSeen;

    for (int lineNumber = 0; lineNumber < lines.size(); ++lineNumber) {
        const QString& line = lines.at(lineNumber);
        const int length = static_cast<int>(line.size());

        int i = 0;
        while (i < length) {
            if (!isWordStart(line.at(i))) {
                ++i;
                continue;
            }

            int j = i;
            while (j < length && isWordPart(line.at(j))) {
                ++j;
            }

            // Single characters are skipped. `i`, `x` and `n` are the most
            // common words in any file and the least useful to complete.
            if (j - i > 1) {
                lastSeen.insert(line.mid(i, j - i), lineNumber);
            }
            i = j;
        }
    }

    std::vector<std::pair<QString, int>> words;
    words.reserve(static_cast<size_t>(lastSeen.size()));
    for (auto it = lastSeen.cbegin(); it != lastSeen.cend(); ++it) {
        words.emplace_back(it.key(), it.value());
    }
    return words;
}

std::vector<WordSuggestion> WordCompleter::suggest(
    const TextDocument& document, int line, int column,
    SyntaxHighlighter::Language language)
{
    const QStringList lines = document.text().split(QLatin1Char('\n'));

    if (line < 0 || line >= lines.size()) {
        return {};
    }

    const QString prefix = prefixAt(lines.at(line), column);
    if (prefix.size() < kMinPrefixLength) {
        return {};
    }

    std::vector<WordSuggestion> suggestions;

    // ---- Words already in the buffer --------------------------------------
    for (const auto& [word, seenOn] : wordsIn(lines)) {
        // The word being typed is not a suggestion for itself.
        if (word == prefix) {
            continue;
        }
        if (!word.startsWith(prefix, Qt::CaseSensitive)) {
            continue;
        }

        suggestions.push_back({word, WordSuggestion::Source::Buffer,
                               std::abs(seenOn - line)});
    }

    // ---- The language's own vocabulary ------------------------------------
    if (language != SyntaxHighlighter::Language::None) {
        const auto add = [&](const QStringList& words,
                             WordSuggestion::Source source, int rank) {
            for (const QString& word : words) {
                if (word == prefix || !word.startsWith(prefix, Qt::CaseSensitive)) {
                    continue;
                }
                suggestions.push_back({word, source, rank});
            }
        };

        add(SyntaxHighlighter::keywordsOf(language),
            WordSuggestion::Source::Keyword, kKeywordRank);
        add(SyntaxHighlighter::typesOf(language),
            WordSuggestion::Source::Type, kTypeRank);
    }

    // A keyword that also appears in the buffer is offered twice. Deduplicated
    // by word first, keeping the better-ranked entry - which is the buffer one
    // when it is nearby, because distance is the more useful ordering.
    //
    // Sorted by word rather than by rank for this pass: std::unique only
    // removes *adjacent* equals, and the same word at two ranks is not adjacent
    // in rank order.
    std::sort(suggestions.begin(), suggestions.end(),
              [](const WordSuggestion& a, const WordSuggestion& b) {
                  if (a.word != b.word) {
                      return a.word < b.word;
                  }
                  return a.rank < b.rank;
              });

    suggestions.erase(
        std::unique(suggestions.begin(), suggestions.end(),
                    [](const WordSuggestion& a, const WordSuggestion& b) {
                        return a.word == b.word;
                    }),
        suggestions.end());

    // Then into the order the popup shows.
    std::sort(suggestions.begin(), suggestions.end(),
              [](const WordSuggestion& a, const WordSuggestion& b) {
                  if (a.rank != b.rank) {
                      return a.rank < b.rank;
                  }
                  return a.word < b.word;
              });

    if (suggestions.size() > static_cast<size_t>(kMaxSuggestions)) {
        suggestions.resize(kMaxSuggestions);
    }
    return suggestions;
}

} // namespace keys::editor
