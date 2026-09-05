#pragma once

#include "core/Cancellation.h"
#include "core/TaskScheduler.h"
#include "project/Project.h"

#include <QObject>
#include <QString>
#include <QStringList>

#include <memory>

namespace keys::search {

/// One file in the index.
struct IndexedFile {
    /// Path relative to the project root, with forward slashes. Relative because
    /// that is what quick open displays and matches against — the root prefix is
    /// identical for every entry and would only dilute the fuzzy score.
    QString relativePath;

    /// Cached so quick open does not re-derive it per keystroke per file.
    QString fileName;
};

/// The list of files in the project, built in the background.
///
/// **Why an index at all.** Quick open has to answer within 100 ms on a
/// 50,000-file project. Walking the filesystem per keystroke cannot do that —
/// the directory reads alone would take longer. So the tree is walked once, off
/// the UI thread, and every keystroke then scores against a flat in-memory list,
/// which is a few milliseconds of pure CPU.
///
/// **Why it stays flat.** A trie or n-gram index would make prefix queries
/// faster, but fuzzy matching is a subsequence test, not a prefix test, so no
/// such structure prunes it. Scoring the flat list is O(files) with a small
/// constant, and 50,000 short strings is well inside the budget — measured, not
/// assumed. Milestone 15 revisits this if a real project proves otherwise.
///
/// **Idle cost is zero.** The index is built once and then maintained
/// incrementally from the file watcher; nothing polls.
class FileIndex : public QObject {
    Q_OBJECT

public:
    FileIndex(project::Project& project, core::TaskScheduler& scheduler,
              QObject* parent = nullptr);
    ~FileIndex() override;

    /// Rebuilds from scratch, cancelling any walk in progress.
    void rebuild();

    /// Discards everything, for a project closing.
    void clear();

    [[nodiscard]] int fileCount() const { return static_cast<int>(m_files.size()); }
    [[nodiscard]] bool isBuilding() const { return m_building; }

    /// The indexed files. Safe to read from the UI thread: the worker builds
    /// into a separate list and swaps it in on this thread when complete.
    [[nodiscard]] const std::vector<IndexedFile>& files() const { return m_files; }

    /// Adds a file the watcher reported as created. Ignored if already present
    /// or if the project's ignore rules exclude it.
    void addFile(const QString& absolutePath);

    /// Removes a file the watcher reported as deleted.
    void removeFile(const QString& absolutePath);

signals:
    void buildingChanged();

    /// The index contents changed: a build finished, or a file was added or
    /// removed. Quick open re-runs its query on this.
    void changed();

private:
    /// Walks the project tree. Runs on a worker; returns the whole list rather
    /// than reporting progress, because a half-built index shown in quick open
    /// would rank badly and then reorder under the user.
    [[nodiscard]] static std::vector<IndexedFile> walk(const QString& root,
                                                       const project::IgnoreRules& rules,
                                                       const core::CancellationToken& token);

    project::Project& m_project;
    core::TaskScheduler& m_scheduler;

    std::vector<IndexedFile> m_files;

    /// Cancels a walk in progress when the project changes, so a slow walk of a
    /// closed project cannot deliver its results into the next one.
    std::unique_ptr<core::CancellationSource> m_cancellation;

    bool m_building = false;

    /// A guard against walking an unbounded tree: a project containing a
    /// symlink loop, or one opened at a drive root by accident, must not consume
    /// the machine. Well above any real project.
    static constexpr int kMaxFiles = 200000;
};

} // namespace keys::search
