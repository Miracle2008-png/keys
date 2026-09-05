#include "config/Settings.h"
#include "ui/EditorSettings.h"
#include "ui/SettingsModel.h"

#include <QSignalSpy>
#include <QTest>

#include <memory>
#include <type_traits>

using keys::config::Settings;
using keys::ui::EditorSettings;
using keys::ui::SettingsModel;

/// The settings page's model and the editor's resolved settings.
///
/// These are what make the schema's declarations reach the interface, so the
/// tests are about that mapping: the right control for a type, values that
/// round-trip, and settings the editor actually honours.
class SettingsUiTests : public QObject {
    Q_OBJECT

private:
    std::unique_ptr<Settings> m_settings;
    std::unique_ptr<SettingsModel> m_model;

    [[nodiscard]] QVariant roleAt(int row, const char* name) const
    {
        const QHash<int, QByteArray> names = m_model->roleNames();
        for (auto it = names.constBegin(); it != names.constEnd(); ++it) {
            if (it.value() == name) {
                return m_model->data(m_model->index(row, 0), it.key());
            }
        }
        return {};
    }

    /// The row showing `key`, or -1.
    [[nodiscard]] int rowOf(const QString& key) const
    {
        for (int row = 0; row < m_model->count(); ++row) {
            if (roleAt(row, "key").toString() == key) {
                return row;
            }
        }
        return -1;
    }

private slots:
    void init()
    {
        m_settings = std::make_unique<Settings>();
        m_model = std::make_unique<SettingsModel>(*m_settings);
    }

    void cleanup()
    {
        m_model.reset();
        m_settings.reset();
    }

    // ---- Generated from the schema ----------------------------------------

    void everyUserVisibleSettingHasARow()
    {
        // The page is generated, so a setting added to the schema must appear
        // without a matching edit in QML. Counting proves nothing is dropped.
        int expected = 0;
        for (const auto& definition : m_settings->schema().all()) {
            if (definition.userVisible) {
                ++expected;
            }
        }
        QCOMPARE(m_model->count(), expected);
        QVERIFY(expected > 0);
    }

    void windowStateIsNotShown()
    {
        // Sidebar width is persisted but is not a preference: the user sets it
        // by dragging, and a duplicate control in settings would be a second
        // source of truth.
        QCOMPARE(rowOf(QStringLiteral("workbench.sidebarWidth")), -1);
        QCOMPARE(rowOf(QStringLiteral("workbench.activeView")), -1);
    }

    void controlFollowsTheDeclaredType()
    {
        // The whole point of generating the page: the declaration decides the
        // control, so a setting cannot be given one that does not fit its type.
        QCOMPARE(roleAt(rowOf(QStringLiteral("editor.insertSpaces")), "control").toInt(),
                 static_cast<int>(SettingsModel::Control::Toggle));
        QCOMPARE(roleAt(rowOf(QStringLiteral("appearance.theme")), "control").toInt(),
                 static_cast<int>(SettingsModel::Control::Choice));
        QCOMPARE(roleAt(rowOf(QStringLiteral("editor.fontSize")), "control").toInt(),
                 static_cast<int>(SettingsModel::Control::Slider));
        QCOMPARE(roleAt(rowOf(QStringLiteral("editor.fontFamily")), "control").toInt(),
                 static_cast<int>(SettingsModel::Control::Text));
    }

    void integerSettingsAreMarkedAsSuch()
    {
        // A slider over an int must not offer 4.37 as a tab size: the schema
        // would reject it and the control would appear to do nothing.
        QVERIFY(roleAt(rowOf(QStringLiteral("editor.tabSize")), "isInteger").toBool());
        QVERIFY(!roleAt(rowOf(QStringLiteral("editor.fontSize")), "isInteger").toBool());
    }

    void choicesAndBoundsComeFromTheSchema()
    {
        const int theme = rowOf(QStringLiteral("appearance.theme"));
        QCOMPARE(roleAt(theme, "choices").toStringList(),
                 (QStringList{QStringLiteral("dark"), QStringLiteral("light")}));

        const int fontSize = rowOf(QStringLiteral("editor.fontSize"));
        QCOMPARE(roleAt(fontSize, "minimum").toDouble(), 8.0);
        QCOMPARE(roleAt(fontSize, "maximum").toDouble(), 32.0);
    }

    void rowsAreGroupedWithOneHeadingEach()
    {
        int starts = 0;
        for (int row = 0; row < m_model->count(); ++row) {
            if (roleAt(row, "isGroupStart").toBool()) {
                ++starts;
                if (row > 0) {
                    QVERIFY(roleAt(row, "group") != roleAt(row - 1, "group"));
                }
            }
        }

        // Every group is contiguous, so the count of headings equals the count
        // of distinct groups.
        QSet<QString> groups;
        for (int row = 0; row < m_model->count(); ++row) {
            groups.insert(roleAt(row, "group").toString());
        }
        QCOMPARE(starts, groups.size());
    }

    // ---- Editing -----------------------------------------------------------

    void settingAValueWritesThroughToSettings()
    {
        QVERIFY(m_model->setValue(QStringLiteral("editor.tabSize"), 8));
        QCOMPARE(m_settings->intValue(QStringLiteral("editor.tabSize")), 8);
        QCOMPARE(roleAt(rowOf(QStringLiteral("editor.tabSize")), "value").toInt(), 8);
    }

    void outOfRangeValuesAreRejected()
    {
        // Bounds are enforced by the schema, not merely by the slider - a
        // hand-edited settings file must not be able to produce an unusable
        // editor.
        QVERIFY(!m_model->setValue(QStringLiteral("editor.fontSize"), 800.0));
        QCOMPARE(m_settings->doubleValue(QStringLiteral("editor.fontSize")), 13.0);
    }

    void valuesOutsideTheAllowedSetAreRejected()
    {
        QVERIFY(!m_model->setValue(QStringLiteral("appearance.theme"),
                                   QStringLiteral("solarized")));
        QCOMPARE(m_settings->stringValue(QStringLiteral("appearance.theme")),
                 QStringLiteral("dark"));
    }

    void modifiedRowsAreMarkedAndCanBeReset()
    {
        const int row = rowOf(QStringLiteral("editor.tabSize"));
        QVERIFY(!roleAt(row, "isModified").toBool());
        QVERIFY(!m_model->hasModifiedValues());

        QVERIFY(m_model->setValue(QStringLiteral("editor.tabSize"), 2));
        QVERIFY(roleAt(row, "isModified").toBool());
        QVERIFY(m_model->hasModifiedValues());

        m_model->resetValue(QStringLiteral("editor.tabSize"));
        QVERIFY(!roleAt(row, "isModified").toBool());
        // Reset reveals the default rather than zeroing the value.
        QCOMPARE(m_settings->intValue(QStringLiteral("editor.tabSize")), 4);
    }

    void resetAllClearsEveryOverride()
    {
        QVERIFY(m_model->setValue(QStringLiteral("editor.tabSize"), 2));
        QVERIFY(m_model->setValue(QStringLiteral("appearance.theme"),
                                  QStringLiteral("light")));

        m_model->resetAll();
        QVERIFY(!m_model->hasModifiedValues());
        QCOMPARE(m_settings->stringValue(QStringLiteral("appearance.theme")),
                 QStringLiteral("dark"));
    }

    void aChangeFromElsewhereUpdatesTheRow()
    {
        // The page must show the current value, not what it was when it opened:
        // a command or a workspace file can change a setting behind it.
        const int row = rowOf(QStringLiteral("editor.tabSize"));
        QSignalSpy spy(m_model.get(), &SettingsModel::dataChanged);

        QVERIFY(static_cast<bool>(
            m_settings->setValue(QStringLiteral("editor.tabSize"), 8)));

        QCOMPARE(spy.count(), 1);
        QCOMPARE(roleAt(row, "value").toInt(), 8);
    }

    // ---- QML registration --------------------------------------------------

    void singletonsAreNotEngineConstructible()
    {
        // See ThemeTests::qmlResolvesToThePublishedInstance for what this
        // prevents: a default-constructible QML_SINGLETON is built by the engine
        // instead of through create(), and QML silently binds to an unwired
        // second instance.
        static_assert(!std::is_default_constructible_v<SettingsModel>);
        static_assert(!std::is_default_constructible_v<EditorSettings>);

        SettingsModel::setInstance(m_model.get());
        QCOMPARE(SettingsModel::create(nullptr, nullptr), m_model.get());
        SettingsModel::setInstance(nullptr);
    }

    // ---- Editor settings ---------------------------------------------------

    void editorSettingsFollowSettings()
    {
        EditorSettings editor(*m_settings);
        QSignalSpy spy(&editor, &EditorSettings::changed);

        QVERIFY(static_cast<bool>(
            m_settings->setValue(QStringLiteral("editor.lineHeight"), 2.0)));

        QCOMPARE(spy.count(), 1);
        QCOMPARE(editor.lineHeightFactor(), 2.0);
    }

    void indentStringHonoursBothSettings()
    {
        EditorSettings editor(*m_settings);

        // The default: four spaces.
        QCOMPARE(editor.indentString(), QStringLiteral("    "));

        QVERIFY(static_cast<bool>(
            m_settings->setValue(QStringLiteral("editor.tabSize"), 2)));
        QCOMPARE(editor.indentString(), QStringLiteral("  "));

        // Tab size must not leak into a tab character: with insertSpaces off,
        // one tab is inserted regardless of the width it represents.
        QVERIFY(static_cast<bool>(
            m_settings->setValue(QStringLiteral("editor.insertSpaces"), false)));
        QCOMPARE(editor.indentString(), QStringLiteral("\t"));
    }

    void fontFamilyFallsBackToThePlatformFace()
    {
        EditorSettings editor(*m_settings);

        // Empty means "whatever the platform uses", which the view cannot
        // resolve for itself - so it must never be handed an empty family.
        QVERIFY(!editor.fontFamily().isEmpty());

        QVERIFY(static_cast<bool>(m_settings->setValue(
            QStringLiteral("editor.fontFamily"), QStringLiteral("Iosevka"))));
        QCOMPARE(editor.fontFamily(), QStringLiteral("Iosevka"));
    }

    void fontSizeIsConvertedToPoints()
    {
        EditorSettings editor(*m_settings);

        // The setting is in the design's CSS pixels; QML assigns it to
        // font.pointSize, so it renders at the intended size on a high-DPI
        // display rather than being pinned to device pixels.
        QVERIFY(static_cast<bool>(
            m_settings->setValue(QStringLiteral("editor.fontSize"), 16.0)));
        QCOMPARE(editor.fontSize(), 12.0);
    }
};

QTEST_MAIN(SettingsUiTests)
#include "SettingsUiTests.moc"
