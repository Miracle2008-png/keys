#include "search/FileIndex.h"

#include "core/Log.h"
#include "core/Trace.h"
#include "filesystem/FileSystem.h"

#include <QDirIterator>
#include <QFileInfo>

using keys::core::CancellationSource;
using keys::core::CancellationToken;
using keys::fs::FileSystem;

namespace keys::search {

FileIndex::FileIndex(project::Project& project, core::TaskScheduler& scheduler,
                     QObject* parent)
    : QObject(parent),
      m_project(project),
      m_scheduler(scheduler),
      m_cancellation(std::make_unique<CancellationSource>())
{
    connect(&m_project, &project::Project::opened, this, [this] { rebuild(); });
    connect(&m_project, &project::Project::closed, this, [this] { clear(); });
}

FileIndex::~FileIndex()
{
    // A walk in flight captures `this`; cancelling means it drops its result
    // rather than posting it to a destroyed object.
    m_cancellation->cancel();
}

void FileIndex::clear()
{
    m_cancellation->cancel();
    m_cancellation = std::make_unique<CancellationSource>();

    if (m_building) {
        m_building = false;
        emit buildingChanged();
    }

    if (!m_files.empty()) {
        m_files.clear();
        emit changed();
    }
}

void FileIndex::rebuild()
{
    // Supersede any walk already running, so a slow one for a previous project
    // cannot deliver into this index.
    m_cancellation->cancel();
    m_cancellation = std::make_unique<CancellationSource>();

    m_files.clear();
    emit changed();

    if (!m_project.isOpen()) {
        return;
    }

    m_building = true;
    emit buildingChanged();

    const QString root = m_project.root();
    const CancellationToken token = m_cancellation->token();

    // The ignore rules are copied rather than referenced: the worker must not
    // read a Project that could be closed underneath it.
    const project::IgnoreRules rules = m_project.ignoreRules();

    m_scheduler.postWithResult<std::vector<IndexedFile>>(
        this,
        [root, rules, token] { return walk(root, rules, token); },
        [this, token](std::vector<IndexedFile> files) {
            if (token.isCancelled()) {
                return;
            }

            m_files = std::move(files);
            m_building = false;

            qCDebug(lcCore) << "indexed" << m_files.size() << "files";

            emit buildingChanged();
            emit changed();
        },
        // Background: the user can work while this runs, and starving an
        // interactive task behind a full project walk would be the wrong trade.
        core::TaskScheduler::Priority::Background);
}

std::vector<IndexedFile> FileIndex::walk(const QString& root,
                                         const project::IgnoreRules& rules,
                                         const CancellationToken& token)
{
    KEYS_TRACE("FileIndex::walk");

    std::vector<IndexedFile> files;

    // QDirIterator rather than recursive listDirectory calls: it does not build
    // an intermediate list per directory, so peak memory stays flat on a deep
    // tree, and cancellation can take effect between entries rather than only
    // between directories.
    //
    // NoSymLinks is what prevents a symlink loop from walking forever - the
    // guard below is a second line of defence rather than the only one.
    QDirIterator iterator(root,
                          QDir::Files | QDir::NoDotAndDotDot | QDir::Hidden | QDir::NoSymLinks,
                          QDirIterator::Subdirectories);

    const int rootLength = root.length() + 1;

    while (iterator.hasNext()) {
        if (token.isCancelled()) {
            return {};
        }

        const QString absolute = FileSystem::normalize(iterator.next());
        if (absolute.length() <= rootLength) {
            continue;
        }

        const QString relative = absolute.mid(rootLength);

        // The project's own ignore rules decide what belongs. Checking here
        // rather than pruning directories means an ignored directory's children
        // are still visited - the simpler correct behaviour, and the cost is
        // bounded because .gitignore's heavy hitters (node_modules, .git) are
        // matched on their first path segment.
        if (rules.isIgnored(relative, false)) {
            continue;
        }

        IndexedFile entry;
        entry.relativePath = relative;

        const int lastSeparator = relative.lastIndexOf(QLatin1Char('/'));
        entry.fileName = lastSeparator >= 0 ? relative.mid(lastSeparator + 1) : relative;

        files.push_back(std::move(entry));

        if (files.size() >= static_cast<size_t>(kMaxFiles)) {
            qCWarning(lcCore) << "file index hit its limit of" << kMaxFiles
                              << "- results will be incomplete";
            break;
        }
    }

    return files;
}

void FileIndex::addFile(const QString& absolutePath)
{
    if (!m_project.isOpen()) {
        return;
    }

    const QString relative = m_project.relativePath(absolutePath);
    if (relative.isEmpty() || m_project.ignoreRules().isIgnored(relative, false)) {
        return;
    }

    // Already present: the watcher can report the same creation more than once.
    const auto existing = std::find_if(m_files.begin(), m_files.end(),
                                       [&relative](const IndexedFile& file) {
                                           return file.relativePath == relative;
                                       });
    if (existing != m_files.end()) {
        return;
    }

    IndexedFile entry;
    entry.relativePath = relative;
    const int lastSeparator = relative.lastIndexOf(QLatin1Char('/'));
    entry.fileName = lastSeparator >= 0 ? relative.mid(lastSeparator + 1) : relative;

    m_files.push_back(std::move(entry));
    emit changed();
}

void FileIndex::removeFile(const QString& absolutePath)
{
    if (!m_project.isOpen()) {
        return;
    }

    const QString relative = m_project.relativePath(absolutePath);
    if (relative.isEmpty()) {
        return;
    }

    const auto removed = std::remove_if(m_files.begin(), m_files.end(),
                                        [&relative](const IndexedFile& file) {
                                            return file.relativePath == relative;
                                        });
    if (removed == m_files.end()) {
        return;
    }

    m_files.erase(removed, m_files.end());
    emit changed();
}

} // namespace keys::search
