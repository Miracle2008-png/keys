// Types into a real document and reports what completion offered.
//
// WordCompleterTests covers the ranking and the prefix rules. This covers the
// part they cannot: whether typing actually reaches the completer at all. The
// automatic trigger sat below a `if (!client) return`, so on a machine with no
// language server the popup never opened and Ctrl+Space was the only way in -
// a defect no unit test on the completer itself would have found.
//
//     cmake --build build --target keys_completion_check
//     build/bin/keys_completion_check

#include "editor/SyntaxHighlighter.h"
#include "editor/TextDocument.h"
#include "editor/WordCompleter.h"

#include <QCoreApplication>
#include <QTextStream>

using namespace keys;

namespace {

int g_failures = 0;

void check(QTextStream& out, const QString& name, bool ok, const QString& detail)
{
    out << (ok ? QStringLiteral("  ok  ") : QStringLiteral("  FAIL")) << " "
        << name.leftJustified(44) << "  " << detail << "\n";
    if (!ok) {
        ++g_failures;
    }
}

/// Types `text` one character at a time into a document, as a person would,
/// and reports what would be offered after the last keystroke.
QStringList typeAndComplete(const QString& seed, const QString& typed,
                            editor::SyntaxHighlighter::Language language)
{
    editor::TextDocument document;
    document.setText(seed);

    // The caret goes to the end, then each character is inserted separately -
    // the completer sees the same sequence of states a typist produces.
    document.moveToDocumentEnd();
    for (const QChar character : typed) {
        document.insertText(QString(character));
    }

    const editor::Position caret = document.cursor().position;
    QStringList words;
    for (const editor::WordSuggestion& suggestion :
         editor::WordCompleter::suggest(document, caret.line, caret.column, language)) {
        words << suggestion.word;
    }
    return words;
}

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    QTextStream out(stdout);

    out << "-- completion without a language server --\n";

    {
        const QStringList words = typeAndComplete(
            QStringLiteral("const invoiceTotal = 42;\n"),
            QStringLiteral("inv"),
            editor::SyntaxHighlighter::Language::JavaScript);

        check(out, QStringLiteral("a word from the buffer"),
              words.contains(QStringLiteral("invoiceTotal")),
              words.mid(0, 5).join(QLatin1String(", ")));
    }

    {
        const QStringList words = typeAndComplete(
            QStringLiteral("int main() {\n    "),
            QStringLiteral("ret"),
            editor::SyntaxHighlighter::Language::C);

        check(out, QStringLiteral("a keyword of the language"),
              words.contains(QStringLiteral("return")),
              words.mid(0, 5).join(QLatin1String(", ")));
    }

    {
        const QStringList words = typeAndComplete(
            QStringLiteral("struct Point { }\n"),
            QStringLiteral("Poi"),
            editor::SyntaxHighlighter::Language::Rust);

        check(out, QStringLiteral("a type declared in the file"),
              words.contains(QStringLiteral("Point")),
              words.mid(0, 5).join(QLatin1String(", ")));
    }

    {
        // One character is not enough: the list would be most of the file.
        const QStringList words = typeAndComplete(
            QStringLiteral("invoiceTotal = 1;\n"),
            QStringLiteral("i"),
            editor::SyntaxHighlighter::Language::JavaScript);

        check(out, QStringLiteral("one character offers nothing"),
              words.isEmpty(),
              QStringLiteral("%1 suggestion(s)").arg(words.size()));
    }

    {
        // A file Keys has no rules for still completes from itself.
        const QStringList words = typeAndComplete(
            QStringLiteral("someUnusualIdentifier\n"),
            QStringLiteral("someUn"),
            editor::SyntaxHighlighter::Language::None);

        check(out, QStringLiteral("an unknown language still completes"),
              words.contains(QStringLiteral("someUnusualIdentifier")),
              words.mid(0, 5).join(QLatin1String(", ")));
    }

    {
        // The proof that nothing is invented: a prefix matching nothing in the
        // buffer and nothing in the language offers nothing at all.
        const QStringList words = typeAndComplete(
            QStringLiteral("alpha beta gamma\n"),
            QStringLiteral("zzqx"),
            editor::SyntaxHighlighter::Language::C);

        check(out, QStringLiteral("nothing is invented"), words.isEmpty(),
              QStringLiteral("%1 suggestion(s)").arg(words.size()));
    }

    out << "\n"
        << (g_failures == 0
                ? QStringLiteral("VERDICT: completion works with no server running.\n")
                : QStringLiteral("VERDICT: FAILED - %1 problem(s).\n").arg(g_failures));

    return g_failures == 0 ? 0 : 1;
}
