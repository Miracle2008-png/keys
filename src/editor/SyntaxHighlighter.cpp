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


    static const QStringList java = {
        QStringLiteral("abstract"), QStringLiteral("assert"), QStringLiteral("break"),
        QStringLiteral("case"), QStringLiteral("catch"), QStringLiteral("class"),
        QStringLiteral("continue"), QStringLiteral("default"), QStringLiteral("do"),
        QStringLiteral("else"), QStringLiteral("enum"), QStringLiteral("extends"),
        QStringLiteral("final"), QStringLiteral("finally"), QStringLiteral("for"),
        QStringLiteral("goto"), QStringLiteral("if"), QStringLiteral("implements"),
        QStringLiteral("import"), QStringLiteral("instanceof"), QStringLiteral("interface"),
        QStringLiteral("native"), QStringLiteral("new"), QStringLiteral("package"),
        QStringLiteral("private"), QStringLiteral("protected"), QStringLiteral("public"),
        QStringLiteral("return"), QStringLiteral("static"), QStringLiteral("strictfp"),
        QStringLiteral("super"), QStringLiteral("switch"), QStringLiteral("synchronized"),
        QStringLiteral("this"), QStringLiteral("throw"), QStringLiteral("throws"),
        QStringLiteral("transient"), QStringLiteral("try"), QStringLiteral("volatile"),
        QStringLiteral("while"), QStringLiteral("var"), QStringLiteral("record"),
        QStringLiteral("sealed"), QStringLiteral("permits"), QStringLiteral("yield"),
    };

    static const QStringList ruby = {
        QStringLiteral("alias"), QStringLiteral("and"), QStringLiteral("begin"),
        QStringLiteral("break"), QStringLiteral("case"), QStringLiteral("class"),
        QStringLiteral("def"), QStringLiteral("defined?"), QStringLiteral("do"),
        QStringLiteral("else"), QStringLiteral("elsif"), QStringLiteral("end"),
        QStringLiteral("ensure"), QStringLiteral("for"), QStringLiteral("if"),
        QStringLiteral("in"), QStringLiteral("module"), QStringLiteral("next"),
        QStringLiteral("not"), QStringLiteral("or"), QStringLiteral("redo"),
        QStringLiteral("rescue"), QStringLiteral("retry"), QStringLiteral("return"),
        QStringLiteral("self"), QStringLiteral("super"), QStringLiteral("then"),
        QStringLiteral("undef"), QStringLiteral("unless"), QStringLiteral("until"),
        QStringLiteral("when"), QStringLiteral("while"), QStringLiteral("yield"),
        QStringLiteral("require"), QStringLiteral("require_relative"), QStringLiteral("attr_accessor"),
        QStringLiteral("attr_reader"), QStringLiteral("attr_writer"), QStringLiteral("lambda"),
        QStringLiteral("proc"),
    };

    // Written both cases in real code; matched case-insensitively.
    static const QStringList sql = {
        QStringLiteral("select"), QStringLiteral("from"), QStringLiteral("where"),
        QStringLiteral("insert"), QStringLiteral("update"), QStringLiteral("delete"),
        QStringLiteral("create"), QStringLiteral("drop"), QStringLiteral("alter"),
        QStringLiteral("table"), QStringLiteral("index"), QStringLiteral("view"),
        QStringLiteral("join"), QStringLiteral("inner"), QStringLiteral("left"),
        QStringLiteral("right"), QStringLiteral("outer"), QStringLiteral("full"),
        QStringLiteral("on"), QStringLiteral("group"), QStringLiteral("by"),
        QStringLiteral("order"), QStringLiteral("having"), QStringLiteral("limit"),
        QStringLiteral("offset"), QStringLiteral("union"), QStringLiteral("all"),
        QStringLiteral("distinct"), QStringLiteral("as"), QStringLiteral("into"),
        QStringLiteral("values"), QStringLiteral("set"), QStringLiteral("primary"),
        QStringLiteral("key"), QStringLiteral("foreign"), QStringLiteral("references"),
        QStringLiteral("constraint"), QStringLiteral("unique"), QStringLiteral("not"),
        QStringLiteral("null"), QStringLiteral("default"), QStringLiteral("cascade"),
        QStringLiteral("and"), QStringLiteral("or"), QStringLiteral("in"),
        QStringLiteral("exists"), QStringLiteral("between"), QStringLiteral("like"),
        QStringLiteral("is"), QStringLiteral("case"), QStringLiteral("when"),
        QStringLiteral("then"), QStringLiteral("else"), QStringLiteral("end"),
        QStringLiteral("with"), QStringLiteral("returning"),
    };

    // CMake's commands rather than keywords: the language is almost entirely
    // commands, and colouring only if/foreach would leave a build file flat.
    static const QStringList cmake = {
        QStringLiteral("if"), QStringLiteral("elseif"), QStringLiteral("else"),
        QStringLiteral("endif"), QStringLiteral("foreach"), QStringLiteral("endforeach"),
        QStringLiteral("while"), QStringLiteral("endwhile"), QStringLiteral("function"),
        QStringLiteral("endfunction"), QStringLiteral("macro"), QStringLiteral("endmacro"),
        QStringLiteral("return"), QStringLiteral("break"), QStringLiteral("continue"),
        QStringLiteral("set"), QStringLiteral("unset"), QStringLiteral("list"),
        QStringLiteral("string"), QStringLiteral("file"), QStringLiteral("find_package"),
        QStringLiteral("include"), QStringLiteral("add_subdirectory"), QStringLiteral("add_executable"),
        QStringLiteral("add_library"), QStringLiteral("target_link_libraries"), QStringLiteral("target_include_directories"),
        QStringLiteral("target_compile_definitions"), QStringLiteral("target_compile_features"), QStringLiteral("target_compile_options"),
        QStringLiteral("install"), QStringLiteral("option"), QStringLiteral("project"),
        QStringLiteral("cmake_minimum_required"), QStringLiteral("message"), QStringLiteral("get_target_property"),
        QStringLiteral("set_target_properties"), QStringLiteral("add_custom_command"), QStringLiteral("add_custom_target"),
        QStringLiteral("add_test"), QStringLiteral("enable_testing"), QStringLiteral("configure_file"),
    };

    // YAML has no keywords; these are the literals worth marking as values.
    static const QStringList yaml = {
        QStringLiteral("true"), QStringLiteral("false"), QStringLiteral("null"),
        QStringLiteral("yes"), QStringLiteral("no"), QStringLiteral("on"),
        QStringLiteral("off"),
    };

    static const QStringList toml = {
        QStringLiteral("true"), QStringLiteral("false"),
    };

    // TypeScript is JavaScript plus type-level words. Built from the JS list so
    // the two cannot drift apart.
    static const QStringList typescript = [] {
        QStringList words = javascript;
        words << QStringList{
            QStringLiteral("abstract"), QStringLiteral("any"), QStringLiteral("as"),
            QStringLiteral("asserts"), QStringLiteral("bigint"), QStringLiteral("boolean"),
            QStringLiteral("declare"), QStringLiteral("enum"), QStringLiteral("implements"),
            QStringLiteral("infer"), QStringLiteral("is"), QStringLiteral("keyof"),
            QStringLiteral("namespace"), QStringLiteral("never"), QStringLiteral("number"),
            QStringLiteral("object"), QStringLiteral("override"), QStringLiteral("private"),
            QStringLiteral("protected"), QStringLiteral("public"), QStringLiteral("readonly"),
            QStringLiteral("require"), QStringLiteral("satisfies"), QStringLiteral("string"),
            QStringLiteral("symbol"), QStringLiteral("type"), QStringLiteral("undefined"),
            QStringLiteral("unique"), QStringLiteral("unknown"),
        };
        words.sort();
        return words;
    }();

    switch (language) {
    case Language::C:          return c;
    case Language::Python:     return python;
    case Language::JavaScript: return javascript;
    case Language::Rust:       return rust;
    case Language::Go:         return go;
    case Language::Qml:        return qml;
    case Language::Shell:      return shell;
    case Language::TypeScript: return typescript;
    case Language::Java:       return java;
    case Language::Ruby:       return ruby;
    case Language::Sql:        return sql;
    case Language::CMake:      return cmake;
    case Language::Yaml:       return yaml;
    case Language::Toml:       return toml;
    case Language::Html:
    case Language::Css:
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

    static const QStringList java = {
        QStringLiteral("boolean"), QStringLiteral("byte"), QStringLiteral("char"),
        QStringLiteral("double"), QStringLiteral("float"), QStringLiteral("int"),
        QStringLiteral("long"), QStringLiteral("short"), QStringLiteral("void"),
        QStringLiteral("String"), QStringLiteral("Integer"), QStringLiteral("Boolean"),
        QStringLiteral("Double"), QStringLiteral("Long"), QStringLiteral("Object"),
        QStringLiteral("List"), QStringLiteral("Map"), QStringLiteral("Set"),
    };

    switch (language) {
    case Language::C:    return c;
    case Language::Rust: return rust;
    case Language::Go:   return go;
    case Language::Java: return java;
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

/// Brackets and separators: the scaffolding of a line rather than its content.
bool isPunctuation(QChar character)
{
    static const QString punctuation = QStringLiteral("{}()[];,.@");
    return punctuation.contains(character);
}

/// Operators, kept apart from punctuation so that structure stays quiet while
/// the arithmetic and logic in a line stand out.
bool isOperator(QChar character)
{
    static const QString operators = QStringLiteral("=+-*/%<>!&|^~?:");
    return operators.contains(character);
}

/// A name in SCREAMING_SNAKE_CASE, which by convention across every language
/// here means a constant. Two characters minimum, so `A` and `I` stay plain.
bool looksLikeConstant(const QString& word)
{
    if (word.size() < 2) {
        return false;
    }
    bool hasLetter = false;
    for (const QChar character : word) {
        if (character.isLower()) {
            return false;
        }
        hasLetter = hasLetter || character.isLetter();
    }
    return hasLetter;
}

/// Literals that are values rather than actions. Coloured apart from keywords
/// because a condition reads better when its operands and its operators do not
/// share a colour.
bool isConstantWord(const QString& word, Language language)
{
    static const QStringList shared = {
        QStringLiteral("true"), QStringLiteral("false"), QStringLiteral("null"),
        QStringLiteral("nullptr"), QStringLiteral("None"), QStringLiteral("True"),
        QStringLiteral("False"), QStringLiteral("nil"), QStringLiteral("undefined"),
        QStringLiteral("NULL"), QStringLiteral("self"), QStringLiteral("this"),
        QStringLiteral("yes"), QStringLiteral("no"), QStringLiteral("on"),
        QStringLiteral("off"),
    };
    Q_UNUSED(language);
    return shared.contains(word);
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
    case Language::TypeScript:
    case Language::Java:
    case Language::Css:
        return QStringLiteral("//");
    case Language::Python:
    case Language::Shell:
    case Language::Ruby:
    case Language::Yaml:
    case Language::Toml:
    case Language::CMake:
        return QStringLiteral("#");
    case Language::Sql:
        return QStringLiteral("--");
    case Language::Html:
    case Language::Markdown:
    case Language::None:
        break;
    }
    return QString();
}

bool SyntaxHighlighter::isCaseInsensitive() const
{
    // Only SQL. Everywhere else `If` is an identifier, and colouring it as a
    // keyword would be wrong rather than generous.
    return m_language == Language::Sql;
}

bool SyntaxHighlighter::isMarkup() const
{
    return m_language == Language::Html;
}

bool SyntaxHighlighter::hasBlockComments() const
{
    switch (m_language) {
    case Language::C:
    case Language::JavaScript:
    case Language::Rust:
    case Language::Go:
    case Language::Qml:
    case Language::TypeScript:
    case Language::Java:
    case Language::Css:
    case Language::Sql:
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
        {QStringLiteral("ts"), Language::TypeScript},
        {QStringLiteral("tsx"), Language::TypeScript},
        {QStringLiteral("json"), Language::JavaScript},
        {QStringLiteral("rs"), Language::Rust},
        {QStringLiteral("go"), Language::Go},
        {QStringLiteral("qml"), Language::Qml},
        {QStringLiteral("md"), Language::Markdown},
        {QStringLiteral("sh"), Language::Shell},
        {QStringLiteral("bash"), Language::Shell},
        {QStringLiteral("zsh"), Language::Shell},
        {QStringLiteral("java"), Language::Java},
        {QStringLiteral("rb"), Language::Ruby},
        {QStringLiteral("html"), Language::Html},
        {QStringLiteral("htm"), Language::Html},
        {QStringLiteral("xml"), Language::Html},
        {QStringLiteral("svg"), Language::Html},
        {QStringLiteral("css"), Language::Css},
        {QStringLiteral("scss"), Language::Css},
        {QStringLiteral("yml"), Language::Yaml},
        {QStringLiteral("yaml"), Language::Yaml},
        {QStringLiteral("toml"), Language::Toml},
        {QStringLiteral("ini"), Language::Toml},
        {QStringLiteral("cfg"), Language::Toml},
        {QStringLiteral("sql"), Language::Sql},
        {QStringLiteral("cmake"), Language::CMake},
    };

    // Files whose name carries the type, with no extension to read. CMakeLists
    // is the one a user of this project sees constantly.
    static const QHash<QString, Language> byName = {
        {QStringLiteral("cmakelists.txt"), Language::CMake},
        {QStringLiteral("dockerfile"), Language::Shell},
        {QStringLiteral("makefile"), Language::Shell},
        {QStringLiteral(".gitignore"), Language::Shell},
        {QStringLiteral(".clangd"), Language::Yaml},
        {QStringLiteral(".clang-format"), Language::Yaml},
    };

    if (const auto byFileName =
            byName.constFind(QFileInfo(path).fileName().toLower());
        byFileName != byName.constEnd()) {
        return byFileName.value();
    }

    return byExtension.value(QFileInfo(path).suffix().toLower(), Language::None);
}

/// Markup: `<tag attribute="value">`, comments, and entities.
///
/// Written as its own pass rather than as cases in the identifier loop. In HTML
/// the tag *is* the structure and the text between tags is prose, so running it
/// through rules built for statements would colour ordinary words as if they
/// were code.
void SyntaxHighlighter::tokenizeMarkup(const QString& line, LineState incoming,
                                       LineState& outgoing,
                                       std::vector<Token>& tokens) const
{
    const int length = static_cast<int>(line.size());
    int i = 0;

    // A comment carried in from a previous line runs until it closes.
    if (incoming == LineState::InMarkupComment) {
        const int close = line.indexOf(QStringLiteral("-->"));
        if (close < 0) {
            tokens.push_back({0, length, TokenKind::Comment});
            outgoing = LineState::InMarkupComment;
            return;
        }
        tokens.push_back({0, close + 3, TokenKind::Comment});
        outgoing = LineState::Normal;
        i = close + 3;
    }

    bool inTag = incoming == LineState::InTag;

    while (i < length) {
        const QChar character = line.at(i);

        if (!inTag) {
            if (character == QLatin1Char('<')) {
                // `<!-- ... -->`, which may not close on this line.
                if (line.mid(i, 4) == QLatin1String("<!--")) {
                    const int close = line.indexOf(QStringLiteral("-->"), i + 4);
                    if (close < 0) {
                        tokens.push_back({i, length - i, TokenKind::Comment});
                        outgoing = LineState::InMarkupComment;
                        return;
                    }
                    tokens.push_back({i, close + 3 - i, TokenKind::Comment});
                    i = close + 3;
                    continue;
                }

                // The angle bracket and the element name together, including a
                // closing slash or a `?` for a processing instruction.
                int j = i + 1;
                while (j < length
                       && (line.at(j) == QLatin1Char('/') || line.at(j) == QLatin1Char('!')
                           || line.at(j) == QLatin1Char('?'))) {
                    ++j;
                }
                while (j < length && (isIdentifierPart(line.at(j))
                                      || line.at(j) == QLatin1Char('-')
                                      || line.at(j) == QLatin1Char(':'))) {
                    ++j;
                }
                tokens.push_back({i, j - i, TokenKind::Tag});
                inTag = true;
                i = j;
                continue;
            }

            // An entity is a value, not prose.
            if (character == QLatin1Char('&')) {
                const int close = line.indexOf(QLatin1Char(';'), i);
                if (close > i && close - i < 12) {
                    tokens.push_back({i, close + 1 - i, TokenKind::Constant});
                    i = close + 1;
                    continue;
                }
            }

            // Text content: left plain, which is what prose should be.
            ++i;
            continue;
        }

        // Inside a tag.
        if (character == QLatin1Char('>')) {
            tokens.push_back({i, 1, TokenKind::Tag});
            inTag = false;
            ++i;
            continue;
        }

        if (character == QLatin1Char('/') && i + 1 < length
            && line.at(i + 1) == QLatin1Char('>')) {
            tokens.push_back({i, 2, TokenKind::Tag});
            inTag = false;
            i += 2;
            continue;
        }

        if (character == QLatin1Char('"') || character == QLatin1Char('\'')) {
            const QChar quote = character;
            int j = i + 1;
            while (j < length && line.at(j) != quote) {
                ++j;
            }
            tokens.push_back({i, std::min(j + 1, length) - i, TokenKind::String});
            i = j + 1;
            continue;
        }

        if (isIdentifierStart(character)) {
            int j = i;
            while (j < length && (isIdentifierPart(line.at(j))
                                  || line.at(j) == QLatin1Char('-')
                                  || line.at(j) == QLatin1Char(':'))) {
                ++j;
            }
            tokens.push_back({i, j - i, TokenKind::Attribute});
            i = j;
            continue;
        }

        if (character == QLatin1Char('=')) {
            tokens.push_back({i, 1, TokenKind::Operator});
        }
        ++i;
    }

    // An element with enough attributes to wrap is ordinary; carry the state.
    outgoing = inTag ? LineState::InTag : LineState::Normal;
}

/// CSS: selectors, properties and values.
///
/// The shape is unlike a programming language - a name before a colon is a
/// property, the text before a brace is a selector, and neither is a keyword -
/// so it gets its own pass rather than a keyword list that would fit badly.
void SyntaxHighlighter::tokenizeCss(const QString& line, LineState incoming,
                                    LineState& outgoing,
                                    std::vector<Token>& tokens) const
{
    const int length = static_cast<int>(line.size());
    int i = 0;

    if (incoming == LineState::InBlockComment) {
        const int close = line.indexOf(QStringLiteral("*/"));
        if (close < 0) {
            tokens.push_back({0, length, TokenKind::Comment});
            outgoing = LineState::InBlockComment;
            return;
        }
        tokens.push_back({0, close + 2, TokenKind::Comment});
        outgoing = LineState::Normal;
        i = close + 2;
    }

    // Whether a name on this line is a property or part of a selector. A colon
    // puts us in a value until the declaration ends.
    bool inValue = false;

    while (i < length) {
        const QChar character = line.at(i);

        if (character == QLatin1Char('/') && i + 1 < length
            && line.at(i + 1) == QLatin1Char('*')) {
            const int close = line.indexOf(QStringLiteral("*/"), i + 2);
            if (close < 0) {
                tokens.push_back({i, length - i, TokenKind::Comment});
                outgoing = LineState::InBlockComment;
                return;
            }
            tokens.push_back({i, close + 2 - i, TokenKind::Comment});
            i = close + 2;
            continue;
        }

        if (character == QLatin1Char('"') || character == QLatin1Char('\'')) {
            const QChar quote = character;
            int j = i + 1;
            while (j < length && line.at(j) != quote) {
                ++j;
            }
            tokens.push_back({i, std::min(j + 1, length) - i, TokenKind::String});
            i = j + 1;
            continue;
        }

        // `#fff`, `#1a2b3c` - a colour is a value, and reads as one. Outside a
        // declaration the same syntax is an id selector.
        if (character == QLatin1Char('#') && i + 1 < length
            && line.at(i + 1).isLetterOrNumber()) {
            int j = i + 1;
            while (j < length && line.at(j).isLetterOrNumber()) {
                ++j;
            }
            tokens.push_back({i, j - i, inValue ? TokenKind::Number : TokenKind::Tag});
            i = j;
            continue;
        }

        if (character.isDigit()
            || (character == QLatin1Char('-') && i + 1 < length
                && line.at(i + 1).isDigit())) {
            int j = i + 1;
            while (j < length && (line.at(j).isLetterOrNumber()
                                  || line.at(j) == QLatin1Char('.')
                                  || line.at(j) == QLatin1Char('%'))) {
                ++j;
            }
            tokens.push_back({i, j - i, TokenKind::Number});
            i = j;
            continue;
        }

        // `@media`, `@import` - the at-rules, which are the closest CSS has to
        // keywords.
        if (character == QLatin1Char('@')) {
            int j = i + 1;
            while (j < length && isIdentifierPart(line.at(j))) {
                ++j;
            }
            tokens.push_back({i, j - i, TokenKind::Keyword});
            i = j;
            continue;
        }

        // `.card` is one selector, not a stray dot followed by a name: the
        // leading punctuation is part of what the reader is looking at, so it
        // is consumed with it and coloured like the `#id` form beside it.
        if (character == QLatin1Char('.') && i + 1 < length
            && isIdentifierStart(line.at(i + 1))) {
            int j = i + 1;
            while (j < length && (isIdentifierPart(line.at(j))
                                  || line.at(j) == QLatin1Char('-'))) {
                ++j;
            }
            tokens.push_back({i, j - i, TokenKind::Tag});
            i = j;
            continue;
        }

        if (isIdentifierStart(character) || character == QLatin1Char('-')) {
            int j = i;
            while (j < length && (isIdentifierPart(line.at(j))
                                  || line.at(j) == QLatin1Char('-'))) {
                ++j;
            }
            if (j == i) {
                ++j;
            }
            tokens.push_back({i, j - i,
                              inValue ? TokenKind::Constant : TokenKind::Attribute});
            i = j;
            continue;
        }

        if (character == QLatin1Char(':')) {
            inValue = true;
            tokens.push_back({i, 1, TokenKind::Operator});
            ++i;
            continue;
        }

        if (character == QLatin1Char(';') || character == QLatin1Char('{')
            || character == QLatin1Char('}')) {
            inValue = false;
            tokens.push_back({i, 1, TokenKind::Punctuation});
            ++i;
            continue;
        }

        if (isPunctuation(character)) {
            tokens.push_back({i, 1, TokenKind::Punctuation});
        }
        ++i;
    }

    outgoing = LineState::Normal;
}

std::vector<Token> SyntaxHighlighter::tokenize(const QString& line, LineState incoming,
                                               LineState& outgoing) const
{
    outgoing = incoming;

    std::vector<Token> tokens;
    if (m_language == Language::None || line.isEmpty()) {
        return tokens;
    }

    // Markup and stylesheets have a shape statements do not; each has its own
    // pass rather than a keyword list bolted onto a loop built for code.
    if (isMarkup()) {
        tokenizeMarkup(line, incoming, outgoing, tokens);
        return tokens;
    }
    if (m_language == Language::Css) {
        tokenizeCss(line, incoming, outgoing, tokens);
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
    // its structure - both the line-level kind and the inline spans, which the
    // first version of this ignored entirely. A document is mostly prose, so a
    // highlighter that only marks headings leaves nearly every line plain.
    if (m_language == Language::Markdown) {
        const QString trimmed = line.trimmed();
        const int indent = static_cast<int>(line.size() - line.trimmed().size());

        // A fenced block owns its lines. Everything inside is code in another
        // language, and marking its punctuation as Markdown's would be wrong.
        if (trimmed.startsWith(QLatin1String("```"))
            || trimmed.startsWith(QLatin1String("~~~"))) {
            tokens.push_back({0, length, TokenKind::Comment});
            return tokens;
        }

        // A heading is the line. Bold and links inside one are not marked
        // separately: the heading already reads as the strongest thing there.
        if (trimmed.startsWith(QLatin1Char('#'))) {
            tokens.push_back({0, length, TokenKind::Keyword});
            return tokens;
        }

        // A horizontal rule, before the bullet check - `---` and `***` both
        // start like list markers.
        if (trimmed.size() >= 3
            && (trimmed.count(QLatin1Char('-')) == trimmed.size()
                || trimmed.count(QLatin1Char('*')) == trimmed.size()
                || trimmed.count(QLatin1Char('_')) == trimmed.size())) {
            tokens.push_back({indent, static_cast<int>(trimmed.size()),
                              TokenKind::Punctuation});
            return tokens;
        }

        // The line's marker: a bullet, an ordered number, a quote, or a table
        // row. Marked first so the inline pass below does not treat a leading
        // `*` as the start of emphasis.
        int inlineStart = indent;

        if (trimmed.startsWith(QLatin1Char('>'))) {
            tokens.push_back({indent, 1, TokenKind::Punctuation});
            inlineStart = indent + 1;
        } else if (trimmed.startsWith(QLatin1String("- "))
                   || trimmed.startsWith(QLatin1String("* "))
                   || trimmed.startsWith(QLatin1String("+ "))) {
            tokens.push_back({indent, 1, TokenKind::Punctuation});
            inlineStart = indent + 1;
        } else if (trimmed.at(0).isDigit()) {
            // `1.` or `1)` - an ordered marker, which the first version missed
            // entirely, so every numbered list read as prose.
            int digits = 0;
            while (digits < trimmed.size() && trimmed.at(digits).isDigit()) {
                ++digits;
            }
            if (digits < trimmed.size()
                && (trimmed.at(digits) == QLatin1Char('.')
                    || trimmed.at(digits) == QLatin1Char(')'))) {
                tokens.push_back({indent, digits + 1, TokenKind::Number});
                inlineStart = indent + digits + 1;
            }
        }

        // A table row: the pipes are the structure.
        if (trimmed.startsWith(QLatin1Char('|'))) {
            for (int at = inlineStart; at < length; ++at) {
                if (line.at(at) == QLatin1Char('|')) {
                    tokens.push_back({at, 1, TokenKind::Punctuation});
                }
            }
        }

        // Inline spans. Scanned once, left to right, so an unclosed marker
        // cannot swallow the rest of the line - a `*` in prose is common, and
        // treating it as the start of emphasis that never ends would colour
        // everything after it.
        int at = inlineStart;
        while (at < length) {
            const QChar character = line.at(at);

            // `code`, and ``code with a backtick``.
            if (character == QLatin1Char('`')) {
                int ticks = 0;
                while (at + ticks < length && line.at(at + ticks) == QLatin1Char('`')) {
                    ++ticks;
                }
                const QString fence(ticks, QLatin1Char('`'));
                const int close = line.indexOf(fence, at + ticks);
                if (close > 0) {
                    tokens.push_back({at, close + ticks - at, TokenKind::String});
                    at = close + ticks;
                    continue;
                }
                at += ticks;
                continue;
            }

            // **bold**, __bold__, *italic*, _italic_.
            if (character == QLatin1Char('*') || character == QLatin1Char('_')) {
                const int run = (at + 1 < length && line.at(at + 1) == character) ? 2 : 1;
                const QString marker(run, character);
                const int close = line.indexOf(marker, at + run);
                if (close > 0) {
                    tokens.push_back({at, close + run - at,
                                      run == 2 ? TokenKind::Keyword : TokenKind::Type});
                    at = close + run;
                    continue;
                }
                at += run;
                continue;
            }

            // [text](url) and ![alt](src). The url is what the reader needs to
            // pick out; the text is prose and stays prose.
            if (character == QLatin1Char('[')) {
                const int textEnd = line.indexOf(QLatin1Char(']'), at);
                if (textEnd > 0 && textEnd + 1 < length
                    && line.at(textEnd + 1) == QLatin1Char('(')) {
                    const int urlEnd = line.indexOf(QLatin1Char(')'), textEnd + 1);
                    if (urlEnd > 0) {
                        tokens.push_back({at, textEnd - at + 1, TokenKind::Punctuation});
                        tokens.push_back({textEnd + 1, urlEnd - textEnd,
                                          TokenKind::Constant});
                        at = urlEnd + 1;
                        continue;
                    }
                }
            }

            ++at;
        }

        return tokens;
    }

    // A preprocessor directive owns its line from the `#` to the end of the
    // word. Recognised before the loop because it is only a directive in the
    // leading position - a `#` anywhere else is an operator or a comment.
    if (m_language == Language::C) {
        int lead = 0;
        while (lead < length && line.at(lead).isSpace()) {
            ++lead;
        }
        if (lead < length && line.at(lead) == QLatin1Char('#')) {
            int j = lead + 1;
            while (j < length && line.at(j).isSpace()) {
                ++j;
            }
            while (j < length && isIdentifierPart(line.at(j))) {
                ++j;
            }
            tokens.push_back({lead, j - lead, TokenKind::Preprocessor});
            i = j;
        }
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

            const Qt::CaseSensitivity sensitivity =
                isCaseInsensitive() ? Qt::CaseInsensitive : Qt::CaseSensitive;

            TokenKind kind = TokenKind::Plain;
            if (isConstantWord(word, m_language)) {
                // Before the keyword test: `true` and `nullptr` are reserved
                // words in most of these languages, but they read as values.
                kind = TokenKind::Constant;
            } else if (keywordList.contains(word, sensitivity)) {
                kind = TokenKind::Keyword;
            } else if (typeList.contains(word, sensitivity)) {
                kind = TokenKind::Type;
            } else if (looksLikeConstant(word)) {
                kind = TokenKind::Constant;
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

        // Operators run together: `!=`, `&&`, `->`, `<=>` are each one thing,
        // and colouring them character by character would flicker.
        if (isOperator(character)) {
            int j = i;
            while (j < length && isOperator(line.at(j))) {
                ++j;
            }
            tokens.push_back({i, j - i, TokenKind::Operator});
            i = j;
            continue;
        }

        ++i;
    }

    return tokens;
}

} // namespace keys::editor
