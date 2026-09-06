#pragma once

#include "editor/TextDocument.h"
#include "langsvc/LanguageServiceManager.h"
#include "workspace/EditorLayout.h"

#include <QAbstractListModel>
#include <QHash>
#include <QQmlEngine>
#include <QTimer>
#include <QVariantList>

#include <vector>

namespace keys::ui {

/// Language intelligence, as the editor sees it.
///
/// **Owns the sync, not the servers.** The manager starts and stops servers;
/// this keeps their copy of every open document identical to the editor's. A
/// missed edit makes every later answer wrong in a way that looks like the
/// server being broken, so the wiring is in one place rather than spread across
/// whatever touched a document.
///
/// **Completion is a list model; diagnostics are a property.** Completions are
/// long, arrive per keystroke and are scrolled — the popup renders only what is
/// visible. Diagnostics are few per file and replaced wholesale, so a plain list
/// is less machinery for the same result.
class LanguageModel : public QAbstractListModel {
    Q_OBJECT
    QML_NAMED_ELEMENT(Language)
    QML_SINGLETON

    /// Completion popup state.
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(bool completionVisible READ completionVisible NOTIFY completionVisibleChanged)
    Q_PROPERTY(int selectedIndex READ selectedIndex WRITE setSelectedIndex
                   NOTIFY selectedIndexChanged)

    /// Where the popup should sit: the position completion was asked at.
    Q_PROPERTY(int anchorLine READ anchorLine NOTIFY completionVisibleChanged)
    Q_PROPERTY(int anchorColumn READ anchorColumn NOTIFY completionVisibleChanged)

    /// Diagnostics for the active document, as maps for QML.
    Q_PROPERTY(QVariantList diagnostics READ diagnostics NOTIFY diagnosticsChanged)
    Q_PROPERTY(int errorCount READ errorCount NOTIFY diagnosticsChanged)
    Q_PROPERTY(int warningCount READ warningCount NOTIFY diagnosticsChanged)

    /// Which servers are up, for the status bar. Empty is the normal state for a
    /// project whose language has no server installed.
    Q_PROPERTY(QString serverStatus READ serverStatus NOTIFY serverStatusChanged)

    /// Set when a hover request comes back with something to show.
    Q_PROPERTY(QString hoverText READ hoverText NOTIFY hoverChanged)

public:
    enum Roles {
        LabelRole = Qt::UserRole + 1,
        DetailRole,
        KindLabelRole,
        InsertTextRole,
    };

    LanguageModel(langsvc::LanguageServiceManager& manager,
                  workspace::EditorLayout& editors, QObject* parent = nullptr);

    static void setInstance(LanguageModel* instance);
    static LanguageModel* create(QQmlEngine* engine, QJSEngine* scriptEngine);

    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    [[nodiscard]] int count() const { return static_cast<int>(m_completions.size()); }
    [[nodiscard]] bool completionVisible() const { return m_completionVisible; }
    [[nodiscard]] int selectedIndex() const { return m_selectedIndex; }
    void setSelectedIndex(int index);

    [[nodiscard]] int anchorLine() const { return m_anchor.line; }
    [[nodiscard]] int anchorColumn() const { return m_anchor.character; }

    [[nodiscard]] QVariantList diagnostics() const;
    [[nodiscard]] int errorCount() const;
    [[nodiscard]] int warningCount() const;

    [[nodiscard]] QString serverStatus() const;
    [[nodiscard]] QString hoverText() const { return m_hoverText; }

    /// A document was opened or closed by the workspace. Called by the
    /// composition root rather than discovered, so the sync cannot silently
    /// miss a document.
    void documentOpened(editor::TextDocument* document);
    void documentClosed(const QString& path);

    // ---- Completion --------------------------------------------------------

    /// Asks for completions at the caret. Debounced: typing fast would otherwise
    /// send one request per keystroke and most would be obsolete on arrival.
    Q_INVOKABLE void requestCompletion();
    Q_INVOKABLE void dismissCompletion();
    Q_INVOKABLE void selectNext();
    Q_INVOKABLE void selectPrevious();

    /// Inserts the selected completion, replacing the partial word the caret is
    /// in. Returns false when there is nothing selected.
    Q_INVOKABLE bool acceptCompletion();

    // ---- Navigation --------------------------------------------------------

    /// Jumps to the definition under the caret. Asynchronous; the workbench
    /// opens the file when the answer arrives.
    Q_INVOKABLE void goToDefinition();

    /// Renames the symbol at the caret across the whole project.
    ///
    /// Asked of the language server rather than done by search and replace:
    /// only the server knows which occurrences of a name are the same symbol,
    /// and a textual rename would also hit comments, strings, and unrelated
    /// identifiers that happen to match.
    Q_INVOKABLE void renameSymbol(const QString& newName);

    /// Whether a rename is possible here, so the menu can dim rather than
    /// offering something that will fail.
    [[nodiscard]] Q_INVOKABLE bool canRename() const;

    /// The identifier under the caret, for seeding the rename dialog. A rename
    /// is usually a small change to a name already on screen, so the field
    /// starts with it rather than empty.
    [[nodiscard]] Q_INVOKABLE QString symbolAtCursor() const;

    Q_INVOKABLE void requestHover(int line, int column);
    Q_INVOKABLE void clearHover();

signals:
    void countChanged();
    void completionVisibleChanged();
    void selectedIndexChanged();
    void diagnosticsChanged();
    void serverStatusChanged();
    void hoverChanged();

    /// A definition was found. The model does not open files itself.
    void definitionFound(const QString& path, int line, int column);

    /// Something a person should see: a server that could not start, or a
    /// message from one that did.
    void notice(const QString& message);

    /// A rename finished. Carries what changed so the view can say so - a
    /// silent rename across nine files is alarming rather than reassuring.
    void renameApplied(int fileCount, int editCount);

private:
    /// Applies one file's worth of edits, bottom-up.
    void applyEditsToFile(const QString& path,
                          const std::vector<langsvc::TextEdit>& edits);

    /// Files a rename touched that were not open, and so were skipped.
    int m_renameSkipped = 0;

    /// The document the caret is in, or null.
    [[nodiscard]] editor::TextDocument* activeDocument() const;

    /// Connects a document's edits to the server that owns it.
    void watchDocument(editor::TextDocument* document);

    void applyDiagnostics(const QString& path,
                          const std::vector<langsvc::Diagnostic>& diagnostics);

    void setCompletionVisible(bool visible);

    /// The word the caret sits inside, which a completion replaces. Returned as
    /// a range so accepting one does not leave the prefix behind.
    [[nodiscard]] editor::Range wordRangeAtCursor(editor::TextDocument* document) const;

    langsvc::LanguageServiceManager& m_manager;
    workspace::EditorLayout& m_editors;

    std::vector<langsvc::CompletionItem> m_completions;
    bool m_completionVisible = false;
    int m_selectedIndex = 0;
    langsvc::LspPosition m_anchor;

    /// Diagnostics per file. Kept for every file rather than only the active one
    /// so switching tabs does not lose what a server already reported.
    QHash<QString, std::vector<langsvc::Diagnostic>> m_diagnostics;

    QString m_hoverText;

    /// Collapses a burst of keystrokes into one completion request.
    QTimer m_completionDebounce;

    /// Documents already connected, so a rebind does not connect twice.
    QSet<QString> m_watched;

    static constexpr int kCompletionDebounceMs = 150;

    /// A popup longer than this is scrolled past rather than read.
    static constexpr int kMaxCompletions = 200;
};

} // namespace keys::ui
