#pragma once

#include "core/Result.h"

#include <QDateTime>
#include <QObject>
#include <QString>

namespace keys::workspace {

/// One entry in the recent-projects list.
struct RecentProject {
    QString path;           ///< absolute, normalised
    QString name;           ///< the folder's own name, shown in the list
    QDateTime lastOpened;

    /// Kept at the top of the list and never trimmed. The projects someone
    /// returns to for months would otherwise be pushed out by a week of
    /// one-off folders, which is exactly when the list stops being useful.
    bool pinned = false;
};

/// The most recently opened projects, newest first.
///
/// Persisted separately from settings: this is history rather than preference,
/// it changes on every launch, and mixing it into settings.json would make that
/// file churn constantly and be unpleasant to hand-edit.
///
/// Entries whose folder no longer exists are dropped when the list is loaded, so
/// the welcome screen never offers a project that cannot be opened.
class RecentProjects : public QObject {
    Q_OBJECT

public:
    explicit RecentProjects(QObject* parent = nullptr);

    /// Records a project as opened now, moving it to the front if already
    /// present, and trims the list to kMaxEntries.
    void record(const QString& path);

    /// Removes one entry - for a project the user no longer wants listed, or one
    /// that has been found to be missing.
    void remove(const QString& path);

    /// Pins or unpins a project. Pinned entries sort above the rest and survive
    /// trimming; unpinning returns one to its place in recency order.
    void setPinned(const QString& path, bool pinned);

    void clear();

    [[nodiscard]] const QList<RecentProject>& entries() const { return m_entries; }
    [[nodiscard]] int count() const { return static_cast<int>(m_entries.size()); }

    /// Default location: alongside settings, in the application config directory.
    [[nodiscard]] static QString defaultPath();

    /// Loads from JSON. A missing file is not an error - it means nothing has
    /// been opened yet. Entries pointing at folders that no longer exist are
    /// dropped during load.
    core::Status load(const QString& path);

    core::Status save(const QString& path) const;

    /// Longer than a menu would show, so the welcome screen can present a
    /// generous list while the palette shows only the top few.
    static constexpr int kMaxEntries = 20;

signals:
    void changed();

private:
    /// Pinned first, then by recency. Called after any change that could
    /// disturb the order.
    void sortEntries();

    QList<RecentProject> m_entries;
};

} // namespace keys::workspace
