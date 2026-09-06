#pragma once

#include <QJsonObject>
#include <QHash>
#include <QString>

#include <vector>

namespace keys::langsvc {

/// A position in a document: zero-based line and UTF-16 column.
///
/// Deliberately its own type rather than editor::Position. `langsvc` sits below
/// the editor in the layering and must not depend on it — but the two are
/// numerically identical by design (see TextBuffer's note on UTF-16), so the
/// conversion at the boundary is a field copy rather than an encoding change.
struct LspPosition {
    int line = 0;
    int character = 0;

    [[nodiscard]] QJsonObject toJson() const;
    [[nodiscard]] static LspPosition fromJson(const QJsonObject& object);
    [[nodiscard]] bool operator==(const LspPosition& other) const = default;
};

struct LspRange {
    LspPosition start;
    LspPosition end;

    [[nodiscard]] QJsonObject toJson() const;
    [[nodiscard]] static LspRange fromJson(const QJsonObject& object);
};

/// How bad a diagnostic is. The numbers are LSP's own.
enum class DiagnosticSeverity {
    Error = 1,
    Warning = 2,
    Information = 3,
    Hint = 4,
};

/// One diagnostic from a language server.
struct Diagnostic {
    LspRange range;
    DiagnosticSeverity severity = DiagnosticSeverity::Error;
    QString message;
    QString source;   ///< which tool produced it ("clang", "eslint")
    QString code;

    [[nodiscard]] static Diagnostic fromJson(const QJsonObject& object);
};

/// What kind of thing a completion offers. LSP's numbering, kept so the UI can
/// pick an icon without a translation table.
enum class CompletionKind {
    Text = 1, Method = 2, Function = 3, Constructor = 4, Field = 5,
    Variable = 6, Class = 7, Interface = 8, Module = 9, Property = 10,
    Unit = 11, Value = 12, Enum = 13, Keyword = 14, Snippet = 15,
    Color = 16, File = 17, Reference = 18, Folder = 19, EnumMember = 20,
    Constant = 21, Struct = 22, Event = 23, Operator = 24, TypeParameter = 25,
};

struct CompletionItem {
    QString label;
    CompletionKind kind = CompletionKind::Text;
    QString detail;
    QString documentation;

    /// What to insert, when it differs from the label. Empty means insert the
    /// label.
    QString insertText;

    /// What the server wants this sorted by. Servers put real effort into this
    /// ordering, so it is respected rather than re-sorted alphabetically.
    QString sortText;

    [[nodiscard]] QString textToInsert() const
    {
        return insertText.isEmpty() ? label : insertText;
    }

    [[nodiscard]] static CompletionItem fromJson(const QJsonObject& object);
};

/// Where something is defined.
/// One replacement inside one file.
struct TextEdit {
    LspRange range;
    QString newText;

    [[nodiscard]] static TextEdit fromJson(const QJsonObject& object)
    {
        TextEdit edit;
        edit.range = LspRange::fromJson(object.value(QStringLiteral("range")).toObject());
        edit.newText = object.value(QStringLiteral("newText")).toString();
        return edit;
    }
};

/// Edits across any number of files, which is what a rename returns.
///
/// Renaming a symbol touches every file that uses it, so this is a map from
/// path to the edits in that file rather than a single list - the caller has to
/// open and edit each one.
struct WorkspaceEdit {
    QHash<QString, std::vector<TextEdit>> changes;   ///< keyed by absolute path

    [[nodiscard]] bool isEmpty() const { return changes.isEmpty(); }

    [[nodiscard]] int fileCount() const { return static_cast<int>(changes.size()); }

    [[nodiscard]] int editCount() const
    {
        int total = 0;
        for (const auto& edits : changes) {
            total += static_cast<int>(edits.size());
        }
        return total;
    }
};

struct Location {
    QString uri;
    LspRange range;

    [[nodiscard]] static Location fromJson(const QJsonObject& object);
};

/// A file path as an LSP `file://` URI, and back.
///
/// Its own functions because the Windows cases are easy to get wrong: a drive
/// letter needs a leading slash (`file:///C:/x`), and backslashes must become
/// forward slashes or the server sees a different file.
[[nodiscard]] QString pathToUri(const QString& path);
[[nodiscard]] QString uriToPath(const QString& uri);

/// The LSP language identifier for a file, from its extension. Empty when Keys
/// has no mapping, which is what stops it opening a document a server cannot
/// understand.
[[nodiscard]] QString languageIdForPath(const QString& path);

} // namespace keys::langsvc
