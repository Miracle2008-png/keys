#pragma once

#include "config/Settings.h"

#include <QAbstractListModel>
#include <QQmlEngine>
#include <QString>

#include <vector>

namespace keys::ui {

/// The settings page's contents, generated from the schema.
///
/// **Generated, not hand-written.** The schema already declares every setting's
/// type, default, allowed values and bounds. A hand-built settings page would
/// duplicate all of that in QML and then drift from it: a setting added to the
/// schema would silently fail to appear, and one removed would leave a control
/// wired to nothing. Deriving the page means the two cannot disagree.
///
/// **The control follows the type.** An enumerated setting gets a segmented
/// choice, a bounded number gets a slider, a bool gets a switch, everything else
/// gets a text field. The model decides which from the declaration, so QML
/// contains presentation rather than a table of special cases.
class SettingsModel : public QAbstractListModel {
    Q_OBJECT
    QML_NAMED_ELEMENT(SettingsList)
    QML_SINGLETON

    Q_PROPERTY(int count READ count NOTIFY countChanged)

    /// A property rather than only a function: QML re-evaluates a binding when
    /// the notify signal fires, but would never re-run a bare function call, so
    /// the "Reset all" button would stay as it was when the page opened.
    Q_PROPERTY(bool hasModifiedValues READ hasModifiedValues NOTIFY modifiedChanged)

public:
    /// Which control a row draws. Derived from the declaration rather than
    /// stored, so a setting cannot be declared with a control that does not fit
    /// its type.
    enum class Control {
        Toggle,   ///< bool
        Choice,   ///< allowedValues
        Slider,   ///< bounded number
        Text,     ///< anything else
    };
    Q_ENUM(Control)

    enum Roles {
        KeyRole = Qt::UserRole + 1,
        TitleRole,
        DescriptionRole,
        GroupRole,
        IsGroupStartRole, ///< first row of a group, so QML can draw its heading
        ControlRole,
        ValueRole,
        ChoicesRole,
        MinimumRole,
        MaximumRole,
        IsIntegerRole,    ///< a slider over ints must not offer 4.37 as a tab size
        IsModifiedRole,   ///< the user has overridden the default
    };

    explicit SettingsModel(config::Settings& settings, QObject* parent = nullptr);

    static void setInstance(SettingsModel* instance);
    static SettingsModel* create(QQmlEngine* engine, QJSEngine* scriptEngine);

    [[nodiscard]] int rowCount(const QModelIndex& parent = {}) const override;
    [[nodiscard]] QVariant data(const QModelIndex& index, int role) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

    [[nodiscard]] int count() const { return static_cast<int>(m_rows.size()); }

    /// Writes a value. Returns false if the schema rejects it, which is what
    /// keeps a text field from storing nonsense.
    Q_INVOKABLE bool setValue(const QString& key, const QVariant& value);

    /// Clears the user's override, revealing the default.
    Q_INVOKABLE void resetValue(const QString& key);

    /// True when any setting has been overridden, so the page can offer to
    /// restore everything without the button being pointlessly present.
    [[nodiscard]] bool hasModifiedValues() const;

    Q_INVOKABLE void resetAll();

signals:
    void countChanged();
    void modifiedChanged();

private:
    struct Row {
        QString key;
        QString title;
        QString description;
        QString group;
        Control control = Control::Text;
        QStringList choices;
        double minimum = 0.0;
        double maximum = 0.0;
        bool isInteger = false;
    };

    /// Builds the rows from the schema. Called once: the schema is fixed after
    /// startup, so rebuilding per change would be wasted work.
    void build();

    /// Emits dataChanged for whichever row holds `key`.
    void notifyChanged(const QString& key);

    config::Settings& m_settings;
    std::vector<Row> m_rows;
};

} // namespace keys::ui
