#pragma once

#include "editor/SyntaxHighlighter.h"

#include <QString>
#include <QStringList>

#include <vector>

namespace keys::editor {

class TextDocument;

/// One suggestion, and why it is being offered.
struct WordSuggestion {
    QString word;

    /// Where it came from, so the popup can say. A keyword the language defines
    /// and a word the user typed three lines up deserve different weight, and
    /// hiding the difference would make the list feel arbitrary.
    enum class Source {
        Buffer,     ///< a word already in this file
        Keyword,    ///< a reserved word of the language
        Type,       ///< a built-in type name
    };
    Source source = Source::Buffer;

    /// Lower sorts first. Distance from the caret for buffer words, a fixed
    /// bucket for the rest.
    int rank = 0;
};

/// Completion without a language server.
///
/// **Why this exists.** Completion in Keys came only from a language server, so
/// on a machine with no clangd, no pyright and no gopls - which is most
/// machines, most of the time - typing offered nothing at all. A word completer
/// is not a substitute for a server that understands types and scope, but it is
/// the difference between an editor that helps and one that does not.
///
/// **It never guesses at meaning.** Every suggestion is a word that already
/// exists: in this buffer, or in the language's own reserved words. Nothing is
/// inferred, so nothing can be confidently wrong - which is the failure mode
/// that makes an autocomplete worse than none.
///
/// **Nearby beats far away.** A word used two lines up is far more likely to be
/// the one wanted than the same word four hundred lines away, so buffer words
/// are ranked by distance from the caret.
class WordCompleter {
public:
    /// How many suggestions to return at most. A popup longer than this is not
    /// read, it is scrolled past.
    static constexpr int kMaxSuggestions = 40;

    /// The shortest prefix worth completing. One character matches most of the
    /// file and the popup becomes noise.
    static constexpr int kMinPrefixLength = 2;

    /// The word being typed at `line`/`column`, or empty when the caret is not
    /// in one. Static so it can be used to decide whether to complete at all
    /// without building a completer.
    [[nodiscard]] static QString prefixAt(const QString& lineText, int column);

    /// Suggestions for the prefix at the caret, best first.
    ///
    /// `language` supplies the keyword and type lists; None means buffer words
    /// only, which is right for a file Keys has no rules for.
    [[nodiscard]] static std::vector<WordSuggestion> suggest(
        const TextDocument& document, int line, int column,
        SyntaxHighlighter::Language language);

    /// Every distinct identifier-shaped word in the text, with the line it was
    /// last seen on. Exposed for testing and for callers that want the raw set.
    [[nodiscard]] static std::vector<std::pair<QString, int>> wordsIn(
        const QStringList& lines);
};

} // namespace keys::editor
