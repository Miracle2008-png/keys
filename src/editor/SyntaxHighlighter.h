#pragma once

#include <QString>
#include <QStringList>

#include <vector>

namespace keys::editor {

/// What a run of characters is.
///
/// These are the categories the design's palette defines, and no more. A
/// finer-grained set would need colours the theme does not have, and inventing
/// them would break the one thing a colour scheme has to do: stay coherent.
enum class TokenKind {
    Plain,
    Keyword,
    Type,
    String,
    Number,
    Comment,
    Function,
    Punctuation,
};

/// One run of characters sharing a kind, as [start, start + length) in UTF-16
/// units - the same coordinates the buffer and LSP use.
struct Token {
    int start = 0;
    int length = 0;
    TokenKind kind = TokenKind::Plain;
};

/// The state a line ends in, so the next line can be highlighted independently.
///
/// This is what makes per-line highlighting correct rather than approximate: a
/// block comment or an unterminated raw string spans lines, and a highlighter
/// that looked at one line alone would colour the rest of the file wrongly.
enum class LineState {
    Normal,
    InBlockComment,
};

/// Colours one line at a time.
///
/// **Per line, on demand.** The editor renders only the lines in its viewport,
/// so highlighting the whole file would be work nobody sees - on a 200,000-line
/// file it would be most of the cost of opening it. Each line is coloured as it
/// is drawn, from the state the previous line ended in.
///
/// **Lexical, not semantic.** This knows that `class` is a keyword and that
/// `"..."` is a string. It does not know whether an identifier names a type or a
/// variable - that needs a compiler, which is what the language server is for.
/// Getting the lexical layer right is most of the visual benefit and costs
/// nothing at startup.
///
/// **A small set of languages, honestly.** A language Keys has no rules for is
/// left plain rather than run through rules that nearly fit: mis-colouring reads
/// as a bug, whereas no colour reads as a file type that is not supported yet.
class SyntaxHighlighter {
public:
    /// What Keys can highlight. Chosen by file extension.
    enum class Language {
        None,
        C,          ///< also C++, which shares the lexical shape
        Python,
        JavaScript, ///< also TypeScript and JSON, close enough lexically
        Rust,
        Go,
        Qml,
        Markdown,
        Shell,
    };

    /// The language for a path, or None when Keys has no rules for it.
    [[nodiscard]] static Language languageForPath(const QString& path);

    explicit SyntaxHighlighter(Language language = Language::None);

    [[nodiscard]] Language language() const { return m_language; }
    void setLanguage(Language language);

    /// Tokenises one line, starting from `incoming` and reporting the state the
    /// line ends in through `outgoing`. Returns no tokens for Language::None,
    /// which the view draws as plain text.
    [[nodiscard]] std::vector<Token> tokenize(const QString& line, LineState incoming,
                                              LineState& outgoing) const;

private:
    /// The keywords of the active language, sorted for lookup.
    [[nodiscard]] const QStringList& keywords() const;
    [[nodiscard]] const QStringList& types() const;

    /// The comment syntax of the active language. Empty means the language has
    /// no comment of that shape.
    [[nodiscard]] QString lineCommentPrefix() const;
    [[nodiscard]] bool hasBlockComments() const;

    Language m_language = Language::None;
};

} // namespace keys::editor
