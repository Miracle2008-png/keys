#include "ui/SearchModel.h"

#include <QDir>
#include <QFileInfo>
#include <QQmlEngine>
#include <QRegularExpression>

#include <algorithm>

namespace keys::ui {
namespace {

SearchModel* g_instance = nullptr;

/// Splits a comma-separated glob field into patterns, dropping the empties a
/// trailing comma leaves behind.
QStringList splitGlobs(const QString& text)
{
    QStringList globs;
    for (const QString& part : text.split(QLatin1Char(','), Qt::SkipEmptyParts)) {
        const QString trimmed = part.trimmed();
        if (!trimmed.isEmpty()) {
            globs.append(trimmed);
        }
    }
    return globs;
}

} // namespace

SearchModel::SearchModel(search::TextSearch& search, QObject* parent)
    : QAbstractListModel(parent), m_search(search)
{
    m_debounce.setSingleShot(true);
    m_debounce.setInterval(kDebounceMs);
    connect(&m_debounce, &QTimer::timeout, this, &SearchModel::startSearch);

    // Results arrive in batches, so the list fills in as the project is scanned
    // rather than appearing all at once when the scan finishes.
    connect(&m_search, &search::TextSearch::resultsChanged,
            this, &SearchModel::rebuild);

    connect(&m_search, &search::TextSearch::searchingChanged,
            this, &SearchModel::stateChanged);

    connect(&m_search, &search::TextSearch::finished, this, [this] {
        rebuild();
        emit stateChanged();
    });
}

int SearchModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_rows.size());
}

QVariant SearchModel::data(const QModelIndex& index, int role) const
{
    if (index.row() < 0 || index.row() >= static_cast<int>(m_rows.size())) {
        return {};
    }

    const Row& row = m_rows.at(static_cast<size_t>(index.row()));
    const FileGroup& file = m_files.at(static_cast<size_t>(row.fileIndex));

    switch (role) {
    case KindRole:
        return row.kind;
    case PathRole:
        return file.relativePath;
    case FileNameRole:
        return QFileInfo(file.relativePath).fileName();
    case DirectoryRole: {
        const QString directory = QFileInfo(file.relativePath).path();
        return directory == QLatin1String(".") ? QString() : directory;
    }
    case MatchCountRole:
        return static_cast<int>(file.matches.size());
    case CollapsedRole:
        return file.collapsed;
    default:
        break;
    }

    if (row.kind != MatchRow) {
        return {};
    }

    const search::SearchMatch& match =
        file.matches.at(static_cast<size_t>(row.matchIndex));

    switch (role) {
    case LineRole:
        return match.line + 1;      // stored zero-based, shown one-based
    case ColumnRole:
        return match.column + 1;
    case LineTextRole:
        return match.lineText;
    case MatchStartRole:
        return match.column;
    case MatchLengthRole:
        return match.length;
    default:
        return {};
    }
}

QHash<int, QByteArray> SearchModel::roleNames() const
{
    return {
        {KindRole, "kind"},
        {PathRole, "path"},
        {FileNameRole, "fileName"},
        {DirectoryRole, "directory"},
        {MatchCountRole, "matchCount"},
        {CollapsedRole, "collapsed"},
        {LineRole, "line"},
        {ColumnRole, "column"},
        {LineTextRole, "lineText"},
        {MatchStartRole, "matchStart"},
        {MatchLengthRole, "matchLength"},
    };
}

void SearchModel::setQuery(const QString& query)
{
    if (m_query == query) {
        return;
    }
    m_query = query;
    emit queryChanged();

    if (m_query.isEmpty()) {
        clear();
        return;
    }
    m_debounce.start();
}

void SearchModel::setCaseSensitive(bool on)
{
    if (m_options.caseSensitive == on) {
        return;
    }
    m_options.caseSensitive = on;
    emit optionsChanged();
    m_debounce.start();
}

void SearchModel::setWholeWord(bool on)
{
    if (m_options.wholeWord == on) {
        return;
    }
    m_options.wholeWord = on;
    emit optionsChanged();
    m_debounce.start();
}

void SearchModel::setRegularExpression(bool on)
{
    if (m_options.regularExpression == on) {
        return;
    }
    m_options.regularExpression = on;
    emit optionsChanged();
    m_debounce.start();
}

void SearchModel::setIncludeGlobs(const QString& globs)
{
    if (m_includeText == globs) {
        return;
    }
    m_includeText = globs;
    emit optionsChanged();
    m_debounce.start();
}

void SearchModel::setExcludeGlobs(const QString& globs)
{
    if (m_excludeText == globs) {
        return;
    }
    m_excludeText = globs;
    emit optionsChanged();
    m_debounce.start();
}

bool SearchModel::isSearching() const
{
    return m_search.isSearching();
}

int SearchModel::matchCount() const
{
    return m_search.resultCount();
}

int SearchModel::fileCount() const
{
    return static_cast<int>(m_files.size());
}

bool SearchModel::wasTruncated() const
{
    return m_search.wasTruncated();
}

void SearchModel::searchNow()
{
    m_debounce.stop();
    startSearch();
}

void SearchModel::startSearch()
{
    m_options.query = m_query;
    m_options.includeGlobs = splitGlobs(m_includeText);
    m_options.excludeGlobs = splitGlobs(m_excludeText);

    // Checked here rather than left to TextSearch, which treats an uncompilable
    // pattern as the user still typing and stays quiet. That is right for the
    // search; it is wrong for the panel, where an empty list would imply the
    // pattern was fine and matched nothing.
    m_patternError.clear();
    if (m_options.regularExpression && !m_query.isEmpty()) {
        const QRegularExpression pattern(m_query);
        if (!pattern.isValid()) {
            m_patternError = pattern.errorString();

            beginResetModel();
            m_files.clear();
            m_rows.clear();
            endResetModel();

            m_hasSearched = true;
            emit stateChanged();
            return;
        }
    }

    m_hasSearched = true;
    m_search.search(m_options);
    emit stateChanged();
}

void SearchModel::rebuild()
{
    // Collapse state is remembered in m_collapsed rather than read back off
    // m_files, because starting a search cancels the previous one and empties
    // its results first: by the time the new batches arrive there is nothing
    // left to read the flags from, and every file would spring open again.
    for (const FileGroup& file : m_files) {
        if (file.collapsed) {
            m_collapsed.insert(file.relativePath);
        } else {
            m_collapsed.remove(file.relativePath);
        }
    }

    beginResetModel();

    m_files.clear();

    // Results arrive grouped by file already - one task per file - but batches
    // from different workers interleave, so the same file can appear in more
    // than one batch. Indexed by path rather than assuming adjacency.
    QHash<QString, qsizetype> indexByPath;
    for (const search::SearchMatch& match : m_search.results()) {
        const auto it = indexByPath.constFind(match.relativePath);
        qsizetype index = 0;
        if (it == indexByPath.cend()) {
            index = static_cast<qsizetype>(m_files.size());
            indexByPath.insert(match.relativePath, index);

            FileGroup group;
            group.relativePath = match.relativePath;
            group.collapsed = m_collapsed.contains(match.relativePath);
            m_files.push_back(std::move(group));
        } else {
            index = it.value();
        }
        m_files.at(static_cast<size_t>(index)).matches.push_back(match);
    }

    // Alphabetical, so the list does not reorder itself as batches land. Within
    // a file the matches are sorted into document order, which is not
    // necessarily the order the workers found them in.
    std::sort(m_files.begin(), m_files.end(),
              [](const FileGroup& a, const FileGroup& b) {
                  return a.relativePath.compare(b.relativePath, Qt::CaseInsensitive) < 0;
              });

    for (FileGroup& file : m_files) {
        std::sort(file.matches.begin(), file.matches.end(),
                  [](const search::SearchMatch& a, const search::SearchMatch& b) {
                      return a.line != b.line ? a.line < b.line : a.column < b.column;
                  });
    }

    buildRows();

    endResetModel();
    emit stateChanged();
}

void SearchModel::buildRows()
{
    m_rows.clear();
    for (size_t f = 0; f < m_files.size(); ++f) {
        m_rows.push_back({FileRow, static_cast<int>(f), 0});
        if (m_files.at(f).collapsed) {
            continue;
        }
        for (size_t m = 0; m < m_files.at(f).matches.size(); ++m) {
            m_rows.push_back({MatchRow, static_cast<int>(f), static_cast<int>(m)});
        }
    }
}

void SearchModel::rebuildRows()
{
    beginResetModel();
    buildRows();
    endResetModel();
}

void SearchModel::activate(int row)
{
    if (row < 0 || row >= static_cast<int>(m_rows.size())) {
        return;
    }

    const Row entry = m_rows.at(static_cast<size_t>(row));

    if (entry.kind == FileRow) {
        // A heading toggles its section rather than opening the file: that is
        // what clicking a heading means in every other panel.
        FileGroup& file = m_files.at(static_cast<size_t>(entry.fileIndex));
        file.collapsed = !file.collapsed;
        if (file.collapsed) {
            m_collapsed.insert(file.relativePath);
        } else {
            m_collapsed.remove(file.relativePath);
        }
        rebuildRows();
        return;
    }

    const FileGroup& file = m_files.at(static_cast<size_t>(entry.fileIndex));
    const search::SearchMatch& match =
        file.matches.at(static_cast<size_t>(entry.matchIndex));

    emit matchActivated(file.relativePath, match.line + 1, match.column + 1);
}

void SearchModel::clear()
{
    m_debounce.stop();
    m_search.cancel();

    beginResetModel();
    m_files.clear();
    m_rows.clear();
    endResetModel();

    m_collapsed.clear();
    m_patternError.clear();
    m_hasSearched = false;
    emit stateChanged();
}

void SearchModel::setInstance(SearchModel* instance)
{
    g_instance = instance;
}

SearchModel* SearchModel::create(QQmlEngine*, QJSEngine*)
{
    Q_ASSERT_X(g_instance, "SearchModel::create",
               "SearchModel::setInstance was not called");

    QQmlEngine::setObjectOwnership(g_instance, QQmlEngine::CppOwnership);
    return g_instance;
}

} // namespace keys::ui
