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

    /// `#include`, `#define`, and the rest of the C preprocessor. Its own kind
    /// because it is not the language proper - it reads as scaffolding around
    /// the code, and colouring it as a keyword makes a header block look like
    /// a wall of control flow.
    Preprocessor,

    /// `true`, `nullptr`, `None`, and screaming-case names. A literal that is
    /// not a number or a string still reads as a value rather than an action,
    /// and separating it is most of what makes a condition scannable.
    Constant,

    /// Operators, as distinct from brackets and separators. Kept apart from
    /// Punctuation so structure stays quiet while the arithmetic and logic in
    /// a line stand out.
    Operator,

    /// A markup element name, and the attribute names inside it. HTML and XML
    /// have no keywords; the tag *is* the structure, so it is what has to be
    /// visible.
    Tag,
    Attribute,
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

    /// Inside a `<!-- -->`. Markup comments do not nest and do not share the
    /// block-comment terminator, so they need their own state rather than
    /// borrowing one that closes on `*/`.
    InMarkupComment,

    /// Inside a tag whose `>` has not arrived yet - an element with enough
    /// attributes to wrap is ordinary in HTML.
    InTag,
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
        JavaScript, ///< also JSON, close enough lexically
        TypeScript, ///< JavaScript plus the type-level keywords
        Rust,
        Go,
        Qml,
        Markdown,
        Shell,
        Java,
        Ruby,
        Html,       ///< also XML: the same tag-and-attribute shape
        Css,
        Yaml,
        Toml,       ///< also INI, which is a subset of its shape
        Sql,
        CMake,

        // Curly-brace languages. Each gets its own entry rather than sharing
        // C's: the lexical shape is close, but the keyword sets are not, and
        // colouring `func` or `fun` as an identifier in a file full of them is
        // the difference between highlighting and decoration.
        CSharp,
        Swift,
        Php,        ///< the code inside the tags; the markup around it is not
        Kotlin,
        Dart,
        Scala,

        // Scripting languages whose comment and string rules differ from the
        // C family's.
        Lua,        ///< `--` comments and `--[[ ]]` blocks
        Perl,
        R,
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

    /// The line comment marker for the active language, empty where it has
    /// none. Public because commenting a selection is an editing command, not
    /// only a detail of tokenising.
    [[nodiscard]] QString lineCommentPrefix() const;

private:
    /// The keywords of the active language, sorted for lookup.
    [[nodiscard]] const QStringList& keywords() const;
    [[nodiscard]] const QStringList& types() const;

    /// The comment syntax of the active language.
    [[nodiscard]] bool hasBlockComments() const;

    /// Whether identifiers are matched without regard to case. SQL keywords are
    /// written both ways in real code and colouring only one is worse than
    /// colouring neither.
    [[nodiscard]] bool isCaseInsensitive() const;

    /// Languages whose structure is markup rather than statements. These take
    /// their own path through tokenize; running them through the identifier
    /// loop would colour attribute values as if they were code.
    [[nodiscard]] bool isMarkup() const;

    /// Tokenises markup: `<tag attribute="value">`, with entities and comments.
    void tokenizeMarkup(const QString& line, LineState incoming, LineState& outgoing,
                        std::vector<Token>& tokens) const;

    /// Tokenises CSS, where a name before a colon is a property and the text
    /// before a brace is a selector - neither of which is a keyword.
    void tokenizeCss(const QString& line, LineState incoming, LineState& outgoing,
                     std::vector<Token>& tokens) const;

    Language m_language = Language::None;
};

} // namespace keys::editor
