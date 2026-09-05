#include "workspace/RecentProjects.h"

#include "core/Log.h"
#include "filesystem/FileSystem.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>

using keys::core::ErrorCode;
using keys::core::Err;
using keys::core::Ok;
using keys::core::Status;
using keys::fs::FileSystem;

namespace keys::workspace {

RecentProjects::RecentProjects(QObject* parent) : QObject(parent) {}

QString RecentProjects::defaultPath()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    return QDir(dir).filePath(QStringLiteral("recent-projects.json"));
}

void RecentProjects::record(const QString& path)
{
    const QString normalized = FileSystem::normalize(path);
    if (normalized.isEmpty()) {
        return;
    }

    // Remove any existing entry first so re-opening moves a project to the front
    // rather than creating a duplicate.
    m_entries.removeIf([&normalized](const RecentProject& entry) {
        return entry.path == normalized;
    });

    RecentProject entry;
    entry.path = normalized;
    entry.name = QFileInfo(normalized).fileName();
    if (entry.name.isEmpty()) {
        entry.name = normalized;   // a drive root has no file name of its own
    }
    entry.lastOpened = QDateTime::currentDateTime();

    m_entries.prepend(std::move(entry));

    while (m_entries.size() > kMaxEntries) {
        m_entries.removeLast();
    }

    emit changed();
}

void RecentProjects::remove(const QString& path)
{
    const QString normalized = FileSystem::normalize(path);
    if (m_entries.removeIf([&normalized](const RecentProject& entry) {
            return entry.path == normalized;
        }) > 0) {
        emit changed();
    }
}

void RecentProjects::clear()
{
    if (m_entries.isEmpty()) {
        return;
    }
    m_entries.clear();
    emit changed();
}

Status RecentProjects::load(const QString& path)
{
    QFile file(path);
    if (!file.exists()) {
        // Nothing opened yet. An ordinary first-run state.
        return Ok();
    }
    if (!file.open(QIODevice::ReadOnly)) {
        return Err(ErrorCode::IoError, file.errorString(), path);
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        return Err(ErrorCode::ParseError, parseError.errorString(), path);
    }
    if (!document.isArray()) {
        return Err(ErrorCode::ParseError,
                   QStringLiteral("Expected a JSON array at the top level"), path);
    }

    m_entries.clear();

    const QJsonArray array = document.array();
    for (const QJsonValue& value : array) {
        if (!value.isObject()) {
            continue;
        }
        const QJsonObject object = value.toObject();

        RecentProject entry;
        entry.path = FileSystem::normalize(object.value(QStringLiteral("path")).toString());
        if (entry.path.isEmpty()) {
            continue;
        }

        // Drop projects that have been moved or deleted since they were recorded.
        // Offering one that cannot open would waste the user's click and force
        // them to see an error for something Keys already knew.
        if (!FileSystem::isDirectory(entry.path)) {
            qCDebug(lcCore) << "dropping missing recent project" << entry.path;
            continue;
        }

        entry.name = object.value(QStringLiteral("name")).toString();
        if (entry.name.isEmpty()) {
            entry.name = QFileInfo(entry.path).fileName();
        }
        entry.lastOpened = QDateTime::fromString(
            object.value(QStringLiteral("lastOpened")).toString(), Qt::ISODate);

        m_entries.append(std::move(entry));

        if (m_entries.size() >= kMaxEntries) {
            break;
        }
    }

    emit changed();
    return Ok();
}

Status RecentProjects::save(const QString& path) const
{
    const QFileInfo info(path);
    if (!info.dir().exists() && !QDir().mkpath(info.absolutePath())) {
        return Err(ErrorCode::IoError,
                   QStringLiteral("Could not create the configuration directory"), path);
    }

    QJsonArray array;
    for (const RecentProject& entry : m_entries) {
        QJsonObject object;
        object.insert(QStringLiteral("path"), entry.path);
        object.insert(QStringLiteral("name"), entry.name);
        object.insert(QStringLiteral("lastOpened"), entry.lastOpened.toString(Qt::ISODate));
        array.append(object);
    }

    // Atomic, like settings: an interrupted write must not corrupt the history.
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return Err(ErrorCode::IoError, file.errorString(), path);
    }
    if (file.write(QJsonDocument(array).toJson(QJsonDocument::Indented)) == -1) {
        return Err(ErrorCode::IoError, file.errorString(), path);
    }
    if (!file.commit()) {
        return Err(ErrorCode::IoError, file.errorString(), path);
    }
    return Ok();
}

} // namespace keys::workspace
