#pragma once

#include "vcs/Repository.h"

#include <QAbstractListModel>
#include <QQmlEngine>
#include <QString>

#include <vector>

namespace keys::ui {

/// The changed files, grouped into staged and unstaged.
///
/// **Two sections, one list.** A file can be staged and modified again, so it
/// legitimately appears in both — which a tree of two separate models would
/// make awkward to keep consistent. One flat list with a section role keeps the
/// view simple and the two sections always drawn from the same status.
class SourceControlModel : public QAbstractListModel {
    Q_OBJECT
    QML_NAMED_ELEMENT(SourceControl)
    QML_SINGLETON

    Q_PROPERTY(int count READ count NOTIFY changed)

    /// Repository state, exposed for the panel's header and the status bar.
    Q_PROPERTY(bool hasRepository READ hasRepository NOTIFY changed)
    Q_PROPERTY(bool gitAvailable READ gitAvailable CONSTANT)
    Q_PROPERTY(QString branch READ branch NOTIFY changed)
    Q_PROPERTY(int ahead READ ahead NOTIFY changed)
    Q_PROPERTY(int behind READ behind NOTIFY changed)
    Q_PROPERTY(bool hasUpstream READ hasUpstream NOTIFY changed)
    Q_PROPERTY(QString operationName READ operationName NOTIFY changed)
    Q_PROPERTY(bool refreshing READ refreshing NOTIFY refreshingChanged)

    Q_PROPERTY(int stagedCount READ stagedCount NOTIFY changed)
    Q_PROPERTY(int unstagedCount READ unstagedCount NOTIFY changed)

    /// Whether a commit is possible right now. The button reads this rather than
    /// reimplementing the rule, so a disabled button and a refused commit cannot
    /// disagree.
    Q_PROPERTY(bool canCommit READ canCommit NOTIFY changed)

public:
    enum Section {
        StagedSection = 0,
        UnstagedSection = 1,
    };
    Q_ENUM(Section)

    enum Roles {
        PathRole = Qt::UserRole + 1,
        FileNameRole,
        DirectoryRole,
        SectionRole,
        IsSectionStartRole,
        StatusLetterRole,
        StatusLabelRole,
        IsStagedRole,       ///< which side of the index this row represents
        IsConflictedRole,
    };

    explicit SourceControlModel(vcs::Repository& repository, QObject* parent = nullptr);

    static void setInstance(SourceControlModel* instance);
    static SourceControlModel* create(QQmlEngine* engine, QJSEngine* scriptEngine);

    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    [[nodiscard]] int count() const { return static_cast<int>(m_rows.size()); }

    [[nodiscard]] bool hasRepository() const { return m_repository.isOpen(); }
    [[nodiscard]] static bool gitAvailable() { return vcs::Repository::isGitAvailable(); }

    [[nodiscard]] QString branch() const;
    [[nodiscard]] int ahead() const { return m_repository.status().ahead; }
    [[nodiscard]] int behind() const { return m_repository.status().behind; }
    [[nodiscard]] bool hasUpstream() const { return m_repository.status().hasUpstream(); }
    [[nodiscard]] QString operationName() const;
    [[nodiscard]] bool refreshing() const { return m_repository.isRefreshing(); }

    [[nodiscard]] int stagedCount() const { return m_repository.status().stagedCount(); }
    [[nodiscard]] int unstagedCount() const { return m_repository.status().unstagedCount(); }

    [[nodiscard]] bool canCommit() const;

    // ---- Actions -----------------------------------------------------------

    Q_INVOKABLE void stage(const QString& path);
    Q_INVOKABLE void unstage(const QString& path);
    Q_INVOKABLE void discard(const QString& path);

    Q_INVOKABLE void stageAll();
    Q_INVOKABLE void unstageAll();

    Q_INVOKABLE void commit(const QString& message);
    Q_INVOKABLE void refresh();

    /// Reports that a row was opened. The panel does not open files itself; the
    /// workbench does, the same way the palette reports rather than acts.
    Q_INVOKABLE void activate(const QString& path);

signals:
    void changed();
    void refreshingChanged();

    /// A file was chosen, so the workbench can open it. The panel does not open
    /// files itself, matching how the palette reports rather than acts.
    void fileActivated(const QString& relativePath);

private:
    /// One displayed row: a path on one side of the index.
    struct Row {
        QString path;
        QString fileName;
        QString directory;
        Section section = UnstagedSection;
        vcs::FileStatus status = vcs::FileStatus::Unmodified;
        bool conflicted = false;
    };

    void rebuild();

    /// Absolute path for a row, since the workbench opens absolute paths while
    /// git speaks in paths relative to the repository root.
    [[nodiscard]] QString absolutePathFor(const QString& relativePath) const;

    vcs::Repository& m_repository;
    std::vector<Row> m_rows;
};

} // namespace keys::ui
