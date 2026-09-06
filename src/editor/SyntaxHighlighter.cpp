#include "editor/SyntaxHighlighter.h"

#include <QFileInfo>
#include <QHash>

#include <algorithm>

namespace keys::editor {
namespace {

using Language = SyntaxHighlighter::Language;

/// Keywords per language. Deliberately the reserved words only: adding library
/// names would mean maintaining a list that is always out of date and colouring
/// an identifier the user redefined.
const QStringList& keywordsFor(Language language)
{
    static const QStringList none;

    static const QStringList c = {
        QStringLiteral("alignas"), QStringLiteral("alignof"), QStringLiteral("auto"),
        QStringLiteral("break"), QStringLiteral("case"), QStringLiteral("catch"),
        QStringLiteral("class"), QStringLiteral("concept"), QStringLiteral("const"),
        QStringLiteral("consteval"), QStringLiteral("constexpr"), QStringLiteral("constinit"),
        QStringLiteral("continue"), QStringLiteral("co_await"), QStringLiteral("co_return"),
        QStringLiteral("co_yield"), QStringLiteral("default"), QStringLiteral("delete"),
        QStringLiteral("do"), QStringLiteral("else"), QStringLiteral("enum"),
        QStringLiteral("explicit"), QStringLiteral("export"), QStringLiteral("extern"),
        QStringLiteral("false"), QStringLiteral("final"), QStringLiteral("for"),
        QStringLiteral("friend"), QStringLiteral("goto"), QStringLiteral("if"),
        QStringLiteral("inline"), QStringLiteral("mutable"), QStringLiteral("namespace"),
        QStringLiteral("new"), QStringLiteral("noexcept"), QStringLiteral("nullptr"),
        QStringLiteral("operator"), QStringLiteral("override"), QStringLiteral("private"),
        QStringLiteral("protected"), QStringLiteral("public"), QStringLiteral("register"),
        QStringLiteral("requires"), QStringLiteral("return"), QStringLiteral("sizeof"),
        QStringLiteral("static"), QStringLiteral("static_assert"), QStringLiteral("struct"),
        QStringLiteral("switch"), QStringLiteral("template"), QStringLiteral("this"),
        QStringLiteral("thread_local"), QStringLiteral("throw"), QStringLiteral("true"),
        QStringLiteral("try"), QStringLiteral("typedef"), QStringLiteral("typename"),
        QStringLiteral("union"), QStringLiteral("using"), QStringLiteral("virtual"),
        QStringLiteral("volatile"), QStringLiteral("while"),
    };

    static const QStringList python = {
        QStringLiteral("and"), QStringLiteral("as"), QStringLiteral("assert"),
        QStringLiteral("async"), QStringLiteral("await"), QStringLiteral("break"),
        QStringLiteral("class"), QStringLiteral("continue"), QStringLiteral("def"),
        QStringLiteral("del"), QStringLiteral("elif"), QStringLiteral("else"),
        QStringLiteral("except"), QStringLiteral("False"), QStringLiteral("finally"),
        QStringLiteral("for"), QStringLiteral("from"), QStringLiteral("global"),
        QStringLiteral("if"), QStringLiteral("import"), QStringLiteral("in"),
        QStringLiteral("is"), QStringLiteral("lambda"), QStringLiteral("None"),
        QStringLiteral("nonlocal"), QStringLiteral("not"), QStringLiteral("or"),
        QStringLiteral("pass"), QStringLiteral("raise"), QStringLiteral("return"),
        QStringLiteral("True"), QStringLiteral("try"), QStringLiteral("while"),
        QStringLiteral("with"), QStringLiteral("yield"),
    };

    static const QStringList javascript = {
        QStringLiteral("as"), QStringLiteral("async"), QStringLiteral("await"),
        QStringLiteral("break"), QStringLiteral("case"), QStringLiteral("catch"),
        QStringLiteral("class"), QStringLiteral("const"), QStringLiteral("continue"),
        QStringLiteral("debugger"), QStringLiteral("default"), QStringLiteral("delete"),
        QStringLiteral("do"), QStringLiteral("else"), QStringLiteral("export"),
        QStringLiteral("extends"), QStringLiteral("false"), QStringLiteral("finally"),
        QStringLiteral("for"), QStringLiteral("from"), QStringLiteral("function"),
        QStringLiteral("if"), QStringLiteral("implements"), QStringLiteral("import"),
        QStringLiteral("in"), QStringLiteral("instanceof"), QStringLiteral("interface"),
        QStringLiteral("let"), QStringLiteral("new"), QStringLiteral("null"),
        QStringLiteral("of"), QStringLiteral("return"), QStringLiteral("static"),
        QStringLiteral("super"), QStringLiteral("switch"), QStringLiteral("this"),
        QStringLiteral("throw"), QStringLiteral("true"), QStringLiteral("try"),
        QStringLiteral("type"), QStringLiteral("typeof"), QStringLiteral("undefined"),
        QStringLiteral("var"), QStringLiteral("void"), QStringLiteral("while"),
        QStringLiteral("yield"),
    };

    static const QStringList rust = {
        QStringLiteral("as"), QStringLiteral("async"), QStringLiteral("await"),
        QStringLiteral("break"), QStringLiteral("const"), QStringLiteral("continue"),
        QStringLiteral("crate"), QStringLiteral("dyn"), QStringLiteral("else"),
        QStringLiteral("enum"), QStringLiteral("extern"), QStringLiteral("false"),
        QStringLiteral("fn"), QStringLiteral("for"), QStringLiteral("if"),
        QStringLiteral("impl"), QStringLiteral("in"), QStringLiteral("let"),
        QStringLiteral("loop"), QStringLiteral("match"), QStringLiteral("mod"),
        QStringLiteral("move"), QStringLiteral("mut"), QStringLiteral("pub"),
        QStringLiteral("ref"), QStringLiteral("return"), QStringLiteral("self"),
        QStringLiteral("static"), QStringLiteral("struct"), QStringLiteral("super"),
        QStringLiteral("trait"), QStringLiteral("true"), QStringLiteral("type"),
        QStringLiteral("unsafe"), QStringLiteral("use"), QStringLiteral("where"),
        QStringLiteral("while"),
    };

    static const QStringList go = {
        QStringLiteral("break"), QStringLiteral("case"), QStringLiteral("chan"),
        QStringLiteral("const"), QStringLiteral("continue"), QStringLiteral("default"),
        QStringLiteral("defer"), QStringLiteral("else"), QStringLiteral("fallthrough"),
        QStringLiteral("for"), QStringLiteral("func"), QStringLiteral("go"),
        QStringLiteral("goto"), QStringLiteral("if"), QStringLiteral("import"),
        QStringLiteral("interface"), QStringLiteral("map"), QStringLiteral("package"),
        QStringLiteral("range"), QStringLiteral("return"), QStringLiteral("select"),
        QStringLiteral("struct"), QStringLiteral("switch"), QStringLiteral("type"),
        QStringLiteral("var"),
    };

    static const QStringList qml = {
        QStringLiteral("as"), QStringLiteral("break"), QStringLiteral("case"),
        QStringLiteral("catch"), QStringLiteral("const"), QStringLiteral("continue"),
        QStringLiteral("default"), QStringLiteral("delete"), QStringLiteral("do"),
        QStringLiteral("else"), QStringLiteral("false"), QStringLiteral("for"),
        QStringLiteral("function"), QStringLiteral("if"), QStringLiteral("import"),
        QStringLiteral("in"), QStringLiteral("let"), QStringLiteral("new"),
        QStringLiteral("null"), QStringLiteral("pragma"), QStringLiteral("property"),
        QStringLiteral("readonly"), QStringLiteral("required"), QStringLiteral("return"),
        QStringLiteral("signal"), QStringLiteral("switch"), QStringLiteral("this"),
        QStringLiteral("throw"), QStringLiteral("true"), QStringLiteral("try"),
        QStringLiteral("typeof"), QStringLiteral("var"), QStringLiteral("while"),
    };

    static const QStringList shell = {
        QStringLiteral("case"), QStringLiteral("do"), QStringLiteral("done"),
        QStringLiteral("elif"), QStringLiteral("else"), QStringLiteral("esac"),
        QStringLiteral("export"), QStringLiteral("fi"), QStringLiteral("for"),
        QStringLiteral("function"), QStringLiteral("if"), QStringLiteral("in"),
        QStringLiteral("local"), QStringLiteral("return"), QStringLiteral("then"),
        QStringLiteral("until"), QStringLiteral("while"),
    };

    switch (language) {
    case Language::C:          return c;
    case Language::Python:     return python;
    case Language::JavaScript: return javascript;
    case Language::Rust:       return rust;
    case Language::Go:         return go;
    case Language::Qml:        return qml;
    case Language::Shell:      return shell;
    case Language::Markdown:
    case Language::None:
        break;
    }
    return none;
}

/// Built-in type names, coloured differently from keywords because the design's
/// palette distinguishes them and because it makes a declaration readable at a
/// glance.
const QStringList& typesFor(Language language)
{
    static const QStringList none;

    static const QStringList c = {
        QStringLiteral("bool"), QStringLiteral("char"), QStringLiteral("char8_t"),
        QStringLiteral("char16_t"), QStringLiteral("char32_t"), QStringLiteral("double"),
        QStringLiteral("float"), QStringLiteral("int"), QStringLiteral("int8_t"),
        QStringLiteral("int16_t"), QStringLiteral("int32_t"), QStringLiteral("int64_t"),
        QStringLiteral("long"), QStringLiteral("short"), QStringLiteral("signed"),
        QStringLiteral("size_t"), QStringLiteral("uint8_t"), QStringLiteral("uint16_t"),
        QStringLiteral("uint32_t"), QStringLiteral("uint64_t"), QStringLiteral("unsigned"),
        QStringLiteral("void"), QStringLiteral("wchar_t"),
    };

    static const QStringList rust = {
        QStringLiteral("bool"), QStringLiteral("char"), QStringLiteral("f32"),
        QStringLiteral("f64"), QStringLiteral("i8"), QStringLiteral("i16"),
        QStringLiteral("i32"), QStringLiteral("i64"), QStringLiteral("isize"),
        QStringLiteral("str"), QStringLiteral("String"), QStringLiteral("u8"),
        QStringLiteral("u16"), QStringLiteral("u32"), QStringLiteral("u64"),
        QStringLiteral("usize"), QStringLiteral("Vec"),
    };

    static const QStringList go = {
        QStringLiteral("bool"), QStringLiteral("byte"), QStringLiteral("complex64"),
        QStringLiteral("complex128"), QStringLiteral("error"), QStringLiteral("float32"),
        QStringLiteral("float64"), QStringLiteral("int"), QStringLiteral("int8"),
        QStringLiteral("int16"), QStringLiteral("int32"), QStringLiteral("int64"),
        QStringLiteral("rune"), QStringLiteral("string"), QStringLiteral("uint"),
        QStringLiteral("uintptr"),
    };

    switch (language) {
    case Language::C:    return c;
    case Language::Rust: return rust;
    case Language::Go:   return go;
    default:             break;
    }
    return none;
}

bool isIdentifierStart(QChar character)
{
    return character.isLetter() || character == QLatin1Char('_');
}

bool isIdentifierPart(QChar character)
{
    return character.isLetterOrNumber() || character == QLatin1Char('_');
}

/// Whether a character can appear in a number literal. Deliberately permissive:
/// hex digits, exponents, separators and suffixes all belong to the literal, and
/// splitting them would produce a run of differently-coloured fragments.
bool isNumberPart(QChar character)
{
    return character.isLetterOrNumber() || character == QLatin1Char('.')
           || character == QLatin1Char('_') || character == QLatin1Char('\'');
}

bool isPunctuation(QChar character)
{
    static const QString punctuation = QStringLiteral("{}()[];,.:=+-*/%<>!&|^~?#@");
    return punctuation.contains(character);
}

} // namespace

SyntaxHighlighter::SyntaxHighlighter(Language language) : m_language(language) {}

void SyntaxHighlighter::setLanguage(Language language)
{
    m_language = language;
}

const QStringList& SyntaxHighlighter::keywords() const
{
    return keywordsFor(m_language);
}

const QStringList& SyntaxHighlighter::types() const
{
    return typesFor(m_language);
}

QString SyntaxHighlighter::lineCommentPrefix() const
{
    switch (m_language) {
    case Language::C:
    case Language::JavaScript:
    case Language::Rust:
    case Language::Go:
    case Language::Qml:
        return QStringLiteral("//");
    case Language::Python:
    case Language::Shell:
        return QStringLiteral("#");
    case Language::Markdown:
    case Language::None:
        break;
    }
    return QString();
}

bool SyntaxHighlighter::hasBlockComments() const
{
    switch (m_language) {
    case Language::C:
    case Language::JavaScript:
    case Language::Rust:
    case Language::Go:
    case Language::Qml:
        return true;
    default:
        return false;
    }
}

SyntaxHighlighter::Language SyntaxHighlighter::languageForPath(const QString& path)
{
    // Only extensions with real rules. An unmapped one returns None and the view
    // draws plain text - mis-colouring reads as a bug, no colour reads as a file
    // type Keys does not know yet.
    static const QHash<QString, Language> byExtension = {
        {QStringLiteral("c"), Language::C},
        {QStringLiteral("h"), Language::C},
        {QStringLiteral("cc"), Language::C},
        {QStringLiteral("cpp"), Language::C},
        {QStringLiteral("cxx"), Language::C},
        {QStringLiteral("hpp"), Language::C},
        {QStringLiteral("hxx"), Language::C},
        {QStringLiteral("py"), Language::Python},
        {QStringLiteral("js"), Language::JavaScript},
        {QStringLiteral("jsx"), Language::JavaScript},
        {QStringLiteral("ts"), Language::JavaScript},
        {QStringLiteral("tsx"), Language::JavaScript},
        {QStringLiteral("json"), Language::JavaScript},
        {QStringLiteral("rs"), Language::Rust},
        {QStringLiteral("go"), Language::Go},
        {QStringLiteral("qml"), Language::Qml},
        {QStringLiteral("md"), Language::Markdown},
        {QStringLiteral("sh"), Language::Shell},
        {QStringLiteral("bash"), Language::Shell},
    };

    return byExtension.value(QFileInfo(path).suffix().toLower(), Language::None);
}

std::vector<Token> SyntaxHighlighter::tokenize(const QString& line, LineState incoming,
                                               LineState& outgoing) const
{
    outgoing = incoming;

    std::vector<Token> tokens;
    if (m_language == Language::None || line.isEmpty()) {
        return tokens;
    }

    const int length = static_cast<int>(line.size());
    int i = 0;

    // A block comment carried in from the previous line runs until it closes.
    if (incoming == LineState::InBlockComment) {
        const int close = line.indexOf(QStringLiteral("*/"));
        if (close < 0) {
            tokens.push_back({0, length, TokenKind::Comment});
            return tokens;   // still open at end of line
        }
        tokens.push_back({0, close + 2, TokenKind::Comment});
        outgoing = LineState::Normal;
        i = close + 2;
    }

    const QString linePrefix = lineCommentPrefix();
    const QStringList& keywordList = keywords();
    const QStringList& typeList = types();

    // Markdown is handled separately: it has no keywords, and what matters is
    // its line-level structure.
    if (m_language == Language::Markdown) {
        const QString trimmed = line.trimmed();
        if (trimmed.startsWith(QLatin1Char('#'))) {
            tokens.push_back({0, length, TokenKind::Keyword});
        } else if (trimmed.startsWith(QLatin1String("```"))) {
            tokens.push_back({0, length, TokenKind::Comment});
        } else if (trimmed.startsWith(QLatin1String("- "))
                   || trimmed.startsWith(QLatin1String("* "))
                   || trimmed.startsWith(QLatin1Char('>'))) {
            const int indent = static_cast<int>(line.size() - line.trimmed().size());
            tokens.push_back({indent, 1, TokenKind::Punctuation});
        }
        return tokens;
    }

    while (i < length) {
        const QChar character = line.at(i);

        // Line comment: everything to the end.
        if (!linePrefix.isEmpty() && line.mid(i, linePrefix.size()) == linePrefix) {
            tokens.push_back({i, length - i, TokenKind::Comment});
            break;
        }

        // Block comment opening.
        if (hasBlockComments() && character == QLatin1Char('/')
            && i + 1 < length && line.at(i + 1) == QLatin1Char('*')) {
            const int close = line.indexOf(QStringLiteral("*/"), i + 2);
            if (close < 0) {
                tokens.push_back({i, length - i, TokenKind::Comment});
                outgoing = LineState::InBlockComment;
                break;
            }
            tokens.push_back({i, close + 2 - i, TokenKind::Comment});
            i = close + 2;
            continue;
        }

        // Strings. The escape handling matters: "a\"b" is one string, and a
        // highlighter that stopped at the escaped quote would colour the rest of
        // the line wrongly.
        if (character == QLatin1Char('"') || character == QLatin1Char('\'')
            || (m_language == Language::JavaScript && character == QLatin1Char('`'))) {
            const QChar quote = character;
            int j = i + 1;
            while (j < length) {
                if (line.at(j) == QLatin1Char('\\')) {
                    j += 2;   // skip the escaped character
                    continue;
                }
                if (line.at(j) == quote) {
                    ++j;
                    break;
                }
                ++j;
            }
            tokens.push_back({i, std::min(j, length) - i, TokenKind::String});
            i = j;
            continue;
        }

        // Numbers, including hex and floats.
        if (character.isDigit()) {
            int j = i;
            while (j < length && isNumberPart(line.at(j))) {
                ++j;
            }
            tokens.push_back({i, j - i, TokenKind::Number});
            i = j;
            continue;
        }

        // Identifiers: keyword, type, function call, or plain.
        if (isIdentifierStart(character)) {
            int j = i;
            while (j < length && isIdentifierPart(line.at(j))) {
                ++j;
            }
            const QString word = line.mid(i, j - i);

            TokenKind kind = TokenKind::Plain;
            if (keywordList.contains(word)) {
                kind = TokenKind::Keyword;
            } else if (typeList.contains(word)) {
                kind = TokenKind::Type;
            } else {
                // A call, if the next non-space character opens a paren. This is
                // a heuristic, and a cheap one - but it is right far more often
                // than not, and it is what makes code scannable.
                int k = j;
                while (k < length && line.at(k).isSpace()) {
                    ++k;
                }
                if (k < length && line.at(k) == QLatin1Char('(')) {
                    kind = TokenKind::Function;
                }
            }

            if (kind != TokenKind::Plain) {
                tokens.push_back({i, j - i, kind});
            }
            i = j;
            continue;
        }

        if (isPunctuation(character)) {
            tokens.push_back({i, 1, TokenKind::Punctuation});
            ++i;
            continue;
        }

        ++i;
    }

    return tokens;
}

} // namespace keys::editor
