#include "config/Settings.h"
#include "update/UpdateChecker.h"

#include <QSignalSpy>
#include <QTest>

using keys::config::Settings;
using keys::update::UpdateChecker;

/// What broke in the shipped build: the update keys were read and written but
/// never declared in the schema, so every write was refused and every read
/// warned. The version comparison was covered by nothing at all.
class UpdateTests : public QObject {
    Q_OBJECT

private slots:
    // ---- The settings the checker depends on ------------------------------

    void enabledDefaultsToOn()
    {
        Settings settings;
        const UpdateChecker checker(settings);
        QVERIFY(checker.isEnabled());
    }

    void optingOutPersists()
    {
        // The regression: setValue refuses undeclared keys, so the switch
        // reported success in the UI and the next launch checked anyway.
        Settings settings;
        UpdateChecker checker(settings);

        checker.setEnabled(false);
        QVERIFY(!checker.isEnabled());

        checker.setEnabled(true);
        QVERIFY(checker.isEnabled());
    }

    void changingTheSettingIsAnnounced()
    {
        Settings settings;
        UpdateChecker checker(settings);

        const QSignalSpy spy(&checker, &UpdateChecker::stateChanged);
        checker.setEnabled(false);
        QCOMPARE(spy.count(), 1);
    }

    void lastCheckSurvivesAWrite()
    {
        // checkIfDue reads this back to decide whether a day has passed. An
        // undeclared key made it permanently empty, so every launch checked.
        Settings settings;
        const QString stamp = QStringLiteral("2026-09-06T22:00:00");

        QVERIFY(settings.setValue(QStringLiteral("updates.lastCheck"), stamp));
        QCOMPARE(settings.stringValue(QStringLiteral("updates.lastCheck")), stamp);
    }

    // ---- Not asking when asked not to -------------------------------------

    void disabledMeansNoRequest()
    {
        // Not "a request whose answer is discarded". With no network in the
        // test environment the observable proof is that nothing starts: a
        // check that ran would flip isChecking.
        Settings settings;
        UpdateChecker checker(settings);
        checker.setEnabled(false);

        checker.checkIfDue();
        QVERIFY(!checker.isChecking());
    }

    void noReleaseSeenMeansNoUpdate()
    {
        Settings settings;
        const UpdateChecker checker(settings);

        QVERIFY(checker.availableVersion().isEmpty());
        QVERIFY(!checker.updateAvailable());
    }

    void reportsItsOwnVersion()
    {
        Settings settings;
        const UpdateChecker checker(settings);

        // Comes from KEYS_VERSION at compile time; an empty one would make
        // every comparison against it meaningless.
        QVERIFY(!checker.currentVersion().isEmpty());
        QVERIFY(checker.currentVersion().contains(QLatin1Char('.')));
    }
};

QTEST_MAIN(UpdateTests)
#include "UpdateTests.moc"
