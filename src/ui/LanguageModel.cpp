#include "ui/LanguageModel.h"

#include "editor/WordCompleter.h"

#include "core/Log.h"

#include <QVariantMap>

#include <algorithm>

using keys::langsvc::CompletionItem;
using keys::langsvc::CompletionKind;
using keys::langsvc::Diagnostic;
using keys::langsvc::DiagnosticSeverity;
using keys::langsvc::LanguageClient;
using keys::langsvc::LspPosition;
using keys::langsvc::LspRange;

namespace keys::ui {
namespace {

/// The instance main() publishes for QML; see Theme.cpp for the reasoning.
LanguageModel* g_instance = nullptr;

/// A short label for a completion's kind. Shown beside the label so a function
/// and a variable of the same name are distinguishable at a glance.
QString kindLabel(CompletionKind kind)
{
    switch (kind) {
    case CompletionKind::Method:
    case CompletionKind::Function:
    case CompletionKind::Constructor:
        return QStringLiteral("fn");
    case CompletionKind::Field:
    case CompletionKind::Property:
        return QStringLiteral("field");
    case CompletionKind::Variable:
        return QStringLiteral("var");
    case CompletionKind::Class:
    case CompletionKind::Struct:
        return QStringLiteral("class");
    case CompletionKind::Interface:
        return QStringLiteral("iface");
    case CompletionKind::Module:
        return QStringLiteral("mod");
    case CompletionKind::Enum:
    case CompletionKind::EnumMember:
        return QStringLiteral("enum");
    case CompletionKind::Keyword:
        return QStringLiteral("kw");
    case CompletionKind::Constant:
        return QStringLiteral("const");
    case CompletionKind::File:
    case CompletionKind::Folder:
        return QStringLiteral("path");
    case CompletionKind::TypeParameter:
        return QStringLiteral("type");
    default:
        return QString();
    }
}

/// Whether a character can be part of an identifier being completed.
bool isWordCharacter(QChar character)
{
    return character.isLetterOrNumber() || character == QLatin1Char('_');
}

} // namespace

void LanguageModel::setInstance(LanguageModel* instance)
{
    g_instance = instance;
}

LanguageModel* LanguageModel::create(QQmlEngine* engine, QJSEngine* scriptEngine)
{
    Q_UNUSED(engine)
    Q_UNUSED(scriptEngine)

    Q_ASSERT_X(g_instance, "LanguageModel::create",
               "LanguageModel::setInstance was not called");

    QQmlEngine::setObjectOwnership(g_instance, QQmlEngine::CppOwnership);
    return g_instance;
}

LanguageModel::LanguageModel(langsvc::LanguageServiceManager& manager,
                             workspace::EditorLayout& editors, QObject* parent)
    : QAbstractListModel(parent), m_manager(manager), m_editors(editors)
{
    m_completionDebounce.setSingleShot(true);
    m_completionDebounce.setInterval(kCompletionDebounceMs);
    connect(&m_completionDebounce, &QTimer::timeout, this,
            &LanguageModel::requestCompletion);

    connect(&m_manager, &langsvc::LanguageServiceManager::diagnosticsPublished,
            this, &LanguageModel::applyDiagnostics);

    connect(&m_manager, &langsvc::LanguageServiceManager::clientStateChanged,
            this, &LanguageModel::serverStatusChanged);

    connect(&m_manager, &langsvc::LanguageServiceManager::serverFailed, this,
            [this](const QString&, const QString& message) { emit notice(message); });

    connect(&m_manager, &langsvc::LanguageServiceManager::serverMessage,
            this, &LanguageModel::notice);

    // Switching documents changes which diagnostics are shown and invalidates
    // any popup that was open over the previous one.
    connect(&m_editors, &workspace::EditorLayout::activeDocumentChanged, this, [this] {
        dismissCompletion();
        emit diagnosticsChanged();
    });
}

editor::TextDocument* LanguageModel::activeDocument() const
{
    return m_editors.activeDocument();
}

QString LanguageModel::serverStatus() const
{
    const QStringList running = m_manager.runningServers();
    return running.isEmpty() ? QString() : running.join(QStringLiteral(", "));
}

void LanguageModel::documentOpened(editor::TextDocument* document)
{
    if (!document || document->path().isEmpty()) {
        return;
    }

    const QString path = document->path();
    LanguageClient* client = m_manager.clientFor(path);
    if (!client) {
        return;   // no server for this language; not an error
    }

    client->openDocument(path, langsvc::languageIdForPath(path), document->text());
    watchDocument(document);
    emit serverStatusChanged();
}

void LanguageModel::documentClosed(const QString& path)
{
    if (LanguageClient* client = m_manager.existingClientFor(path)) {
        client->closeDocument(path);
    }

    m_watched.remove(path);

    // Diagnostics for a closed file would otherwise accumulate for the life of
    // the session.
    if (m_diagnostics.remove(path)) {
        emit diagnosticsChanged();
    }
}

void LanguageModel::watchDocument(editor::TextDocument* document)
{
    const QString path = document->path();
    if (m_watched.contains(path)) {
        return;   // already connected; a rebind must not connect twice
    }
    m_watched.insert(path);

    connect(document, &editor::TextDocument::contentsChanged, this,
            [this, document](const editor::Range& replaced, int) {
                const QString path = document->path();

                // Completion is considered whether or not a server is running.
                // This used to sit below the `if (!client) return`, so on a
                // machine with no language server - which is most machines -
                // typing never offered anything and Ctrl+Space was the only way
                // in. A feature reachable only by an unadvertised shortcut is
                // not a feature.
                considerCompletion(*document, replaced);

                LanguageClient* client = m_manager.existingClientFor(path);
                if (!client) {
                    return;
                }

                // The editor's coordinates are already LSP's: zero-based lines
                // and UTF-16 columns. See TextBuffer's note - this is a field
                // copy, not an encoding change.
                LspRange range;
                range.start = {replaced.start.line, replaced.start.column};
                range.end = {replaced.end.line, replaced.end.column};

                // The full text is passed too, for a server that only supports
                // full sync. The client picks which to send.
                client->changeDocument(path, range, document->text(), document->text());

            });

    connect(document, &editor::TextDocument::modifiedChanged, this,
            [this, document](bool modified) {
                if (!modified) {
                    // Became unmodified: it was saved. Several servers only
                    // re-check a file on save.
                    if (LanguageClient* client =
                            m_manager.existingClientFor(document->path())) {
                        client->saveDocument(document->path());
                    }
                }
            });
}

void LanguageModel::applyDiagnostics(const QString& path,
                                     const std::vector<Diagnostic>& diagnostics)
{
    // LSP publishes the whole set for a file, so this replaces rather than adds.
    if (diagnostics.empty()) {
        m_diagnostics.remove(path);
    } else {
        m_diagnostics.insert(path, diagnostics);
    }
    emit diagnosticsChanged();
}

QVariantList LanguageModel::diagnostics() const
{
    const editor::TextDocument* document = activeDocument();
    if (!document) {
        return {};
    }

    const auto it = m_diagnostics.constFind(document->path());
    if (it == m_diagnostics.cend()) {
        return {};
    }

    QVariantList result;
    result.reserve(static_cast<qsizetype>(it->size()));

    for (const Diagnostic& diagnostic : *it) {
        QVariantMap entry;
        entry.insert(QStringLiteral("line"), diagnostic.range.start.line);
        entry.insert(QStringLiteral("startColumn"), diagnostic.range.start.character);
        entry.insert(QStringLiteral("endColumn"), diagnostic.range.end.character);
        entry.insert(QStringLiteral("endLine"), diagnostic.range.end.line);
        entry.insert(QStringLiteral("severity"), static_cast<int>(diagnostic.severity));
        entry.insert(QStringLiteral("message"), diagnostic.message);
        entry.insert(QStringLiteral("source"), diagnostic.source);
        result.append(entry);
    }
    return result;
}

int LanguageModel::errorCount() const
{
    const editor::TextDocument* document = activeDocument();
    if (!document) {
        return 0;
    }
    const auto it = m_diagnostics.constFind(document->path());
    if (it == m_diagnostics.cend()) {
        return 0;
    }
    return static_cast<int>(std::count_if(
        it->cbegin(), it->cend(), [](const Diagnostic& diagnostic) {
            return diagnostic.severity == DiagnosticSeverity::Error;
        }));
}

int LanguageModel::warningCount() const
{
    const editor::TextDocument* document = activeDocument();
    if (!document) {
        return 0;
    }
    const auto it = m_diagnostics.constFind(document->path());
    if (it == m_diagnostics.cend()) {
        return 0;
    }
    return static_cast<int>(std::count_if(
        it->cbegin(), it->cend(), [](const Diagnostic& diagnostic) {
            return diagnostic.severity == DiagnosticSeverity::Warning;
        }));
}

// ---- Completion ------------------------------------------------------------

void LanguageModel::setCompletionVisible(bool visible)
{
    if (visible == m_completionVisible) {
        return;
    }
    m_completionVisible = visible;
    emit completionVisibleChanged();
}

void LanguageModel::requestCompletion()
{
    editor::TextDocument* document = activeDocument();
    if (!document || document->path().isEmpty()) {
        return;
    }

    LanguageClient* client = m_manager.existingClientFor(document->path());
    if (!client || !client->supportsCompletion()) {
        // No server: complete from the words already in the file and the
        // language's own vocabulary. Not a substitute for a server that
        // understands types and scope, but the alternative here was nothing at
        // all - and most machines have no clangd, pyright or gopls installed.
        completeFromBuffer(*document);
        return;
    }

    const editor::Position caret = document->cursor().position;
    const LspPosition position{caret.line, caret.column};

    client->requestCompletion(
        document->path(), position,
        [this, position](std::vector<CompletionItem> items) {
            // The caret may have moved while the request was in flight. Showing
            // stale entries is worse than showing none.
            const editor::TextDocument* current = activeDocument();
            if (!current || current->cursor().position.line != position.line) {
                return;
            }

            if (items.size() > static_cast<size_t>(kMaxCompletions)) {
                items.resize(static_cast<size_t>(kMaxCompletions));
            }

            // The server's own ordering is respected: servers put real effort
            // into sortText, and re-sorting alphabetically throws that away.
            std::stable_sort(items.begin(), items.end(),
                             [](const CompletionItem& a, const CompletionItem& b) {
                                 const QString& left =
                                     a.sortText.isEmpty() ? a.label : a.sortText;
                                 const QString& right =
                                     b.sortText.isEmpty() ? b.label : b.sortText;
                                 return left < right;
                             });

            beginResetModel();
            m_completions = std::move(items);
            endResetModel();

            m_selectedIndex = 0;
            m_anchor = position;

            emit countChanged();
            emit selectedIndexChanged();
            setCompletionVisible(!m_completions.empty());
        });
}

void LanguageModel::considerCompletion(const editor::TextDocument& document,
                                      const editor::Range& replaced)
{
    // An open popup always refreshes: its entries were computed for a prefix
    // that no longer exists.
    if (m_completionVisible) {
        m_completionDebounce.start();
        return;
    }

    // Otherwise, only while typing a word - and only forwards. A deletion that
    // happens to leave a long enough prefix should not pop a list open under
    // someone who is removing text.
    if (replaced.start.line != replaced.end.line
        || replaced.end.column <= replaced.start.column) {
        return;
    }

    const QStringList lines = document.text().split(QLatin1Char('\n'));
    const editor::Position caret = document.cursor().position;
    if (caret.line < 0 || caret.line >= lines.size()) {
        return;
    }

    // Long enough to be worth completing. One character matches most of the
    // file and the popup becomes noise rather than help.
    const QString prefix =
        editor::WordCompleter::prefixAt(lines.at(caret.line), caret.column);
    if (prefix.size() < editor::WordCompleter::kMinPrefixLength) {
        return;
    }

    m_completionDebounce.start();
}

void LanguageModel::completeFromBuffer(const editor::TextDocument& document)
{
    const editor::Position caret = document.cursor().position;

    const std::vector<editor::WordSuggestion> suggestions =
        editor::WordCompleter::suggest(
            document, caret.line, caret.column,
            editor::SyntaxHighlighter::languageForPath(document.path()));

    std::vector<CompletionItem> items;
    items.reserve(suggestions.size());

    for (const editor::WordSuggestion& suggestion : suggestions) {
        CompletionItem item;
        item.label = suggestion.word;

        // Said plainly, because a suggestion from the buffer is a weaker claim
        // than one from a server that understands the code, and the list should
        // not pretend otherwise.
        switch (suggestion.source) {
        case editor::WordSuggestion::Source::Buffer:
            item.kind = CompletionKind::Text;
            item.detail = tr("in this file");
            break;
        case editor::WordSuggestion::Source::Keyword:
            item.kind = CompletionKind::Keyword;
            item.detail = tr("keyword");
            break;
        case editor::WordSuggestion::Source::Type:
            // Keyword rather than Class: `u32` is a type *name* the language
            // reserves, not a class the file declares, and labelling it
            // "class" would be a claim about the code that is not true.
            item.kind = CompletionKind::Keyword;
            item.detail = tr("type");
            break;
        }

        items.push_back(std::move(item));
    }

    // Already ranked by the completer; re-sorting would discard that.
    beginResetModel();
    m_completions = std::move(items);
    endResetModel();

    m_selectedIndex = 0;
    m_anchor = LspPosition{caret.line, caret.column};

    emit countChanged();
    emit selectedIndexChanged();
    setCompletionVisible(!m_completions.empty());
}

void LanguageModel::dismissCompletion()
{
    m_completionDebounce.stop();

    if (!m_completions.empty()) {
        beginResetModel();
        m_completions.clear();
        endResetModel();
        emit countChanged();
    }
    setCompletionVisible(false);
}

void LanguageModel::setSelectedIndex(int index)
{
    const int clamped = m_completions.empty() ? 0 : std::clamp(index, 0, count() - 1);
    if (clamped == m_selectedIndex) {
        return;
    }
    m_selectedIndex = clamped;
    emit selectedIndexChanged();
}

void LanguageModel::selectNext()
{
    if (m_completions.empty()) {
        return;
    }
    setSelectedIndex((m_selectedIndex + 1) % count());
}

void LanguageModel::selectPrevious()
{
    if (m_completions.empty()) {
        return;
    }
    setSelectedIndex((m_selectedIndex + count() - 1) % count());
}

editor::Range LanguageModel::wordRangeAtCursor(editor::TextDocument* document) const
{
    const editor::Position caret = document->cursor().position;
    const QString line = document->buffer().line(caret.line);

    int start = caret.column;
    while (start > 0 && isWordCharacter(line.at(start - 1))) {
        --start;
    }

    // Only backwards: the caret is at the end of what has been typed, and
    // swallowing text after it would delete something the user did not mean to
    // replace.
    return {{caret.line, start}, caret};
}

bool LanguageModel::acceptCompletion()
{
    if (m_completions.empty() || m_selectedIndex < 0 || m_selectedIndex >= count()) {
        return false;
    }

    editor::TextDocument* document = activeDocument();
    if (!document) {
        return false;
    }

    const QString text =
        m_completions.at(static_cast<size_t>(m_selectedIndex)).textToInsert();

    // The partial word is replaced rather than appended to, or completing "pri"
    // with "print" would leave "priprint".
    const editor::Range word = wordRangeAtCursor(document);
    document->setCursorPosition(word.start);
    document->setCursorPosition(word.end, true);   // select the prefix
    document->insertText(text);

    dismissCompletion();
    return true;
}

// ---- Navigation ------------------------------------------------------------

QString LanguageModel::symbolAtCursor() const
{
    editor::TextDocument* document = activeDocument();
    if (!document) {
        return QString();
    }

    // Reuses the range a completion would replace, so "the word the caret is
    // in" means the same thing in both places.
    const editor::Range range = wordRangeAtCursor(document);
    if (range.isEmpty()) {
        return QString();
    }
    return document->line(range.start.line)
        .mid(range.start.column, range.end.column - range.start.column);
}

bool LanguageModel::canRename() const
{
    const editor::TextDocument* document = activeDocument();
    if (!document || document->path().isEmpty()) {
        return false;
    }
    const LanguageClient* client = m_manager.existingClientFor(document->path());
    return client && client->supportsRename();
}

void LanguageModel::renameSymbol(const QString& newName)
{
    editor::TextDocument* document = activeDocument();
    if (!document || document->path().isEmpty() || newName.isEmpty()) {
        return;
    }

    LanguageClient* client = m_manager.existingClientFor(document->path());
    if (!client || !client->supportsRename()) {
        emit notice(tr("No language server is providing renames for this file."));
        return;
    }

    const editor::Position caret = document->cursor().position;

    client->requestRename(
        document->path(), {caret.line, caret.column}, newName,
        [this](langsvc::WorkspaceEdit edit) {
            if (edit.isEmpty()) {
                emit notice(tr("Nothing to rename here."));
                return;
            }

            const int files = edit.fileCount();
            const int edits = edit.editCount();
            m_renameSkipped = 0;

            for (auto it = edit.changes.constBegin();
                 it != edit.changes.constEnd(); ++it) {
                applyEditsToFile(it.key(), it.value());
            }

            // Said out loud. A rename that silently touches nine files is
            // alarming rather than reassuring, and the user needs to know how
            // far it reached before they decide whether to keep it.
            emit renameApplied(files - m_renameSkipped, edits);

            if (m_renameSkipped > 0) {
                emit notice(tr("%1 file(s) were not open and were left "
                               "unchanged.").arg(m_renameSkipped));
            }
        });
}

void LanguageModel::applyEditsToFile(const QString& path,
                                     const std::vector<langsvc::TextEdit>& edits)
{
    if (edits.empty()) {
        return;
    }

    // Bottom-up within the file, so each edit lands at a position the ones
    // before it have not shifted - the same reason multi-cursor edits run
    // backwards. A server may send them in any order.
    std::vector<langsvc::TextEdit> ordered = edits;
    std::sort(ordered.begin(), ordered.end(),
              [](const langsvc::TextEdit& a, const langsvc::TextEdit& b) {
                  if (a.range.start.line != b.range.start.line) {
                      return b.range.start.line < a.range.start.line;
                  }
                  return b.range.start.character < a.range.start.character;
              });

    // Only files that are already open.
    //
    // LanguageModel sees the editor layout, not the workspace, so it cannot
    // open a file - and editing a closed file on disk behind the user's back
    // would be worse anyway: no undo, no dirty marker, nothing to review. The
    // caller is told what was left untouched rather than the rename quietly
    // being partial.
    editor::TextDocument* document = nullptr;
    for (int group = 0; group < m_editors.groupCount() && !document; ++group) {
        workspace::EditorGroup* editors = m_editors.groupAt(group);
        if (!editors) {
            continue;
        }
        const int index = editors->indexOfPath(path);
        if (index >= 0) {
            document = editors->documentAt(index);
        }
    }

    if (!document) {
        ++m_renameSkipped;
        return;
    }

    for (const langsvc::TextEdit& edit : ordered) {
        document->replaceRange(
            editor::Range{
                editor::Position{edit.range.start.line, edit.range.start.character},
                editor::Position{edit.range.end.line, edit.range.end.character}},
            edit.newText);
    }
}

void LanguageModel::goToDefinition()
{
    editor::TextDocument* document = activeDocument();
    if (!document || document->path().isEmpty()) {
        return;
    }

    LanguageClient* client = m_manager.existingClientFor(document->path());
    if (!client || !client->supportsDefinition()) {
        emit notice(tr("No language server is providing definitions for this file."));
        return;
    }

    const editor::Position caret = document->cursor().position;

    client->requestDefinition(
        document->path(), {caret.line, caret.column},
        [this](std::vector<langsvc::Location> locations) {
            if (locations.empty()) {
                emit notice(tr("No definition found."));
                return;
            }
            // The first: a definition request can return several for an
            // overloaded symbol, and choosing between them needs a picker that
            // is not part of this milestone.
            const langsvc::Location& location = locations.front();
            emit definitionFound(langsvc::uriToPath(location.uri),
                                 location.range.start.line,
                                 location.range.start.character);
        });
}

void LanguageModel::requestHover(int line, int column)
{
    editor::TextDocument* document = activeDocument();
    if (!document || document->path().isEmpty()) {
        return;
    }

    LanguageClient* client = m_manager.existingClientFor(document->path());
    if (!client || !client->supportsHover()) {
        return;
    }

    client->requestHover(document->path(), {line, column}, [this](QString text) {
        if (text == m_hoverText) {
            return;
        }
        m_hoverText = std::move(text);
        emit hoverChanged();
    });
}

void LanguageModel::clearHover()
{
    if (m_hoverText.isEmpty()) {
        return;
    }
    m_hoverText.clear();
    emit hoverChanged();
}

// ---- Model -----------------------------------------------------------------

int LanguageModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_completions.size());
}

QVariant LanguageModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= rowCount()) {
        return {};
    }

    const CompletionItem& item = m_completions.at(static_cast<size_t>(index.row()));

    switch (role) {
    case LabelRole:
        return item.label;
    case DetailRole:
        return item.detail;
    case KindLabelRole:
        return kindLabel(item.kind);
    case InsertTextRole:
        return item.textToInsert();
    default:
        return {};
    }
}

QHash<int, QByteArray> LanguageModel::roleNames() const
{
    return {
        {LabelRole, "label"},
        {DetailRole, "detail"},
        {KindLabelRole, "kindLabel"},
        {InsertTextRole, "insertText"},
    };
}

} // namespace keys::ui
