#pragma once

#include "langsvc/LspTypes.h"

#include <QAbstractListModel>
#include <QQmlEngine>
#include <QString>

#include <map>
#include <vector>

namespace keys::langsvc {
class LanguageServiceManager;
}

namespace keys::project {
class Project;
}

namespace keys::ui {

/// Every diagnostic the language servers have reported, across the project.
///
/// **Accumulated, not snapshotted.** A server publishes diagnostics per file as
/// it analyses them, and clears a file by publishing an empty list for it. So
/// the model keeps a map keyed by path and replaces one file's entries at a
/// time - taking the latest publication as the whole truth would show only the
/// last file touched.
///
/// **Files and problems in one list**, the same flattening the search panel
/// uses and for the same reason: the tree is only ever two deep, and a ListView
/// is far cheaper than a TreeView.
class ProblemsModel : public QAbstractListModel {
    Q_OBJECT
    QML_NAMED_ELEMENT(Problems)
    QML_SINGLETON

    Q_PROPERTY(int errorCount READ errorCount NOTIFY changed)
    Q_PROPERTY(int warningCount READ warningCount NOTIFY changed)
    Q_PROPERTY(int infoCount READ infoCount NOTIFY changed)
    Q_PROPERTY(int fileCount READ fileCount NOTIFY changed)

    /// Whether anything has been reported at all, so the panel can tell "no
    /// problems" apart from "nothing has been analysed yet" - which are very
    /// different things to show someone who just opened a project.
    Q_PROPERTY(bool hasAnalysed READ hasAnalysed NOTIFY changed)

    /// Hides anything below a warning. Off by default: an unused-variable notice
    /// is worth seeing, and a panel that silently omits things is worse than a
    /// long one.
    Q_PROPERTY(bool errorsAndWarningsOnly READ errorsAndWarningsOnly
                   WRITE setErrorsAndWarningsOnly NOTIFY changed)

public:
    enum RowKind {
        FileRow = 0,
        ProblemRow = 1,
    };
    Q_ENUM(RowKind)

    enum Severity {
        ErrorSeverity = 1,
        WarningSeverity = 2,
        InfoSeverity = 3,
        HintSeverity = 4,
    };
    Q_ENUM(Severity)

    enum Roles {
        KindRole = Qt::UserRole + 1,
        PathRole,           ///< absolute, on both kinds of row
        FileNameRole,
        DirectoryRole,
        ProblemCountRole,   ///< file rows
        CollapsedRole,      ///< file rows
        SeverityRole,       ///< problem rows
        MessageRole,
        SourceRole,         ///< which server said it, e.g. "clangd"
        LineRole,           ///< one-based for display
        ColumnRole,
        WorstSeverityRole,  ///< a file row shows its worst
    };

    ProblemsModel(langsvc::LanguageServiceManager& services,
                  const project::Project& project, QObject* parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    [[nodiscard]] int errorCount() const { return m_errors; }
    [[nodiscard]] int warningCount() const { return m_warnings; }
    [[nodiscard]] int infoCount() const { return m_infos; }
    [[nodiscard]] int fileCount() const;
    [[nodiscard]] bool hasAnalysed() const { return m_hasAnalysed; }

    [[nodiscard]] bool errorsAndWarningsOnly() const { return m_severeOnly; }
    void setErrorsAndWarningsOnly(bool on);

    /// Opens the file at the problem. A file row collapses instead.
    Q_INVOKABLE void activate(int row);

    /// Jumps to the next problem after the one last visited, wrapping at the
    /// end. What F2 does in every IDE that has this, and what the Code menu's
    /// "Next Problem" was pointing at the search panel instead of doing.
    Q_INVOKABLE void goToNextProblem();
    Q_INVOKABLE void goToPreviousProblem();

    /// Whether there is anything to jump to, so the menu item can disable
    /// itself rather than being a no-op.
    Q_PROPERTY(bool hasProblems READ hasProblems NOTIFY changed)
    [[nodiscard]] bool hasProblems() const;

    /// Forgets everything. Called when the project changes: diagnostics name
    /// files in the project that was open, and keeping them would point the
    /// panel at paths that are no longer there.
    Q_INVOKABLE void clear();

    static void setInstance(ProblemsModel* instance);
    static ProblemsModel* create(QQmlEngine*, QJSEngine*);

signals:
    void changed();

    /// A problem was chosen. One-based line and column, which is what
    /// AppController::openFileAt expects.
    void problemActivated(const QString& absolutePath, int line, int column);

private:
    struct Row {
        RowKind kind = FileRow;
        int fileIndex = 0;
        int problemIndex = 0;
    };

    struct FileGroup {
        QString path;                              ///< absolute
        std::vector<langsvc::Diagnostic> problems;
        bool collapsed = false;
    };

    void onDiagnostics(const QString& path,
                       const std::vector<langsvc::Diagnostic>& diagnostics);

    /// Rebuilds the groups and rows from m_byPath, and recounts.
    void rebuild();
    void buildRows();

    [[nodiscard]] bool passesFilter(const langsvc::Diagnostic& diagnostic) const;

    langsvc::LanguageServiceManager& m_services;
    const project::Project& m_project;

    /// Keyed by absolute path, ordered so the panel does not reshuffle as
    /// servers report files in whatever order they finish analysing them.
    std::map<QString, std::vector<langsvc::Diagnostic>> m_byPath;

    std::vector<FileGroup> m_files;
    std::vector<Row> m_rows;
    QSet<QString> m_collapsed;

    /// Which problem row the last jump landed on, so the next one continues
    /// rather than starting over. Reset when the rows are rebuilt, because the
    /// index would otherwise point into a list that has changed underneath it.
    int m_lastVisitedRow = -1;

    int m_errors = 0;
    int m_warnings = 0;
    int m_infos = 0;
    bool m_hasAnalysed = false;
    bool m_severeOnly = false;
};

} // namespace keys::ui
