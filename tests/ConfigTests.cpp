#include "config/AnimationPolicy.h"
#include "config/Settings.h"
#include "config/SettingsStore.h"

#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

using namespace keys::config;
using keys::core::ErrorCode;
using keys::core::Status;

class ConfigTests : public QObject {
    Q_OBJECT

private slots:
    // ---- Schema -----------------------------------------------------------

    void schemaDeclaresBuiltins()
    {
        const SettingsSchema schema;
        QVERIFY(schema.contains(QStringLiteral("appearance.theme")));
        QVERIFY(schema.contains(QStringLiteral("animation.level")));
        QVERIFY(schema.contains(QStringLiteral("editor.fontSize")));
        QVERIFY(!schema.contains(QStringLiteral("nonsense.key")));
    }

    void schemaRejectsValuesOutsideAllowedSet()
    {
        const SettingsSchema schema;
        QVERIFY(schema.isValid(QStringLiteral("appearance.theme"), QStringLiteral("light")));
        QVERIFY(!schema.isValid(QStringLiteral("appearance.theme"), QStringLiteral("purple")));
    }

    void schemaRejectsUnknownKeys()
    {
        const SettingsSchema schema;
        QVERIFY(!schema.isValid(QStringLiteral("nonsense.key"), 1));
    }

    // ---- Layered resolution -----------------------------------------------

    void resolvesToSchemaDefaultWhenUnset()
    {
        const Settings settings;
        QCOMPARE(settings.stringValue(QStringLiteral("appearance.theme")),
                 QStringLiteral("dark"));
        QCOMPARE(settings.intValue(QStringLiteral("editor.fontSize")), 13);
    }

    void workspaceOverridesUserOverridesDefault()
    {
        Settings settings;
        const QString key = QStringLiteral("editor.fontSize");

        QCOMPARE(settings.intValue(key), 13);

        QVERIFY(static_cast<bool>(settings.setValue(key, 15, Settings::Layer::User)));
        QCOMPARE(settings.intValue(key), 15);

        QVERIFY(static_cast<bool>(settings.setValue(key, 17, Settings::Layer::Workspace)));
        QCOMPARE(settings.intValue(key), 17);

        // Clearing the top layer must reveal the one beneath, not fall to zero.
        QVERIFY(static_cast<bool>(settings.clearValue(key, Settings::Layer::Workspace)));
        QCOMPARE(settings.intValue(key), 15);

        QVERIFY(static_cast<bool>(settings.clearValue(key, Settings::Layer::User)));
        QCOMPARE(settings.intValue(key), 13);
    }

    void writingUnknownKeyFails()
    {
        Settings settings;
        const Status status = settings.setValue(QStringLiteral("nonsense.key"), 1);
        QVERIFY(!static_cast<bool>(status));
        QCOMPARE(status.error().code(), ErrorCode::NotFound);
    }

    void writingInvalidValueFails()
    {
        Settings settings;
        const Status status =
            settings.setValue(QStringLiteral("appearance.theme"), QStringLiteral("purple"));
        QVERIFY(!static_cast<bool>(status));
        QCOMPARE(status.error().code(), ErrorCode::InvalidArgument);
    }

    void defaultLayerIsReadOnly()
    {
        Settings settings;
        const Status status =
            settings.setValue(QStringLiteral("editor.fontSize"), 20, Settings::Layer::Default);
        QVERIFY(!static_cast<bool>(status));
        QCOMPARE(status.error().code(), ErrorCode::NotSupported);
    }

    void applicationScopedSettingRejectsWorkspaceLayer()
    {
        // Theme is application-scoped: letting a project override it would make
        // the whole interface change colour on project switch.
        Settings settings;
        const Status status = settings.setValue(QStringLiteral("appearance.theme"),
                                                QStringLiteral("light"),
                                                Settings::Layer::Workspace);
        QVERIFY(!static_cast<bool>(status));
        QCOMPARE(status.error().code(), ErrorCode::NotSupported);
    }

    // ---- Change notification ----------------------------------------------

    void emitsChangeWhenResolvedValueChanges()
    {
        Settings settings;
        QSignalSpy spy(&settings, &Settings::changed);

        QVERIFY(static_cast<bool>(
            settings.setValue(QStringLiteral("editor.fontSize"), 15)));
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("editor.fontSize"));
        QCOMPARE(spy.at(0).at(1).toInt(), 15);
    }

    void doesNotEmitWhenResolvedValueIsUnchanged()
    {
        Settings settings;
        const QString key = QStringLiteral("editor.fontSize");

        QVERIFY(static_cast<bool>(settings.setValue(key, 17, Settings::Layer::Workspace)));

        QSignalSpy spy(&settings, &Settings::changed);

        // The workspace layer still wins, so nothing the user can observe changed.
        QVERIFY(static_cast<bool>(settings.setValue(key, 15, Settings::Layer::User)));
        QCOMPARE(spy.count(), 0);
    }

    void clearingWorkspaceLayerReportsRevealedValues()
    {
        Settings settings;
        const QString key = QStringLiteral("editor.fontSize");
        QVERIFY(static_cast<bool>(settings.setValue(key, 21, Settings::Layer::Workspace)));

        QSignalSpy spy(&settings, &Settings::changed);
        settings.clearWorkspaceLayer();

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toString(), key);
        QCOMPARE(spy.at(0).at(1).toInt(), 13);
    }

    void replaceLayerDiscardsInvalidEntries()
    {
        Settings settings;
        settings.replaceLayer(Settings::Layer::User,
                              {{QStringLiteral("editor.fontSize"), 15},
                               {QStringLiteral("appearance.theme"), QStringLiteral("purple")},
                               {QStringLiteral("nonsense.key"), 1}});

        QCOMPARE(settings.intValue(QStringLiteral("editor.fontSize")), 15);
        // The invalid and unknown entries must not reach the store.
        QCOMPARE(settings.stringValue(QStringLiteral("appearance.theme")),
                 QStringLiteral("dark"));
        QVERIFY(!settings.isSetIn(QStringLiteral("nonsense.key"), Settings::Layer::User));
    }

    // ---- Persistence ------------------------------------------------------

    void missingSettingsFileIsNotAnError()
    {
        // First run: no file yet. That is an ordinary state, not a failure.
        const QTemporaryDir dir;
        QVERIFY(dir.isValid());

        Settings settings;
        const Status status = SettingsStore::load(settings, Settings::Layer::User,
                                                  dir.filePath(QStringLiteral("absent.json")));
        QVERIFY(static_cast<bool>(status));
        QCOMPARE(settings.intValue(QStringLiteral("editor.fontSize")), 13);
    }

    void savedSettingsRoundTrip()
    {
        const QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("nested/settings.json"));

        Settings written;
        QVERIFY(static_cast<bool>(
            written.setValue(QStringLiteral("editor.fontSize"), 16)));
        QVERIFY(static_cast<bool>(
            written.setValue(QStringLiteral("appearance.theme"), QStringLiteral("light"))));
        QVERIFY(static_cast<bool>(
            written.setValue(QStringLiteral("editor.insertSpaces"), false)));

        QVERIFY(static_cast<bool>(
            SettingsStore::save(written, Settings::Layer::User, path)));

        Settings read;
        QVERIFY(static_cast<bool>(
            SettingsStore::load(read, Settings::Layer::User, path)));

        QCOMPARE(read.intValue(QStringLiteral("editor.fontSize")), 16);
        QCOMPARE(read.stringValue(QStringLiteral("appearance.theme")),
                 QStringLiteral("light"));
        // False rather than true: the default is true, so writing it would
        // round-trip even if the value never reached the file.
        QCOMPARE(read.boolValue(QStringLiteral("editor.insertSpaces")), false);
    }

    void malformedSettingsFileReportsParseError()
    {
        // A hand-edited file with a typo must be reported, not silently discarded.
        const QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("broken.json"));

        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("{ this is not json");
        file.close();

        Settings settings;
        const Status status = SettingsStore::load(settings, Settings::Layer::User, path);
        QVERIFY(!static_cast<bool>(status));
        QCOMPARE(status.error().code(), ErrorCode::ParseError);
    }

    // ---- Animation policy -------------------------------------------------

    void animationLevelMapsToDesignDurations()
    {
        Settings settings;
        AnimationPolicy policy(settings);

        QCOMPARE(policy.level(), AnimationPolicy::Level::Full);
        QCOMPARE(policy.duration(), 150);
        QVERIFY(policy.enabled());

        QVERIFY(static_cast<bool>(
            settings.setValue(QStringLiteral("animation.level"), QStringLiteral("reduced"))));
        QCOMPARE(policy.duration(), 60);
        QVERIFY(policy.enabled());

        // Off must be genuinely zero: a 1ms animation still lands mid-frame,
        // which is what a user who disabled motion is asking not to have.
        QVERIFY(static_cast<bool>(
            settings.setValue(QStringLiteral("animation.level"), QStringLiteral("off"))));
        QCOMPARE(policy.duration(), 0);
        QCOMPARE(policy.fastDuration(), 0);
        QVERIFY(!policy.enabled());
    }

    void animationPolicyEmitsOnLevelChange()
    {
        Settings settings;
        AnimationPolicy policy(settings);
        QSignalSpy spy(&policy, &AnimationPolicy::changed);

        QVERIFY(static_cast<bool>(
            settings.setValue(QStringLiteral("animation.level"), QStringLiteral("off"))));
        QCOMPARE(spy.count(), 1);

        // Re-setting the same level changes nothing and must stay quiet.
        QVERIFY(static_cast<bool>(
            settings.setValue(QStringLiteral("animation.level"), QStringLiteral("off"))));
        QCOMPARE(spy.count(), 1);
    }

    // ---- Persistence of user changes --------------------------------------

    void themeToggleSurvivesSaveAndReload()
    {
        // A preference the user changes must come back on the next launch. This
        // is the full round trip the application performs: change, save on quit,
        // load on start.
        const QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("settings.json"));

        {
            Settings session;
            QVERIFY(static_cast<bool>(SettingsStore::load(
                session, Settings::Layer::User, path)));
            QCOMPARE(session.stringValue(QStringLiteral("appearance.theme")),
                     QStringLiteral("dark"));

            QVERIFY(static_cast<bool>(session.setValue(
                QStringLiteral("appearance.theme"), QStringLiteral("light"))));
            QVERIFY(static_cast<bool>(SettingsStore::save(
                session, Settings::Layer::User, path)));
        }

        Settings relaunched;
        QVERIFY(static_cast<bool>(SettingsStore::load(
            relaunched, Settings::Layer::User, path)));
        QCOMPARE(relaunched.stringValue(QStringLiteral("appearance.theme")),
                 QStringLiteral("light"));
        QVERIFY(relaunched.isSetIn(QStringLiteral("appearance.theme"),
                                   Settings::Layer::User));
    }

    void workbenchStateSurvivesSaveAndReload()
    {
        const QTemporaryDir dir;
        QVERIFY(dir.isValid());
        const QString path = dir.filePath(QStringLiteral("settings.json"));

        {
            Settings session;
            QVERIFY(static_cast<bool>(session.setValue(
                QStringLiteral("workbench.sidebarVisible"), false)));
            QVERIFY(static_cast<bool>(session.setValue(
                QStringLiteral("workbench.activeView"), QStringLiteral("search"))));
            QVERIFY(static_cast<bool>(session.setValue(
                QStringLiteral("workbench.sidebarWidth"), 320)));
            QVERIFY(static_cast<bool>(SettingsStore::save(
                session, Settings::Layer::User, path)));
        }

        Settings relaunched;
        QVERIFY(static_cast<bool>(SettingsStore::load(
            relaunched, Settings::Layer::User, path)));
        QCOMPARE(relaunched.boolValue(QStringLiteral("workbench.sidebarVisible")), false);
        QCOMPARE(relaunched.stringValue(QStringLiteral("workbench.activeView")),
                 QStringLiteral("search"));
        QCOMPARE(relaunched.intValue(QStringLiteral("workbench.sidebarWidth")), 320);
    }

    void unknownAnimationLevelFallsBackToFull()
    {
        QCOMPARE(AnimationPolicy::levelFromString(QStringLiteral("nonsense")),
                 AnimationPolicy::Level::Full);
        QCOMPARE(AnimationPolicy::levelToString(AnimationPolicy::Level::Reduced),
                 QStringLiteral("reduced"));
    }
};

QTEST_MAIN(ConfigTests)
#include "ConfigTests.moc"
