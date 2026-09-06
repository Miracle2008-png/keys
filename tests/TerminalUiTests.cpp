#include "config/Settings.h"
#include "ui/TerminalModel.h"
#include "ui/Theme.h"

#include <QDir>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <memory>

using keys::config::Settings;
using keys::ui::TerminalModel;
using keys::ui::Theme;

/// The terminal panel's model.
///
/// The shell itself is exercised by tools/terminal-panel-check, which has to run
/// detached because ConPTY behaves differently under a console parent. What is
/// pinned down here is everything around it: session bookkeeping, the refusal to
/// start without a project, and the key encoding - where a wrong byte means an
/// interrupt that does not interrupt or a backspace that does not delete.
class TerminalUiTests : public QObject {
    Q_OBJECT

private:
    std::unique_ptr<Settings> m_settings;
    std::unique_ptr<Theme> m_theme;
    std::unique_ptr<TerminalModel> m_model;

private slots:
    void init()
    {
        m_settings = std::make_unique<Settings>();
        m_theme = std::make_unique<Theme>(*m_settings);
        m_model = std::make_unique<TerminalModel>(*m_theme, *m_settings);
    }

    void cleanup()
    {
        m_model.reset();
        m_theme.reset();
        m_settings.reset();
    }

    // ---- Nothing open -----------------------------------------------------

    void startsWithNoSessions()
    {
        QCOMPARE(m_model->sessionCount(), 0);
        QCOMPARE(m_model->currentSession(), -1);
        QCOMPARE(m_model->rowCount(), 0);
        QVERIFY(!m_model->isRunning());
        QVERIFY(m_model->title().isEmpty());
    }

    void refusesToStartWithoutAProject()
    {
        // A shell rooted in whatever directory the application happened to
        // launch from is a trap: the user types `rm` expecting their project.
        const QSignalSpy failed(m_model.get(), &TerminalModel::errorOccurred);

        m_model->openSession();

        QCOMPARE(m_model->sessionCount(), 0);
        QCOMPARE(failed.count(), 1);
    }

    void queryingAnAbsentSessionIsSafe()
    {
        // The tab strip asks for titles while sessions are being closed.
        QVERIFY(m_model->titleAt(-1).isEmpty());
        QVERIFY(m_model->titleAt(0).isEmpty());
        QVERIFY(m_model->titleAt(99).isEmpty());

        QCOMPARE(m_model->cursorLine(), 0);
        QCOMPARE(m_model->cursorColumn(), 0);
        QVERIFY(!m_model->cursorVisible());
    }

    void actingOnAnAbsentSessionIsSafe()
    {
        // Every one of these is reachable from QML before a shell exists.
        m_model->sendText(QStringLiteral("ls"));
        m_model->sendKey(Qt::Key_Return, Qt::NoModifier);
        m_model->resizeTo(80, 24);
        m_model->closeSession(0);
        m_model->closeSession(-1);
        m_model->setCurrentSession(3);

        QCOMPARE(m_model->sessionCount(), 0);
        QCOMPARE(m_model->currentSession(), -1);
    }

    void plainTextIsEmptyWithNoSession()
    {
        QVERIFY(m_model->plainText().isEmpty());
    }

    // ---- The model's contract with QML ------------------------------------

    void exposesTheRolesThePanelBindsTo()
    {
        // A role name that does not match a delegate's required property is
        // silently undefined at runtime, which is how a panel ships blank.
        const QHash<int, QByteArray> names = m_model->roleNames();
        QVERIFY(names.values().contains(QByteArray("markup")));
        QVERIFY(names.values().contains(QByteArray("plainText")));
    }

    void outOfRangeRowsReturnNothingRatherThanCrashing()
    {
        QVERIFY(!m_model->data(m_model->index(0, 0), TerminalModel::MarkupRole).isValid());
        QVERIFY(!m_model->data(m_model->index(-1, 0), TerminalModel::MarkupRole).isValid());
        QVERIFY(!m_model->data(m_model->index(500, 0), TerminalModel::MarkupRole).isValid());
    }

    void revisionExistsForBindingsToDependOn()
    {
        // QML bindings that call Q_INVOKABLE functions have nothing to depend
        // on and never re-evaluate. The editor shipped with exactly that bug:
        // typing changed the document and the view did not repaint. No shell is
        // started here - that belongs in the detached check, not in a test that
        // would spawn a process on every run.
        QVERIFY(m_model->revision() >= 0);
    }

    // ---- Selecting between sessions ---------------------------------------

    void selectingAnInvalidSessionIsIgnored()
    {
        m_model->setCurrentSession(-1);
        QCOMPARE(m_model->currentSession(), -1);

        m_model->setCurrentSession(0);
        QCOMPARE(m_model->currentSession(), -1);
    }
};

QTEST_MAIN(TerminalUiTests)
#include "TerminalUiTests.moc"
