#include "ui/SettingsModel.h"

#include <QMetaType>

#include <algorithm>

namespace keys::ui {
namespace {

/// The instance main() publishes for QML; see Theme.cpp for the reasoning.
SettingsModel* g_instance = nullptr;

/// Which control fits a declaration. Order matters: a bool with allowed values
/// is still a toggle, and bounds only mean a slider on something numeric.
SettingsModel::Control controlFor(const config::SettingDefinition& definition)
{
    const QMetaType::Type type =
        static_cast<QMetaType::Type>(definition.defaultValue.typeId());

    if (type == QMetaType::Bool) {
        return SettingsModel::Control::Toggle;
    }
    if (!definition.allowedValues.isEmpty()) {
        return SettingsModel::Control::Choice;
    }
    if (definition.isBounded()
        && (type == QMetaType::Int || type == QMetaType::Double)) {
        return SettingsModel::Control::Slider;
    }
    return SettingsModel::Control::Text;
}

} // namespace

void SettingsModel::setInstance(SettingsModel* instance)
{
    g_instance = instance;
}

SettingsModel* SettingsModel::create(QQmlEngine* engine, QJSEngine* scriptEngine)
{
    Q_UNUSED(engine)
    Q_UNUSED(scriptEngine)

    Q_ASSERT_X(g_instance, "SettingsModel::create",
               "SettingsModel::setInstance was not called");

    QQmlEngine::setObjectOwnership(g_instance, QQmlEngine::CppOwnership);
    return g_instance;
}

SettingsModel::SettingsModel(config::Settings& settings, QObject* parent)
    : QAbstractListModel(parent), m_settings(settings)
{
    build();

    // A setting can change from somewhere other than this page - a command, a
    // workspace file being loaded - and the page must show the current value
    // rather than what it was when it opened.
    connect(&m_settings, &config::Settings::changed, this,
            [this](const QString& key, const QVariant&) {
                notifyChanged(key);
                emit modifiedChanged();
            });
}

void SettingsModel::build()
{
    beginResetModel();
    m_rows.clear();

    for (const config::SettingDefinition& definition : m_settings.schema().all()) {
        if (!definition.userVisible) {
            continue;
        }

        Row row;
        row.key = definition.key;
        row.description = definition.description;
        row.choices = definition.allowedValues;
        row.minimum = definition.minimum;
        row.maximum = definition.maximum;
        row.control = controlFor(definition);
        row.isInteger = definition.defaultValue.typeId() == QMetaType::Int;

        // The declaration supplies these, but a setting added without them must
        // still be presentable rather than showing a blank label.
        row.title = definition.title.isEmpty() ? definition.key : definition.title;
        row.group = definition.group.isEmpty()
                        ? definition.key.section(QLatin1Char('.'), 0, 0)
                        : definition.group;

        m_rows.push_back(std::move(row));
    }

    // Grouped, keeping each group in schema order. stable_sort rather than sort
    // so the declaration order inside a group is preserved: it is deliberate
    // (font size before font family), not arbitrary.
    std::stable_sort(m_rows.begin(), m_rows.end(),
                     [](const Row& a, const Row& b) { return a.group < b.group; });

    endResetModel();
    emit countChanged();
}

int SettingsModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : static_cast<int>(m_rows.size());
}

QVariant SettingsModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= rowCount()) {
        return {};
    }

    const Row& row = m_rows.at(static_cast<size_t>(index.row()));

    switch (role) {
    case KeyRole:
        return row.key;
    case TitleRole:
        return row.title;
    case DescriptionRole:
        return row.description;
    case GroupRole:
        return row.group;
    case IsGroupStartRole:
        return index.row() == 0
               || m_rows.at(static_cast<size_t>(index.row()) - 1).group != row.group;
    case ControlRole:
        return QVariant::fromValue(row.control);
    case ValueRole:
        return m_settings.value(row.key);
    case ChoicesRole:
        return row.choices;
    case MinimumRole:
        return row.minimum;
    case MaximumRole:
        return row.maximum;
    case IsIntegerRole:
        return row.isInteger;
    case IsModifiedRole:
        return m_settings.isSetIn(row.key, config::Settings::Layer::User)
               || m_settings.isSetIn(row.key, config::Settings::Layer::Workspace);
    default:
        return {};
    }
}

QHash<int, QByteArray> SettingsModel::roleNames() const
{
    return {
        {KeyRole, "key"},
        {TitleRole, "title"},
        {DescriptionRole, "description"},
        {GroupRole, "group"},
        {IsGroupStartRole, "isGroupStart"},
        {ControlRole, "control"},
        {ValueRole, "value"},
        {ChoicesRole, "choices"},
        {MinimumRole, "minimum"},
        {MaximumRole, "maximum"},
        {IsIntegerRole, "isInteger"},
        {IsModifiedRole, "isModified"},
    };
}

bool SettingsModel::setValue(const QString& key, const QVariant& value)
{
    // Written to the user layer: the settings page edits the user's own
    // preferences. A workspace override is a project decision, made by editing
    // the project's settings file rather than by a control that would silently
    // change meaning depending on what is open.
    return static_cast<bool>(
        m_settings.setValue(key, value, config::Settings::Layer::User));
}

void SettingsModel::resetValue(const QString& key)
{
    m_settings.clearValue(key, config::Settings::Layer::User);
}

bool SettingsModel::hasModifiedValues() const
{
    return std::any_of(m_rows.cbegin(), m_rows.cend(), [this](const Row& row) {
        return m_settings.isSetIn(row.key, config::Settings::Layer::User);
    });
}

void SettingsModel::resetAll()
{
    for (const Row& row : m_rows) {
        m_settings.clearValue(row.key, config::Settings::Layer::User);
    }
}

void SettingsModel::notifyChanged(const QString& key)
{
    const auto it = std::find_if(m_rows.cbegin(), m_rows.cend(),
                                 [&key](const Row& row) { return row.key == key; });
    if (it == m_rows.cend()) {
        return;
    }

    const int row = static_cast<int>(std::distance(m_rows.cbegin(), it));
    emit dataChanged(index(row, 0), index(row, 0), {ValueRole, IsModifiedRole});
}

} // namespace keys::ui
