#include "ui/SourceControlModel.h"

#include <QDir>

#include <algorithm>

namespace keys::ui {
namespace {

/// The instance main() publishes for QML; see Theme.cpp for the reasoning.
SourceControlModel* g_instance = nullptr;

} // namespace

void SourceControlModel::setInstance(SourceControlModel* instance)
{
    g_instance = instance;
}

SourceControlModel* SourceControlModel::create(QQmlEngine* engine, QJSEngine* scriptEngine)
{
    Q_UNUSED(engine)
    Q_UNUSED(scriptEngine)

    Q_ASSERT_X(g_instance, "SourceControlModel::create",
               "SourceControlModel::setInstance was not called");

    QQmlEngine::setObjectOwnership(g_instance, QQmlEngine::CppOwnership);
    return g_instance;
}

SourceControlModel::SourceControlModel(vcs::Repository& repository, QObject* parent)
    : QAbstractListModel(parent), m_repository(repository)
{
    connect(&m_repository, &vcs::Repository::statusChanged,
            this, &SourceControlModel::rebuild);
    connect(&m_repository, &vcs::Repository::repositoryChanged,
            this, &SourceControlModel::rebuild);
    connect(&m_repository, &vcs::Repository::refreshingChanged,
            this, &SourceControlModel::refreshingChanged);

    rebuild();
}

QString SourceControlModel::branch() const
{
    const vcs::RepositoryStatus& status = m_repository.status();
    if (!status.isDetached()) {
        return status.branch;
    }
    // Detached: there is no branch to name, so the commit is what identifies
    // where the user is.
    return status.headCommit.isEmpty() ? QString()
                                       : QStringLiteral("detached at %1").arg(status.headCommit);
}

QString SourceControlModel::operationName() const
{
    return m_repository.status().operationName;
}

bool SourceControlModel::canCommit() const
{
    const vcs::RepositoryStatus& status = m_repository.status();

    // Nothing staged means git would refuse anyway; conflicts mean the user has
    // a merge to finish first. Both are stated here rather than in QML so the
    // button and the command cannot disagree about the rule.
    return m_repository.isOpen() && status.stagedCount() > 0 && !status.hasConflicts();
}

void SourceControlModel::rebuild()
{
    beginResetModel();
    m_rows.clear();

    for (const vcs::FileChange& change : m_repository.status().changes) {
        const int separator = change.path.lastIndexOf(QLatin1Char('/'));

        const auto makeRow = [&](Section section, vcs::FileStatus status) {
            Row row;
            row.path = change.path;
            row.fileName = separator >= 0 ? change.path.mid(separator + 1) : change.path;
            row.directory = separator > 0 ? change.path.left(separator) : QString();
            row.section = section;
            row.status = status;
            row.conflicted = change.isConflicted();
            return row;
        };

        // A file staged and then modified again appears in both sections. That
        // is the truth of the index, and hiding one of them would leave the user
        // unable to act on it.
        if (change.hasStagedChange()) {
            m_rows.push_back(makeRow(StagedSection, change.staged));
        }
        if (change.hasUnstagedChange() || change.unstaged == vcs::FileStatus::Untracked) {
            m_rows.push_back(makeRow(UnstagedSection, change.unstaged));
        }
    }

    // Staged first, then by path, so the list is stable between refreshes rather
    // than following whatever order git happened to emit.
    std::sort(m_rows.begin(), m_rows.end(), [](const Row& a, const Row& b) {
        return a.section != b.section ? a.section < b.section : a.path < b.path;
    });

    endResetModel();
    emit changed();
}

int SourceControlModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_rows.size());
}

QVariant SourceControlModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= rowCount()) {
        return {};
    }

    const Row& row = m_rows.at(static_cast<size_t>(index.row()));

    switch (role) {
    case PathRole:
        return row.path;
    case FileNameRole:
        return row.fileName;
    case DirectoryRole:
        return row.directory;
    case SectionRole:
        return static_cast<int>(row.section);
    case IsSectionStartRole:
        return index.row() == 0
               || m_rows.at(static_cast<size_t>(index.row()) - 1).section != row.section;
    case StatusLetterRole:
        return vcs::fileStatusLetter(row.status);
    case StatusLabelRole:
        return vcs::fileStatusLabel(row.status);
    case IsStagedRole:
        return row.section == StagedSection;
    case IsConflictedRole:
        return row.conflicted;
    default:
        return {};
    }
}

QHash<int, QByteArray> SourceControlModel::roleNames() const
{
    return {
        {PathRole, "path"},
        {FileNameRole, "fileName"},
        {DirectoryRole, "directory"},
        {SectionRole, "section"},
        {IsSectionStartRole, "isSectionStart"},
        {StatusLetterRole, "statusLetter"},
        {StatusLabelRole, "statusLabel"},
        {IsStagedRole, "isStaged"},
        {IsConflictedRole, "isConflicted"},
    };
}

QString SourceControlModel::absolutePathFor(const QString& relativePath) const
{
    const QString root = m_repository.root();
    return root.isEmpty() ? relativePath : QDir(root).filePath(relativePath);
}

void SourceControlModel::stage(const QString& path)
{
    m_repository.stage({path});
}

void SourceControlModel::unstage(const QString& path)
{
    m_repository.unstage({path});
}

void SourceControlModel::discard(const QString& path)
{
    // Confirmed by the caller: this cannot be undone, and the model is not the
    // right place to raise a dialog.
    m_repository.discard({path});
}

void SourceControlModel::stageAll()
{
    m_repository.stage({});   // empty means everything, matching `git add -A`
}

void SourceControlModel::unstageAll()
{
    m_repository.unstage({});
}

void SourceControlModel::commit(const QString& message)
{
    m_repository.commit(message);
}

void SourceControlModel::refresh()
{
    m_repository.refresh();
}

void SourceControlModel::activate(const QString& path)
{
    // Absolute, because git speaks in paths relative to the repository root
    // while the workbench opens absolute ones - and the repository root is not
    // always the project root.
    emit fileActivated(absolutePathFor(path));
}

} // namespace keys::ui
