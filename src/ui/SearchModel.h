#pragma once

#include "search/TextSearch.h"

#include <QAbstractListModel>
#include <QSet>
#include <QQmlEngine>
#include <QString>
#include <QTimer>

#include <vector>

namespace keys::ui {

/// Project-wide search results, as the panel shows them.
///
/// **Files and matches in one list.** A result list is naturally a tree - a file
/// with matches under it - but a ListView is far cheaper than a TreeView and the
/// tree is only ever two deep. So the rows are flattened, with a role saying
/// which kind each row is, and the file rows carry a collapsed flag that hides
/// the matches beneath them without rebuilding anything.
///
/// **Typing does not search.** Every keystroke would start a scan of the whole
/// project and cancel it a moment later. The query is debounced, so what runs is
/// what the user stopped typing, not the six prefixes on the way there.
class SearchModel : public QAbstractListModel {
    Q_OBJECT
    QML_NAMED_ELEMENT(ProjectSearch)
    QML_SINGLETON

    Q_PROPERTY(QString query READ query WRITE setQuery NOTIFY queryChanged)
    Q_PROPERTY(bool caseSensitive READ caseSensitive WRITE setCaseSensitive NOTIFY optionsChanged)
    Q_PROPERTY(bool wholeWord READ wholeWord WRITE setWholeWord NOTIFY optionsChanged)
    Q_PROPERTY(bool regularExpression READ regularExpression WRITE setRegularExpression NOTIFY optionsChanged)
    Q_PROPERTY(QString includeGlobs READ includeGlobs WRITE setIncludeGlobs NOTIFY optionsChanged)
    Q_PROPERTY(QString excludeGlobs READ excludeGlobs WRITE setExcludeGlobs NOTIFY optionsChanged)

    Q_PROPERTY(bool searching READ isSearching NOTIFY stateChanged)
    Q_PROPERTY(int matchCount READ matchCount NOTIFY stateChanged)
    Q_PROPERTY(int fileCount READ fileCount NOTIFY stateChanged)
    Q_PROPERTY(bool truncated READ wasTruncated NOTIFY stateChanged)

    /// Set when the user's regular expression does not compile. The panel shows
    /// the reason rather than an empty result list, which would suggest the
    /// pattern was fine and matched nothing.
    Q_PROPERTY(QString patternError READ patternError NOTIFY stateChanged)

    /// True once a search has actually run, so the panel can tell "no results"
    /// apart from "nothing searched for yet".
    Q_PROPERTY(bool hasSearched READ hasSearched NOTIFY stateChanged)

public:
    enum RowKind {
        FileRow = 0,
        MatchRow = 1,
    };
    Q_ENUM(RowKind)

    enum Roles {
        KindRole = Qt::UserRole + 1,
        PathRole,          ///< project-relative, on both kinds of row
        FileNameRole,
        DirectoryRole,
        MatchCountRole,    ///< file rows only
        CollapsedRole,     ///< file rows only
        LineRole,          ///< match rows, one-based for display
        ColumnRole,
        LineTextRole,
        MatchStartRole,    ///< where to highlight within lineText
        MatchLengthRole,
    };

    SearchModel(search::TextSearch& search, QObject* parent = nullptr);

    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    [[nodiscard]] QString query() const { return m_query; }
    void setQuery(const QString& query);

    [[nodiscard]] bool caseSensitive() const { return m_options.caseSensitive; }
    void setCaseSensitive(bool on);
    [[nodiscard]] bool wholeWord() const { return m_options.wholeWord; }
    void setWholeWord(bool on);
    [[nodiscard]] bool regularExpression() const { return m_options.regularExpression; }
    void setRegularExpression(bool on);

    [[nodiscard]] QString includeGlobs() const { return m_includeText; }
    void setIncludeGlobs(const QString& globs);
    [[nodiscard]] QString excludeGlobs() const { return m_excludeText; }
    void setExcludeGlobs(const QString& globs);

    [[nodiscard]] bool isSearching() const;
    [[nodiscard]] int matchCount() const;
    [[nodiscard]] int fileCount() const;
    [[nodiscard]] bool wasTruncated() const;
    [[nodiscard]] QString patternError() const { return m_patternError; }
    [[nodiscard]] bool hasSearched() const { return m_hasSearched; }

    /// Opens the file at the match. File rows collapse instead, which is what
    /// clicking a heading means everywhere else.
    Q_INVOKABLE void activate(int row);

    /// Runs the current query immediately, skipping the debounce. What Enter in
    /// the field does: the user has said they are finished typing.
    Q_INVOKABLE void searchNow();

    Q_INVOKABLE void clear();

    static void setInstance(SearchModel* instance);
    static SearchModel* create(QQmlEngine*, QJSEngine*);

signals:
    void queryChanged();
    void optionsChanged();
    void stateChanged();

    /// A match was chosen. Carries a one-based line and column, which is what
    /// AppController::openFileAt expects.
    void matchActivated(const QString& relativePath, int line, int column);

private:
    /// One row of the flattened list.
    struct Row {
        RowKind kind = FileRow;
        int fileIndex = 0;    ///< index into m_files
        int matchIndex = 0;   ///< index into that file's matches, for match rows
    };

    /// The matches for one file, in the order they were found.
    struct FileGroup {
        QString relativePath;
        std::vector<search::SearchMatch> matches;
        bool collapsed = false;
    };

    /// Rebuilds the grouping and the visible rows from the search's results.
    void rebuild();

    /// Fills m_rows from m_files, honouring each group's collapsed flag.
    /// Callers own the model reset around it.
    void buildRows();

    /// Rebuilds only the visible rows, for a collapse that cannot change the
    /// grouping underneath it.
    void rebuildRows();

    void startSearch();

    search::TextSearch& m_search;

    QString m_query;
    search::SearchOptions m_options;
    QString m_includeText;
    QString m_excludeText;
    QString m_patternError;
    bool m_hasSearched = false;

    std::vector<FileGroup> m_files;
    std::vector<Row> m_rows;

    /// Which files the user has folded away, by path. Kept apart from m_files
    /// because that vector is emptied and rebuilt on every batch of results,
    /// and a fold should outlive the rebuild that prompted it.
    QSet<QString> m_collapsed;

    /// Waits for the typing to stop. Long enough that a fast typist starts one
    /// search rather than ten, short enough to feel immediate.
    QTimer m_debounce;
    static constexpr int kDebounceMs = 200;
};

} // namespace keys::ui
