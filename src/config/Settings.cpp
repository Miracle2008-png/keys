#include "config/Settings.h"

#include "core/Log.h"

#include <QSet>

using keys::core::ErrorCode;
using keys::core::Err;
using keys::core::Ok;
using keys::core::Status;

namespace keys::config {

Settings::Settings(QObject* parent) : QObject(parent) {}

QVariant Settings::value(const QString& key) const
{
    const SettingDefinition* definition = m_schema.find(key);
    if (!definition) {
        qCWarning(lcConfig) << "read of undeclared setting" << key;
        return {};
    }

    // Highest layer wins; fall through to what is underneath.
    if (const auto it = m_workspace.constFind(key); it != m_workspace.cend()) {
        return it.value();
    }
    if (const auto it = m_user.constFind(key); it != m_user.cend()) {
        return it.value();
    }
    return definition->defaultValue;
}

QString Settings::stringValue(const QString& key) const
{
    return value(key).toString();
}

int Settings::intValue(const QString& key) const
{
    return value(key).toInt();
}

double Settings::doubleValue(const QString& key) const
{
    return value(key).toDouble();
}

bool Settings::boolValue(const QString& key) const
{
    return value(key).toBool();
}

Status Settings::setValue(const QString& key, const QVariant& newValue, Layer layer)
{
    if (layer == Layer::Default) {
        return Err(ErrorCode::NotSupported,
                   QStringLiteral("The default layer is read-only"), key);
    }

    const SettingDefinition* definition = m_schema.find(key);
    if (!definition) {
        return Err(ErrorCode::NotFound, QStringLiteral("Unknown setting"), key);
    }
    if (!m_schema.isValid(key, newValue)) {
        return Err(ErrorCode::InvalidArgument,
                   QStringLiteral("Value is not valid for this setting"), key);
    }

    // An Application-scoped setting must not vary per project — allowing it would
    // make window state and theme flicker as projects are switched.
    if (layer == Layer::Workspace && definition->scope == SettingScope::Application) {
        return Err(ErrorCode::NotSupported,
                   QStringLiteral("This setting is application-scoped and cannot be "
                                  "set per workspace"),
                   key);
    }

    const QVariant before = value(key);
    layerMap(layer).insert(key, newValue);
    const QVariant after = value(key);

    // Writing a lower layer while a higher one overrides it changes nothing the
    // user can observe, so it emits nothing.
    if (before != after) {
        emit changed(key, after);
    }
    return Ok();
}

Status Settings::clearValue(const QString& key, Layer layer)
{
    if (layer == Layer::Default) {
        return Err(ErrorCode::NotSupported,
                   QStringLiteral("The default layer is read-only"), key);
    }
    if (!m_schema.contains(key)) {
        return Err(ErrorCode::NotFound, QStringLiteral("Unknown setting"), key);
    }

    const QVariant before = value(key);
    layerMap(layer).remove(key);
    const QVariant after = value(key);

    if (before != after) {
        emit changed(key, after);
    }
    return Ok();
}

bool Settings::isSetIn(const QString& key, Layer layer) const
{
    return layer != Layer::Default && layerMap(layer).contains(key);
}

void Settings::clearWorkspaceLayer()
{
    replaceLayer(Layer::Workspace, {});
}

void Settings::replaceLayer(Layer layer, const QHash<QString, QVariant>& values)
{
    if (layer == Layer::Default) {
        qCWarning(lcConfig) << "attempt to replace the read-only default layer";
        return;
    }

    QHash<QString, QVariant>& target = layerMap(layer);

    // Snapshot the resolved values of every key either map touches, so removals
    // are reported as well as additions.
    QSet<QString> affected;
    for (auto it = target.cbegin(); it != target.cend(); ++it) {
        affected.insert(it.key());
    }
    for (auto it = values.cbegin(); it != values.cend(); ++it) {
        affected.insert(it.key());
    }

    QHash<QString, QVariant> before;
    before.reserve(affected.size());
    for (const QString& key : affected) {
        before.insert(key, value(key));
    }

    target.clear();
    for (auto it = values.cbegin(); it != values.cend(); ++it) {
        // Validate on the way in: a settings file edited by hand or written by an
        // older version must not be able to poison the store.
        if (m_schema.isValid(it.key(), it.value())) {
            target.insert(it.key(), it.value());
        } else {
            qCWarning(lcConfig) << "ignoring invalid setting" << it.key() << "=" << it.value();
        }
    }

    for (const QString& key : affected) {
        const QVariant after = value(key);
        if (before.value(key) != after) {
            emit changed(key, after);
        }
    }
}

QHash<QString, QVariant> Settings::layerValues(Layer layer) const
{
    if (layer == Layer::Default) {
        QHash<QString, QVariant> defaults;
        for (const SettingDefinition& definition : m_schema.all()) {
            defaults.insert(definition.key, definition.defaultValue);
        }
        return defaults;
    }
    return layerMap(layer);
}

const QHash<QString, QVariant>& Settings::layerMap(Layer layer) const
{
    return layer == Layer::Workspace ? m_workspace : m_user;
}

QHash<QString, QVariant>& Settings::layerMap(Layer layer)
{
    return layer == Layer::Workspace ? m_workspace : m_user;
}

} // namespace keys::config
