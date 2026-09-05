#include "debugger/DapTypes.h"
#include "debugger/DebugSession.h"
#include "langsvc/JsonRpc.h"

#include <QJsonArray>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <memory>

using namespace keys::debugger;
using keys::langsvc::JsonRpcCodec;

/// DAP types and session lifecycle.
///
/// The protocol's own message flow is exercised through the codec rather than a
/// real adapter: adapters are separate installs and none is guaranteed present,
/// but the framing and the type mapping are ours and must be right regardless.
class DebuggerTests : public QObject {
    Q_OBJECT

private:
    std::unique_ptr<QTemporaryDir> m_dir;

private slots:
    void init()
    {
        m_dir = std::make_unique<QTemporaryDir>();
        QVERIFY(m_dir->isValid());
    }

    void cleanup() { m_dir.reset(); }

    // ---- Types -------------------------------------------------------------

    void parsesAStackFrame()
    {
        const QJsonObject json{
            {QStringLiteral("id"), 1001},
            {QStringLiteral("name"), QStringLiteral("main")},
            {QStringLiteral("line"), 42},
            {QStringLiteral("column"), 5},
            {QStringLiteral("source"),
             QJsonObject{{QStringLiteral("path"), QStringLiteral("/src/main.cpp")}}},
        };

        const StackFrame frame = StackFrame::fromJson(json);
        QCOMPARE(frame.id, 1001);
        QCOMPARE(frame.name, QStringLiteral("main"));
        QCOMPARE(frame.line, 42);
        QVERIFY(frame.hasSource());
    }

    void aFrameWithoutSourceIsStillAFrame()
    {
        // A stack that walks into a system library has frames the user cannot
        // open, and they belong in the list so the call chain reads correctly.
        const StackFrame frame = StackFrame::fromJson(
            {{QStringLiteral("id"), 2}, {QStringLiteral("name"), QStringLiteral("ntdll!Rtl")}});

        QVERIFY(!frame.hasSource());
        QCOMPARE(frame.name, QStringLiteral("ntdll!Rtl"));
    }

    void parsesAVariable()
    {
        const Variable variable = Variable::fromJson({
            {QStringLiteral("name"), QStringLiteral("count")},
            {QStringLiteral("value"), QStringLiteral("42")},
            {QStringLiteral("type"), QStringLiteral("int")},
        });

        QCOMPARE(variable.name, QStringLiteral("count"));
        QCOMPARE(variable.value, QStringLiteral("42"));
        QVERIFY(!variable.isExpandable());
    }

    void aVariableWithChildrenIsExpandable()
    {
        // A non-zero variablesReference is DAP's way of saying "ask me for the
        // children"; zero means a leaf.
        const Variable variable = Variable::fromJson(
            {{QStringLiteral("name"), QStringLiteral("v")},
             {QStringLiteral("variablesReference"), 7}});
        QVERIFY(variable.isExpandable());
    }

    void breakpointReportsWhereItActuallyLanded()
    {
        // An adapter can move a breakpoint to the next executable line, and
        // showing it where the user clicked would be a lie.
        Breakpoint breakpoint;
        breakpoint.line = 10;
        QCOMPARE(breakpoint.effectiveLine(), 10);

        breakpoint.actualLine = 12;
        QCOMPARE(breakpoint.effectiveLine(), 12);
    }

    void everyStateHasALabel()
    {
        for (const DebugState state :
             {DebugState::Inactive, DebugState::Starting, DebugState::Running,
              DebugState::Stopped, DebugState::Terminating}) {
            QVERIFY(!debugStateLabel(state).isEmpty());
        }
    }

    // ---- Configuration -----------------------------------------------------

    void anIncompleteConfigurationIsRejected()
    {
        DebugSession session;

        DebugConfig config;   // no name, no adapter
        QVERIFY(!config.isValid());
        QVERIFY(!session.start(config, m_dir->path()));
        QVERIFY(!session.isActive());
    }

    void aMissingAdapterIsReportedNotCrashed()
    {
        // Adapters are separate installs, so this is the common failure.
        DebugSession session;

        DebugConfig config;
        config.name = QStringLiteral("test");
        config.adapterProgram = QStringLiteral("keys-no-such-adapter-exists");

        QSignalSpy failedSpy(&session, &DebugSession::failed);
        QVERIFY(static_cast<bool>(session.start(config, m_dir->path())));

        QTest::qWait(500);
        QCOMPARE(failedSpy.count(), 1);
        // The message names the adapter, or it is not actionable.
        QVERIFY(failedSpy.at(0).at(0).toString().contains(
            QStringLiteral("keys-no-such-adapter-exists")));
        QVERIFY(!session.isActive());
    }

    // ---- Control gating ----------------------------------------------------

    void controlsAreRefusedWhileInactive()
    {
        // Every control checks the state rather than deciding for itself, so a
        // button can never send a request the adapter would reject. These must
        // simply do nothing.
        DebugSession session;
        QCOMPARE(session.state(), DebugState::Inactive);

        session.resume();
        session.pause();
        session.stepOver();
        session.stepInto();
        session.stepOut();
        session.setBreakpoints(QStringLiteral("/a.cpp"), {1, 2});

        QCOMPARE(session.state(), DebugState::Inactive);
        QVERIFY(session.stack().empty());
    }

    void stoppingWhenNothingRunsIsHarmless()
    {
        DebugSession session;
        session.stop();
        QVERIFY(!session.isActive());
    }

    // ---- Wire format -------------------------------------------------------

    void dapSharesLspFramingButNotItsEnvelope()
    {
        // The point of reusing the codec: same Content-Length header, different
        // message shape. A decoder that assumed JSON-RPC's fields would read
        // every DAP message as malformed.
        const QJsonObject request{
            {QStringLiteral("seq"), 1},
            {QStringLiteral("type"), QStringLiteral("request")},
            {QStringLiteral("command"), QStringLiteral("initialize")},
        };

        JsonRpcCodec codec;
        codec.append(JsonRpcCodec::encode(request));

        const auto message = codec.next();
        QVERIFY(message.has_value());

        // JSON-RPC's fields are absent...
        QVERIFY(message->method.isEmpty());
        QVERIFY(!message->isResponse());

        // ...and DAP's are readable from the raw object, which is why the codec
        // keeps it.
        QCOMPARE(message->raw.value(QStringLiteral("type")).toString(),
                 QStringLiteral("request"));
        QCOMPARE(message->raw.value(QStringLiteral("command")).toString(),
                 QStringLiteral("initialize"));
        QCOMPARE(message->raw.value(QStringLiteral("seq")).toInt(), 1);
    }

    void decodesADapEventAndResponse()
    {
        JsonRpcCodec codec;

        codec.append(JsonRpcCodec::encode(
            {{QStringLiteral("type"), QStringLiteral("event")},
             {QStringLiteral("event"), QStringLiteral("stopped")},
             {QStringLiteral("body"),
              QJsonObject{{QStringLiteral("reason"), QStringLiteral("breakpoint")},
                          {QStringLiteral("threadId"), 1}}}}));

        codec.append(JsonRpcCodec::encode(
            {{QStringLiteral("type"), QStringLiteral("response")},
             {QStringLiteral("request_seq"), 3},
             {QStringLiteral("success"), true},
             {QStringLiteral("command"), QStringLiteral("stackTrace")}}));

        const auto event = codec.next();
        QVERIFY(event.has_value());
        QCOMPARE(event->raw.value(QStringLiteral("event")).toString(),
                 QStringLiteral("stopped"));
        QCOMPARE(event->raw.value(QStringLiteral("body")).toObject()
                     .value(QStringLiteral("reason")).toString(),
                 QStringLiteral("breakpoint"));

        const auto response = codec.next();
        QVERIFY(response.has_value());
        QCOMPARE(response->raw.value(QStringLiteral("request_seq")).toInt(), 3);
        QVERIFY(response->raw.value(QStringLiteral("success")).toBool());
    }
};

QTEST_MAIN(DebuggerTests)
#include "DebuggerTests.moc"
