#pragma once

#include "core/Result.h"
#include "project/IgnoreRules.h"

#include <QObject>
#include <QString>

namespace keys::project {

/// What kind of project this is, inferred from marker files in the root.
///
/// Used to pick sensible build/run defaults later. It is a hint, not a contract:
/// a project with no recognised marker is perfectly valid and simply gets no
/// language-specific defaults.
enum class ProjectKind {
    Unknown,
    Cpp,        ///< CMakeLists.txt, meson.build
    Node,       ///< package.json
    Python,     ///< pyproject.toml, setup.py, requirements.txt
    Rust,       ///< Cargo.toml
    Go,         ///< go.mod
};

/// One opened project: a root directory plus what Keys has learned about it.
///
/// Deliberately not a workspace: a workspace may later hold several project
/// roots, so keeping "one root" and "the set of open roots" as separate concepts
/// avoids a rewrite when multi-root support arrives.
class Project : public QObject {
    Q_OBJECT

public:
    explicit Project(QObject* parent = nullptr);

    /// Opens `rootPath`. Fails if it does not exist or is not a directory.
    /// Loads the root .gitignore if present; nested ones are read lazily as the
    /// explorer descends, so opening a large project stays fast.
    core::Status open(const QString& rootPath);

    void close();

    [[nodiscard]] bool isOpen() const { return !m_root.isEmpty(); }

    /// Absolute, normalised path of the project root.
    [[nodiscard]] const QString& root() const { return m_root; }

    /// The directory's own name, shown in the top bar.
    [[nodiscard]] const QString& name() const { return m_name; }

    [[nodiscard]] ProjectKind kind() const { return m_kind; }

    /// True if the root contains a .git directory. The vcs module owns the real
    /// Git integration; this is only the cheap check the UI needs to know
    /// whether to offer source control at all.
    [[nodiscard]] bool isGitRepository() const { return m_isGitRepository; }

    [[nodiscard]] const IgnoreRules& ignoreRules() const { return m_ignoreRules; }

    /// Converts an absolute path to one relative to the root. Returns an empty
    /// string if the path lies outside the project, which callers must treat as
    /// "not part of this project" rather than as the root itself.
    [[nodiscard]] QString relativePath(const QString& absolutePath) const;

    /// True if `absolutePath` is inside the project root. Guards operations that
    /// must not escape the project - a rename or delete driven by a path from
    /// outside should never be executed against the user's wider filesystem.
    [[nodiscard]] bool contains(const QString& absolutePath) const;

signals:
    void opened(const QString& root);
    void closed();

private:
    /// Inspects the root for marker files to infer the project kind.
    void detectKind();
    void loadRootIgnoreRules();

    QString m_root;
    QString m_name;
    ProjectKind m_kind = ProjectKind::Unknown;
    bool m_isGitRepository = false;
    IgnoreRules m_ignoreRules;
};

} // namespace keys::project
