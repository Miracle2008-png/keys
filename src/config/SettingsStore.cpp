#include "config/SettingsStore.h"

#include "core/Log.h"

#include <QDir>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>

using keys::core::ErrorCode;
using keys::core::Err;
using keys::core::Ok;
using keys::core::Status;

namespace keys::config {
namespace {

constexpr auto kSettingsFileName = "settings.json";

/// Flattens a nested JSON object into the dotted keys the schema uses, so
/// {"editor":{"fontSize":13}} on disk becomes "editor.fontSize" in memory. The
/// nested form is what a human editing the file by hand expects to see.
void flatten(const QJsonObject& object, const QString& prefix, QHash<QString, QVariant>& out)
{
    for (auto it = object.constBegin(); it != object.constEnd(); ++it) {
        const QString key = prefix.isEmpty() ? it.key() : prefix + QLatin1Char('.') + it.key();
        if (it.value().isObject()) {
            flatten(it.value().toObject(), key, out);
        } else {
            out.insert(key, it.value().toVariant());
        }
    }
}

/// Inverse of flatten(): turns dotted keys back into nested objects for writing.
QJsonObject nest(const QHash<QString, QVariant>& values)
{
    QJsonObject root;
    for (auto it = values.cbegin(); it != values.cend(); ++it) {
        const QStringList parts = it.key().split(QLatin1Char('.'));

        // Walk down building objects, then set the leaf. Qt's JSON values are
        // copies rather than references, so the branch is rebuilt on the way out.
        QJsonValue leaf = QJsonValue::fromVariant(it.value());
        QList<QJsonObject> chain;
        QJsonObject current = root;
        for (int i = 0; i < parts.size() - 1; ++i) {
            chain.append(current);
            current = current.value(parts.at(i)).toObject();
        }
        current.insert(parts.constLast(), leaf);
        for (int i = parts.size() - 2; i >= 0; --i) {
            QJsonObject parent = chain.at(i);
            parent.insert(parts.at(i), current);
            current = parent;
        }
        root = current;
    }
    return root;
}

} // namespace

QString SettingsStore::userSettingsPath()
{
    const QString dir =
        QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    return QDir(dir).filePath(QLatin1String(kSettingsFileName));
}

QString SettingsStore::workspaceSettingsPath(const QString& projectRoot)
{
    return QDir(projectRoot).filePath(QLatin1String(".keys/") + QLatin1String(kSettingsFileName));
}

Status SettingsStore::load(Settings& settings, Settings::Layer layer, const QString& path)
{
    QFile file(path);
    if (!file.exists()) {
        // First run, or no workspace overrides. Both are ordinary states, not
        // failures — leave the layer empty and carry on.
        settings.replaceLayer(layer, {});
        return Ok();
    }

    if (!file.open(QIODevice::ReadOnly)) {
        return Err(ErrorCode::IoError, file.errorString(), path);
    }

    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError) {
        // Do not silently discard a settings file the user may have hand-edited;
        // report it so they can fix the typo rather than lose their configuration.
        return Err(ErrorCode::ParseError, parseError.errorString(), path);
    }
    if (!document.isObject()) {
        return Err(ErrorCode::ParseError,
                   QStringLiteral("Expected a JSON object at the top level"), path);
    }

    QHash<QString, QVariant> values;
    flatten(document.object(), QString(), values);

    // replaceLayer validates each key against the schema and drops what does not
    // belong, so a stale or malformed entry cannot poison the store.
    settings.replaceLayer(layer, values);

    qCDebug(lcConfig) << "loaded" << values.size() << "settings from" << path;
    return Ok();
}

Status SettingsStore::save(const Settings& settings, Settings::Layer layer, const QString& path)
{
    const QFileInfo info(path);
    if (!info.dir().exists() && !QDir().mkpath(info.absolutePath())) {
        return Err(ErrorCode::IoError,
                   QStringLiteral("Could not create the settings directory"), path);
    }

    // QSaveFile writes to a temporary and renames on commit, so an interrupted
    // save leaves the previous settings intact rather than a truncated file.
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) {
        return Err(ErrorCode::IoError, file.errorString(), path);
    }

    const QJsonDocument document(nest(settings.layerValues(layer)));
    if (file.write(document.toJson(QJsonDocument::Indented)) == -1) {
        return Err(ErrorCode::IoError, file.errorString(), path);
    }
    if (!file.commit()) {
        return Err(ErrorCode::IoError, file.errorString(), path);
    }

    qCDebug(lcConfig) << "saved settings to" << path;
    return Ok();
}

} // namespace keys::config
