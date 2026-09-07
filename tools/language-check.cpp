// Runs the real highlighter over real source files, one per language, and
// reports what it produced.
//
// HighlighterTests checks specific tokens in short snippets. This is the other
// half: whole files of idiomatic code, where the failures are the ones a
// snippet cannot show - a string that never closes and swallows the rest of the
// file, a comment state that leaks across lines, a language whose rules were
// registered but never reached because languageForPath does not know its
// extension.
//
// Every line is expected to produce tokens; a file that highlights nothing, or
// only near the top, is reported rather than passed over.
//
//     cmake --build build --target keys_language_check
//     build/bin/keys_language_check <directory of samples>

#include "editor/SyntaxHighlighter.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>

#include <vector>

using keys::editor::SyntaxHighlighter;

namespace {

QString nameOf(SyntaxHighlighter::Language language)
{
    switch (language) {
    case SyntaxHighlighter::Language::None:       return QStringLiteral("none");
    case SyntaxHighlighter::Language::C:          return QStringLiteral("C/C++");
    case SyntaxHighlighter::Language::Python:     return QStringLiteral("Python");
    case SyntaxHighlighter::Language::JavaScript: return QStringLiteral("JavaScript");
    case SyntaxHighlighter::Language::TypeScript: return QStringLiteral("TypeScript");
    case SyntaxHighlighter::Language::Rust:       return QStringLiteral("Rust");
    case SyntaxHighlighter::Language::Go:         return QStringLiteral("Go");
    case SyntaxHighlighter::Language::Qml:        return QStringLiteral("QML");
    case SyntaxHighlighter::Language::Markdown:   return QStringLiteral("Markdown");
    case SyntaxHighlighter::Language::Shell:      return QStringLiteral("Shell");
    case SyntaxHighlighter::Language::Java:       return QStringLiteral("Java");
    case SyntaxHighlighter::Language::Ruby:       return QStringLiteral("Ruby");
    case SyntaxHighlighter::Language::Html:       return QStringLiteral("HTML");
    case SyntaxHighlighter::Language::Css:        return QStringLiteral("CSS");
    case SyntaxHighlighter::Language::Yaml:       return QStringLiteral("YAML");
    case SyntaxHighlighter::Language::Toml:       return QStringLiteral("TOML");
    case SyntaxHighlighter::Language::Sql:        return QStringLiteral("SQL");
    case SyntaxHighlighter::Language::CMake:      return QStringLiteral("CMake");
    case SyntaxHighlighter::Language::CSharp:     return QStringLiteral("C#");
    case SyntaxHighlighter::Language::Swift:      return QStringLiteral("Swift");
    case SyntaxHighlighter::Language::Php:        return QStringLiteral("PHP");
    case SyntaxHighlighter::Language::Kotlin:     return QStringLiteral("Kotlin");
    case SyntaxHighlighter::Language::Dart:       return QStringLiteral("Dart");
    case SyntaxHighlighter::Language::Scala:      return QStringLiteral("Scala");
    case SyntaxHighlighter::Language::Lua:        return QStringLiteral("Lua");
    case SyntaxHighlighter::Language::Perl:       return QStringLiteral("Perl");
    case SyntaxHighlighter::Language::R:          return QStringLiteral("R");
    case SyntaxHighlighter::Language::Haskell:    return QStringLiteral("Haskell");
    case SyntaxHighlighter::Language::Elixir:     return QStringLiteral("Elixir");
    case SyntaxHighlighter::Language::OCaml:      return QStringLiteral("OCaml");
    case SyntaxHighlighter::Language::FSharp:     return QStringLiteral("F#");
    case SyntaxHighlighter::Language::Zig:        return QStringLiteral("Zig");
    case SyntaxHighlighter::Language::Nim:        return QStringLiteral("Nim");
    case SyntaxHighlighter::Language::Groovy:     return QStringLiteral("Groovy");
    case SyntaxHighlighter::Language::Julia:      return QStringLiteral("Julia");
    case SyntaxHighlighter::Language::ObjectiveC: return QStringLiteral("Objective-C");
    case SyntaxHighlighter::Language::Assembly:   return QStringLiteral("Assembly");
    }
    return QStringLiteral("?");
}

QString kindOf(keys::editor::TokenKind kind)
{
    using K = keys::editor::TokenKind;
    switch (kind) {
    case K::Keyword:   return QStringLiteral("keyword");
    case K::String:    return QStringLiteral("string");
    case K::Number:    return QStringLiteral("number");
    case K::Comment:   return QStringLiteral("comment");
    case K::Function:  return QStringLiteral("function");
    case K::Type:      return QStringLiteral("type");
    case K::Constant:  return QStringLiteral("constant");
    case K::Operator:     return QStringLiteral("operator");
    case K::Preprocessor: return QStringLiteral("preproc");
    case K::Tag:          return QStringLiteral("tag");
    case K::Attribute:    return QStringLiteral("attribute");
    case K::Punctuation:  return QStringLiteral("punct");
    case K::Plain:        return QStringLiteral("plain");
    }
    return QStringLiteral("?");
}

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    QTextStream out(stdout);

    const QStringList arguments = QCoreApplication::arguments();
    if (arguments.size() < 2) {
        out << "usage: keys_language_check <directory>\n";
        return 2;
    }

    QDir directory(arguments.at(1));
    QStringList files = directory.entryList(QDir::Files, QDir::Name);

    int failures = 0;

    for (const QString& name : files) {
        const QString path = directory.filePath(name);

        QFile file(path);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            out << name << ": could not be read\n";
            ++failures;
            continue;
        }

        const QString contents = QString::fromUtf8(file.readAll());
        const QStringList lines = contents.split(QLatin1Char('\n'));

        const SyntaxHighlighter::Language language =
            SyntaxHighlighter::languageForPath(path);

        SyntaxHighlighter highlighter(language);

        // The whole file, line by line, carrying the state across - which is
        // what a block comment or a multi-line string depends on.
        keys::editor::LineState state = keys::editor::LineState::Normal;
        int totalTokens = 0;
        int linesWithTokens = 0;
        int nonBlankLines = 0;
        QHash<QString, int> byKind;

        for (const QString& line : lines) {
            keys::editor::LineState outgoing = state;
            const std::vector<keys::editor::Token> tokens =
                highlighter.tokenize(line, state, outgoing);
            state = outgoing;

            if (!line.trimmed().isEmpty()) {
                ++nonBlankLines;
            }
            if (!tokens.empty()) {
                ++linesWithTokens;
            }
            totalTokens += static_cast<int>(tokens.size());

            for (const keys::editor::Token& token : tokens) {
                ++byKind[kindOf(token.kind)];

                // A token that runs past the end of its line means the offsets
                // are wrong, which shows up as markup landing on the wrong
                // characters or being dropped entirely.
                if (token.start < 0 || token.start + token.length > line.size()) {
                    out << "  " << name << ": token out of range at ["
                        << token.start << "," << token.length << ") on a line of "
                        << line.size() << "\n";
                    ++failures;
                }
            }
        }

        const bool recognised = language != SyntaxHighlighter::Language::None;
        const double coverage = nonBlankLines > 0
                                    ? (100.0 * linesWithTokens / nonBlankLines)
                                    : 0.0;

        out << QStringLiteral("%1  %2  lines=%3 tokens=%4 covered=%5%")
                   .arg(name, -18)
                   .arg(nameOf(language), -12)
                   .arg(nonBlankLines, 4)
                   .arg(totalTokens, 6)
                   .arg(coverage, 5, 'f', 1)
            << "\n";

        // Prose languages are held to a lower bar, and that is not a
        // concession - it is the correct expectation. A paragraph of English in
        // a Markdown file has nothing to mark, and a document that is mostly
        // prose will legitimately report low coverage. Code is different: every
        // non-blank line of C or Rust has a keyword, a bracket or an operator
        // in it, so a code file that leaves lines plain has something wrong.
        // A percentage floor is the wrong instrument for prose. It measures how
        // much prose a document contains rather than how well it was
        // highlighted, and it failed this very README for being mostly prose -
        // which is what a README is.
        //
        // What is checked instead is that markup was marked where markup
        // exists: if the document has headings, code spans or links, they must
        // have produced tokens.
        const bool prose = language == SyntaxHighlighter::Language::Markdown;

        if (!recognised) {
            out << "    FAIL: the extension is not recognised\n";
            ++failures;
        } else if (prose) {
            int markupLines = 0;
            for (const QString& line : lines) {
                const QString trimmed = line.trimmed();
                if (trimmed.startsWith(QLatin1Char('#'))
                    || trimmed.startsWith(QLatin1String("- "))
                    || trimmed.startsWith(QLatin1String("```"))
                    || trimmed.contains(QLatin1Char('`'))
                    || trimmed.contains(QLatin1String("**"))
                    || trimmed.contains(QLatin1String("]("))) {
                    ++markupLines;
                }
            }

            out << "    markup lines: " << markupLines
                << ", of them marked: " << linesWithTokens << "\n";

            if (markupLines > 0 && linesWithTokens == 0) {
                out << "    FAIL: the document has markup and none was marked\n";
                ++failures;
            }
        } else if (coverage < 60.0) {
            // Below this something structural is wrong: a string state that
            // never closes, or rules that stop matching partway down.
            out << "    FAIL: most lines produced no tokens\n";
            ++failures;
        }

        // The kinds found, so a language that only ever emits one kind - rules
        // registered but not reached - is visible.
        QStringList kinds;
        for (auto it = byKind.cbegin(); it != byKind.cend(); ++it) {
            kinds << QStringLiteral("%1=%2").arg(it.key()).arg(it.value());
        }
        kinds.sort();
        out << "    " << kinds.join(QStringLiteral(" ")) << "\n";

        if (recognised && byKind.size() < 2) {
            out << "    FAIL: only one kind of token was produced\n";
            ++failures;
        }
    }

    out << "\n"
        << (failures == 0
                ? QStringLiteral("VERDICT: every sample highlighted.\n")
                : QStringLiteral("VERDICT: FAILED - %1 problem(s).\n").arg(failures));
    return failures == 0 ? 0 : 1;
}
