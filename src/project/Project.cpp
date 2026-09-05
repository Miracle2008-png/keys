#include "project/Project.h"

#include "core/Log.h"
#include "core/Trace.h"
#include "filesystem/FileSystem.h"

#include <QDir>
#include <QFileInfo>

using keys::core::ErrorCode;
using keys::core::Err;
using keys::core::Ok;
using keys::core::Status;
using keys::fs::FileSystem;

namespace keys::project {
namespace {

/// Marker files that identify a project's kind, most specific first. Order
/// matters: a Rust project with a package.json for its web assets is still a
/// Rust project, so Cargo.toml is checked before package.json.
struct KindMarker {
    ProjectKind kind;
    const char* fileName;
};

const QList<KindMarker>& kindMarkers()
{
    static const QList<KindMarker> markers = {
        {ProjectKind::Rust, "Cargo.toml"},
        {ProjectKind::Go, "go.mod"},
        {ProjectKind::Cpp, "CMakeLists.txt"},
        {ProjectKind::Cpp, "meson.build"},
        {ProjectKind::Python, "pyproject.toml"},
        {ProjectKind::Python, "setup.py"},
        {ProjectKind::Python, "requirements.txt"},
        {ProjectKind::Node, "package.json"},
    };
    return markers;
}

} // namespace

Project::Project(QObject* parent) : QObject(parent) {}

Status Project::open(const QString& rootPath)
{
    KEYS_TRACE("Project::open");

    const QString normalized = FileSystem::normalize(rootPath);
    if (normalized.isEmpty()) {
        return Err(ErrorCode::InvalidArgument, QStringLiteral("No path was given"));
    }

    const QFileInfo info(normalized);
    if (!info.exists()) {
        return Err(ErrorCode::NotFound, QStringLiteral("Folder does not exist"), normalized);
    }
    if (!info.isDir()) {
        return Err(ErrorCode::InvalidArgument,
                   QStringLiteral("Path is a file, not a folder"), normalized);
    }
    if (!info.isReadable()) {
        return Err(ErrorCode::PermissionDenied,
                   QStringLiteral("Folder cannot be read"), normalized);
    }

    // Close first so a failed open never leaves the previous project half-replaced.
    close();

    m_root = normalized;
    m_name = info.fileName();

    // A drive root ("C:/") has no file name of its own; fall back to the path so
    // the top bar is never blank.
    if (m_name.isEmpty()) {
        m_name = normalized;
    }

    m_isGitRepository = FileSystem::exists(QDir(m_root).filePath(QStringLiteral(".git")));

    detectKind();
    loadRootIgnoreRules();

    qCDebug(lcCore) << "opened project" << m_name << "at" << m_root;
    emit opened(m_root);
    return Ok();
}

void Project::close()
{
    if (m_root.isEmpty()) {
        return;
    }

    m_root.clear();
    m_name.clear();
    m_kind = ProjectKind::Unknown;
    m_isGitRepository = false;

    // Reset to built-in defaults, not to empty: the next project inherits no
    // rules from this one, but still skips .git and node_modules.
    m_ignoreRules.clear();
    m_ignoreRules.addDefaults();

    emit closed();
}

void Project::detectKind()
{
    const QDir dir(m_root);
    for (const KindMarker& marker : kindMarkers()) {
        if (FileSystem::exists(dir.filePath(QString::fromLatin1(marker.fileName)))) {
            m_kind = marker.kind;
            return;
        }
    }
    m_kind = ProjectKind::Unknown;
}

void Project::loadRootIgnoreRules()
{
    const QString path = QDir(m_root).filePath(QStringLiteral(".gitignore"));
    if (!FileSystem::exists(path)) {
        return;
    }

    const core::Result<QString> contents = FileSystem::readTextFile(path);
    if (!contents) {
        // A project without readable ignore rules still opens; search results are
        // merely noisier. Not worth blocking the user over.
        qCWarning(lcCore) << "could not read .gitignore:" << contents.error().toString();
        return;
    }

    m_ignoreRules.addPatterns(contents.value());
}

QString Project::relativePath(const QString& absolutePath) const
{
    if (m_root.isEmpty()) {
        return {};
    }

    const QString normalized = FileSystem::normalize(absolutePath);
    if (normalized == m_root) {
        return {};
    }
    if (!contains(normalized)) {
        return {};
    }

    // +1 to drop the separator that follows the root.
    return normalized.mid(m_root.length() + 1);
}

bool Project::contains(const QString& absolutePath) const
{
    if (m_root.isEmpty()) {
        return false;
    }

    const QString normalized = FileSystem::normalize(absolutePath);
    if (normalized == m_root) {
        return true;
    }

    // The trailing separator matters: without it "/home/proj-backup" would count
    // as inside "/home/proj". Path comparison is case-sensitive even on Windows,
    // because normalize() does not case-fold and treating "SRC" and "src" as the
    // same path would be wrong on a case-sensitive volume.
    return normalized.startsWith(m_root + QLatin1Char('/'));
}

} // namespace keys::project
