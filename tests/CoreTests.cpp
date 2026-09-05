#include "core/Cancellation.h"
#include "core/CommandRegistry.h"
#include "core/Result.h"
#include "core/TaskScheduler.h"

#include <QSignalSpy>
#include <QTest>

#include <atomic>

using namespace keys::core;

class CoreTests : public QObject {
    Q_OBJECT

private slots:
    // ---- Result -----------------------------------------------------------

    void resultCarriesValue()
    {
        const Result<int> result = 42;
        QVERIFY(result.hasValue());
        QVERIFY(static_cast<bool>(result));
        QCOMPARE(result.value(), 42);
    }

    void resultCarriesError()
    {
        const Result<int> result =
            Err(ErrorCode::NotFound, QStringLiteral("missing"), QStringLiteral("file.txt"));
        QVERIFY(!result.hasValue());
        QVERIFY(!static_cast<bool>(result));
        QCOMPARE(result.error().code(), ErrorCode::NotFound);
        QCOMPARE(result.error().toString(), QStringLiteral("file.txt: missing"));
    }

    void errorWithoutContextOmitsSeparator()
    {
        const Error error(ErrorCode::IoError, QStringLiteral("disk full"));
        QCOMPARE(error.toString(), QStringLiteral("disk full"));
    }

    void valueOrFallsBackOnError()
    {
        const Result<int> ok = 7;
        const Result<int> failed = Err(ErrorCode::Unknown, QStringLiteral("no"));
        QCOMPARE(ok.valueOr(99), 7);
        QCOMPARE(failed.valueOr(99), 99);
    }

    void voidResultDistinguishesSuccessFromFailure()
    {
        QVERIFY(static_cast<bool>(Ok()));

        const Status failed = Err(ErrorCode::Cancelled, QStringLiteral("stopped"));
        QVERIFY(!static_cast<bool>(failed));
        QVERIFY(failed.error().isCancellation());
    }

    // ---- Cancellation -----------------------------------------------------

    void defaultTokenIsNeverCancelled()
    {
        const CancellationToken token;
        QVERIFY(!token.isCancelled());
    }

    void cancellingSourceCancelsIssuedTokens()
    {
        CancellationSource source;
        const CancellationToken before = source.token();

        QVERIFY(!before.isCancelled());
        source.cancel();

        QVERIFY(before.isCancelled());
        // Tokens taken after cancellation must also observe it, or a worker
        // started late would run work that is already obsolete.
        QVERIFY(source.token().isCancelled());
    }

    // ---- CommandRegistry --------------------------------------------------

    void registersAndInvokesCommand()
    {
        CommandRegistry registry;
        int invocations = 0;

        Command command;
        command.id = QStringLiteral("test.run");
        command.title = QStringLiteral("Run");
        command.category = QStringLiteral("Test");
        command.handler = [&invocations] { ++invocations; };

        QSignalSpy spy(&registry, &CommandRegistry::commandRegistered);
        QVERIFY(static_cast<bool>(registry.registerCommand(std::move(command))));
        QCOMPARE(spy.count(), 1);

        QVERIFY(static_cast<bool>(registry.invoke(QStringLiteral("test.run"))));
        QCOMPARE(invocations, 1);
    }

    void rejectsDuplicateId()
    {
        CommandRegistry registry;

        const auto make = [](const QString& title) {
            Command command;
            command.id = QStringLiteral("test.duplicate");
            command.title = title;
            command.handler = [] {};
            return command;
        };

        QVERIFY(static_cast<bool>(registry.registerCommand(make(QStringLiteral("First")))));

        // Two modules claiming one id must surface, not silently overwrite.
        const Status second = registry.registerCommand(make(QStringLiteral("Second")));
        QVERIFY(!static_cast<bool>(second));
        QCOMPARE(second.error().code(), ErrorCode::AlreadyExists);
        QCOMPARE(registry.find(QStringLiteral("test.duplicate"))->title,
                 QStringLiteral("First"));
    }

    void rejectsCommandWithoutHandler()
    {
        CommandRegistry registry;

        Command command;
        command.id = QStringLiteral("test.empty");
        command.title = QStringLiteral("Empty");

        const Status status = registry.registerCommand(std::move(command));
        QVERIFY(!static_cast<bool>(status));
        QCOMPARE(status.error().code(), ErrorCode::InvalidArgument);
    }

    void invokingUnknownCommandFails()
    {
        CommandRegistry registry;
        const Status status = registry.invoke(QStringLiteral("test.missing"));
        QVERIFY(!static_cast<bool>(status));
        QCOMPARE(status.error().code(), ErrorCode::NotFound);
    }

    void disabledCommandDoesNotRun()
    {
        CommandRegistry registry;
        int invocations = 0;
        bool available = false;

        Command command;
        command.id = QStringLiteral("test.gated");
        command.title = QStringLiteral("Gated");
        command.handler = [&invocations] { ++invocations; };
        command.isEnabled = [&available] { return available; };
        QVERIFY(static_cast<bool>(registry.registerCommand(std::move(command))));

        const Status blocked = registry.invoke(QStringLiteral("test.gated"));
        QVERIFY(!static_cast<bool>(blocked));
        QCOMPARE(blocked.error().code(), ErrorCode::NotSupported);
        QCOMPARE(invocations, 0);

        available = true;
        QVERIFY(static_cast<bool>(registry.invoke(QStringLiteral("test.gated"))));
        QCOMPARE(invocations, 1);
    }

    void unregisterRemovesCommand()
    {
        CommandRegistry registry;

        Command command;
        command.id = QStringLiteral("test.temporary");
        command.title = QStringLiteral("Temporary");
        command.handler = [] {};
        QVERIFY(static_cast<bool>(registry.registerCommand(std::move(command))));
        QCOMPARE(registry.count(), 1);

        QSignalSpy spy(&registry, &CommandRegistry::commandUnregistered);
        QVERIFY(static_cast<bool>(registry.unregisterCommand(QStringLiteral("test.temporary"))));
        QCOMPARE(spy.count(), 1);
        QCOMPARE(registry.count(), 0);
        QVERIFY(!registry.contains(QStringLiteral("test.temporary")));

        QVERIFY(!static_cast<bool>(registry.unregisterCommand(QStringLiteral("test.temporary"))));
    }

    void commandUnregisteringItselfDoesNotCrash()
    {
        // A command that removes its own module while running would destroy the
        // handler mid-call if the registry did not copy it first.
        CommandRegistry registry;

        Command command;
        command.id = QStringLiteral("test.selfDestruct");
        command.title = QStringLiteral("Self Destruct");
        command.handler = [&registry] {
            registry.unregisterCommand(QStringLiteral("test.selfDestruct"));
        };
        QVERIFY(static_cast<bool>(registry.registerCommand(std::move(command))));

        QVERIFY(static_cast<bool>(registry.invoke(QStringLiteral("test.selfDestruct"))));
        QCOMPARE(registry.count(), 0);
    }

    void listingIsOrderedByCategoryThenTitle()
    {
        CommandRegistry registry;

        const auto add = [&registry](const QString& id, const QString& category,
                                     const QString& title) {
            Command command;
            command.id = id;
            command.category = category;
            command.title = title;
            command.handler = [] {};
            registry.registerCommand(std::move(command));
        };

        add(QStringLiteral("c"), QStringLiteral("View"), QStringLiteral("Zoom In"));
        add(QStringLiteral("a"), QStringLiteral("File"), QStringLiteral("Save"));
        add(QStringLiteral("b"), QStringLiteral("File"), QStringLiteral("Open"));

        const QList<Command> all = registry.all();
        QCOMPARE(all.size(), 3);
        QCOMPARE(all.at(0).title, QStringLiteral("Open"));
        QCOMPARE(all.at(1).title, QStringLiteral("Save"));
        QCOMPARE(all.at(2).title, QStringLiteral("Zoom In"));
    }

    // ---- TaskScheduler ----------------------------------------------------

    void schedulerLeavesACoreForTheUiThread()
    {
        const TaskScheduler scheduler;
        QVERIFY(scheduler.maxThreadCount() >= 1);
        QCOMPARE(scheduler.maxThreadCount(),
                 std::max(1, static_cast<int>(std::thread::hardware_concurrency()) - 1));
    }

    void schedulerRunsPostedWork()
    {
        TaskScheduler scheduler;
        std::atomic_int counter{0};

        constexpr int kTaskCount = 64;
        for (int i = 0; i < kTaskCount; ++i) {
            scheduler.post([&counter] { counter.fetch_add(1, std::memory_order_relaxed); });
        }
        scheduler.waitForDone();

        QCOMPARE(counter.load(), kTaskCount);
    }

    void schedulerIgnoresEmptyWork()
    {
        TaskScheduler scheduler;
        scheduler.post({});
        scheduler.waitForDone();
        QCOMPARE(scheduler.activeThreadCount(), 0);
    }
};

QTEST_MAIN(CoreTests)
#include "CoreTests.moc"
