#include "langsvc/JsonRpc.h"
#include "langsvc/LanguageServiceManager.h"
#include "langsvc/LspTypes.h"

#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTest>

using namespace keys::langsvc;

/// The LSP transport and type mapping.
///
/// Framing is where LSP clients go wrong: a read can end anywhere, including
/// halfway through a header. So most of these feed the codec deliberately
/// awkward byte splits and check that whole messages still come out.
class LangSvcTests : public QObject {
    Q_OBJECT

private:
    /// A minimal encoded message, for framing cases.
    [[nodiscard]] static QByteArray encoded(const QString& method)
    {
        return JsonRpcCodec::encode({{QStringLiteral("method"), method}});
    }

private slots:
    // ---- Framing -----------------------------------------------------------

    void encodesAContentLengthHeader()
    {
        const QByteArray wire = JsonRpcCodec::encode(
            {{QStringLiteral("method"), QStringLiteral("initialize")}});

        QVERIFY(wire.startsWith("Content-Length: "));
        QVERIFY(wire.contains("\r\n\r\n"));

        // The length must count the body's bytes, which is what a server reads.
        const int separator = wire.indexOf("\r\n\r\n");
        const int declared =
            wire.mid(16, separator - 16).trimmed().toInt();
        QCOMPARE(wire.size() - separator - 4, declared);
    }

    void countsBytesNotCharactersInTheHeader()
    {
        // A body with non-ASCII has more bytes than characters. Getting this
        // wrong truncates every message carrying source text.
        const QByteArray wire = JsonRpcCodec::encode(
            {{QStringLiteral("text"), QStringLiteral("héllo — wörld")}});

        const int separator = wire.indexOf("\r\n\r\n");
        const int declared = wire.mid(16, separator - 16).trimmed().toInt();
        QCOMPARE(wire.size() - separator - 4, declared);

        JsonRpcCodec codec;
        codec.append(wire);
        QVERIFY(codec.next().has_value());
    }

    void decodesASingleMessage()
    {
        JsonRpcCodec codec;
        codec.append(encoded(QStringLiteral("initialize")));

        const auto message = codec.next();
        QVERIFY(message.has_value());
        QCOMPARE(message->method, QStringLiteral("initialize"));
        QVERIFY(message->isNotification());
    }

    void returnsNothingUntilTheHeaderIsComplete()
    {
        JsonRpcCodec codec;
        codec.append("Content-Len");
        QVERIFY(!codec.next().has_value());
    }

    void returnsNothingUntilTheBodyIsComplete()
    {
        const QByteArray wire = encoded(QStringLiteral("initialize"));

        JsonRpcCodec codec;
        codec.append(wire.left(wire.size() - 5));
        QVERIFY(!codec.next().has_value());

        codec.append(wire.right(5));
        QVERIFY(codec.next().has_value());
    }

    void reassemblesAMessageSplitOneByteAtATime()
    {
        // The worst case a real stream can produce.
        const QByteArray wire = encoded(QStringLiteral("textDocument/didOpen"));

        JsonRpcCodec codec;
        for (int i = 0; i < wire.size() - 1; ++i) {
            codec.append(wire.mid(i, 1));
            QVERIFY(!codec.next().has_value());
        }
        codec.append(wire.right(1));

        const auto message = codec.next();
        QVERIFY(message.has_value());
        QCOMPARE(message->method, QStringLiteral("textDocument/didOpen"));
    }

    void decodesSeveralMessagesFromOneRead()
    {
        // A single read can carry several messages; stopping after the first
        // would leave the rest stuck until more bytes happened to arrive.
        JsonRpcCodec codec;
        codec.append(encoded(QStringLiteral("one")) + encoded(QStringLiteral("two"))
                     + encoded(QStringLiteral("three")));

        QCOMPARE(codec.next()->method, QStringLiteral("one"));
        QCOMPARE(codec.next()->method, QStringLiteral("two"));
        QCOMPARE(codec.next()->method, QStringLiteral("three"));
        QVERIFY(!codec.next().has_value());
    }

    void handlesABodyContainingTheHeaderTerminator()
    {
        // Source text can contain \r\n\r\n. A parser that searched for a
        // delimiter instead of counting bytes would split the message here.
        const QByteArray wire = JsonRpcCodec::encode(
            {{QStringLiteral("method"), QStringLiteral("didChange")},
             {QStringLiteral("text"), QStringLiteral("a\r\n\r\nb")}});

        JsonRpcCodec codec;
        codec.append(wire);

        const auto message = codec.next();
        QVERIFY(message.has_value());
        QCOMPARE(message->method, QStringLiteral("didChange"));
        QVERIFY(!codec.next().has_value());
    }

    void distinguishesRequestsResponsesAndNotifications()
    {
        JsonRpcCodec codec;
        codec.append(JsonRpcCodec::encode(
            {{QStringLiteral("id"), 1}, {QStringLiteral("method"), QStringLiteral("m")}}));
        codec.append(JsonRpcCodec::encode(
            {{QStringLiteral("id"), 1}, {QStringLiteral("result"), QJsonValue::Null}}));
        codec.append(JsonRpcCodec::encode({{QStringLiteral("method"), QStringLiteral("n")}}));

        QVERIFY(codec.next()->isRequest());

        // A null result is still a response. contains(), not a null check, is
        // what makes this work.
        const auto response = codec.next();
        QVERIFY(response->isResponse());
        QVERIFY(response->hasResult);

        QVERIFY(codec.next()->isNotification());
    }

    void aMalformedBodyLosesOneMessageNotTheStream()
    {
        // Framing was valid, so the next message is still correctly positioned.
        const QByteArray bad = "Content-Length: 5\r\n\r\n{not!";

        JsonRpcCodec codec;
        codec.append(bad + encoded(QStringLiteral("good")));

        const auto message = codec.next();
        QVERIFY(message.has_value());
        QCOMPARE(message->method, QStringLiteral("good"));
    }

    // ---- URIs --------------------------------------------------------------

    void roundTripsAPath()
    {
#if defined(Q_OS_WIN)
        const QString path = QStringLiteral("C:/src/main.cpp");
#else
        const QString path = QStringLiteral("/src/main.cpp");
#endif
        const QString uri = pathToUri(path);
        QVERIFY(uri.startsWith(QStringLiteral("file://")));
        QCOMPARE(uriToPath(uri), path);
    }

    void encodesADriveLetterCorrectly()
    {
#if defined(Q_OS_WIN)
        // file:///C:/... - three slashes, and forward separators, or the server
        // sees a different file.
        const QString uri = pathToUri(QStringLiteral("C:\\src\\main.cpp"));
        QVERIFY2(uri.startsWith(QStringLiteral("file:///")), qPrintable(uri));
        QVERIFY(!uri.contains(QLatin1Char('\\')));
#else
        QSKIP("drive letters are a Windows concern");
#endif
    }

    void encodesAPathWithSpaces()
    {
        const QString path =
#if defined(Q_OS_WIN)
            QStringLiteral("C:/Program Files/proj/a.cpp");
#else
            QStringLiteral("/opt/Program Files/a.cpp");
#endif
        QCOMPARE(uriToPath(pathToUri(path)), path);
    }

    // ---- Language ids ------------------------------------------------------

    void mapsKnownExtensions()
    {
        QCOMPARE(languageIdForPath(QStringLiteral("a.cpp")), QStringLiteral("cpp"));
        QCOMPARE(languageIdForPath(QStringLiteral("a.h")), QStringLiteral("cpp"));
        QCOMPARE(languageIdForPath(QStringLiteral("a.rs")), QStringLiteral("rust"));
        QCOMPARE(languageIdForPath(QStringLiteral("a.PY")), QStringLiteral("python"));
    }

    void returnsNothingForAnUnknownExtension()
    {
        // Empty rather than a guess: a document must not be opened against a
        // server that cannot read it.
        QVERIFY(languageIdForPath(QStringLiteral("notes.xyz")).isEmpty());
        QVERIFY(languageIdForPath(QStringLiteral("Makefile")).isEmpty());
    }

    // ---- Type mapping ------------------------------------------------------

    void parsesADiagnostic()
    {
        const QJsonObject json{
            {QStringLiteral("range"),
             QJsonObject{{QStringLiteral("start"),
                          QJsonObject{{QStringLiteral("line"), 4},
                                      {QStringLiteral("character"), 8}}},
                         {QStringLiteral("end"),
                          QJsonObject{{QStringLiteral("line"), 4},
                                      {QStringLiteral("character"), 12}}}}},
            {QStringLiteral("severity"), 2},
            {QStringLiteral("message"), QStringLiteral("unused variable")},
            {QStringLiteral("source"), QStringLiteral("clang")},
        };

        const Diagnostic diagnostic = Diagnostic::fromJson(json);
        QCOMPARE(diagnostic.range.start.line, 4);
        QCOMPARE(diagnostic.range.end.character, 12);
        QCOMPARE(diagnostic.severity, DiagnosticSeverity::Warning);
        QCOMPARE(diagnostic.source, QStringLiteral("clang"));
    }

    void defaultsAMissingSeverityToError()
    {
        // Under-reporting a real error is worse than over-reporting a hint.
        const Diagnostic diagnostic = Diagnostic::fromJson(
            {{QStringLiteral("message"), QStringLiteral("something")}});
        QCOMPARE(diagnostic.severity, DiagnosticSeverity::Error);
    }

    void readsADiagnosticCodeThatIsANumber()
    {
        // Servers send this as a string or a number, and both are legal.
        QCOMPARE(Diagnostic::fromJson({{QStringLiteral("code"), 2065}}).code,
                 QStringLiteral("2065"));
        QCOMPARE(Diagnostic::fromJson({{QStringLiteral("code"), QStringLiteral("E01")}}).code,
                 QStringLiteral("E01"));
    }

    void completionFallsBackToItsLabel()
    {
        // insertText is optional; without it the label is what gets inserted.
        const CompletionItem item = CompletionItem::fromJson(
            {{QStringLiteral("label"), QStringLiteral("push_back")}});
        QCOMPARE(item.textToInsert(), QStringLiteral("push_back"));

        const CompletionItem withInsert = CompletionItem::fromJson(
            {{QStringLiteral("label"), QStringLiteral("push_back(…)")},
             {QStringLiteral("insertText"), QStringLiteral("push_back")}});
        QCOMPARE(withInsert.textToInsert(), QStringLiteral("push_back"));
    }

    void readsMarkupDocumentation()
    {
        // A plain string or a MarkupContent object; servers differ.
        QCOMPARE(CompletionItem::fromJson(
                     {{QStringLiteral("documentation"), QStringLiteral("plain")}})
                     .documentation,
                 QStringLiteral("plain"));

        QCOMPARE(CompletionItem::fromJson(
                     {{QStringLiteral("documentation"),
                       QJsonObject{{QStringLiteral("kind"), QStringLiteral("markdown")},
                                   {QStringLiteral("value"), QStringLiteral("**bold**")}}}})
                     .documentation,
                 QStringLiteral("**bold**"));
    }

    // ---- Server configuration ----------------------------------------------

    void everyDefaultServerIsUsable()
    {
        const std::vector<ServerConfig> servers =
            LanguageServiceManager::defaultServers();
        QVERIFY(!servers.empty());

        for (const ServerConfig& server : servers) {
            QVERIFY2(server.isValid(), qPrintable(server.name));
            QVERIFY(!server.name.isEmpty());
        }
    }

    void noTwoServersClaimTheSameLanguage()
    {
        // Two servers for one language means the manager's choice is arbitrary,
        // and a user would see whichever happened to be listed first.
        QSet<QString> claimed;
        for (const ServerConfig& server : LanguageServiceManager::defaultServers()) {
            for (const QString& languageId : server.languageIds) {
                QVERIFY2(!claimed.contains(languageId), qPrintable(languageId));
                claimed.insert(languageId);
            }
        }
    }

    void aManagerWithNoProjectStartsNothing()
    {
        LanguageServiceManager manager;
        QVERIFY(manager.clientFor(QStringLiteral("a.cpp")) == nullptr);
        QVERIFY(manager.runningServers().isEmpty());
    }

    void anUnknownFileTypeGetsNoServer()
    {
        LanguageServiceManager manager;
        manager.setProjectRoot(QDir::tempPath());
        QVERIFY(manager.clientFor(QStringLiteral("notes.xyz")) == nullptr);
    }
};

QTEST_MAIN(LangSvcTests)
#include "LangSvcTests.moc"
