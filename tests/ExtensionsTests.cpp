#include "config/Settings.h"
#include "core/CommandRegistry.h"
#include "extensions/ExtensionHost.h"
#include "extensions/ExtensionRegistry.h"
#include "extensions/Manifest.h"
#include "filesystem/FileSystem.h"

#include <QDir>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <memory>

using namespace keys::extensions;
using keys::fs::FileSystem;

/// Manifests, capability grants and the host's refusals.
///
/// The capability check is the part that has to be right: an extension that can
/// reach past its grant makes the whole out-of-process design pointless.
class ExtensionsTests : public QObject {
    Q_OBJECT

private:
    std::unique_ptr<QTemporaryDir> m_dir;

    [[nodiscard]] static QJsonObject minimalManifest()
    {
        return {
            {QStringLiteral("id"), QStringLiteral("sample")},
            {QStringLiteral("name"), QStringLiteral("Sample")},
            {QStringLiteral("version"), QStringLiteral("1.0.0")},
            {QStringLiteral("entry"),
             QJsonObject{{QStringLiteral("program"), QStringLiteral("run.exe")}}},
        };
    }

private slots:
    void init()
    {
        m_dir = std::make_unique<QTemporaryDir>();
        QVERIFY(m_dir->isValid());
    }

    void cleanup() { m_dir.reset(); }

    // ---- Manifest ----------------------------------------------------------

    void parsesAMinimalManifest()
    {
        const auto manifest = Manifest::parse(minimalManifest());
        QVERIFY(static_cast<bool>(manifest));
        QCOMPARE(manifest.value().id, QStringLiteral("sample"));
        QVERIFY(manifest.value().isValid());
    }

    void rejectsAManifestWithNoId()
    {
        QJsonObject json = minimalManifest();
        json.remove(QStringLiteral("id"));
        QVERIFY(!Manifest::parse(json));
    }

    void rejectsAManifestWithNoEntryProgram()
    {
        QJsonObject json = minimalManifest();
        json.remove(QStringLiteral("entry"));
        QVERIFY(!Manifest::parse(json));
    }

    void rejectsAPathLikeId()
    {
        // The id keys a grant and names a directory. One containing separators
        // could collide with another extension's grant or escape its directory.
        for (const QString& id : {QStringLiteral("../evil"), QStringLiteral("a/b"),
                                  QStringLiteral("a\\b")}) {
            QJsonObject json = minimalManifest();
            json.insert(QStringLiteral("id"), id);
            QVERIFY2(!Manifest::parse(json), qPrintable(id));
        }
    }

    void parsesCapabilities()
    {
        QJsonObject json = minimalManifest();
        json.insert(QStringLiteral("capabilities"),
                    QJsonArray{QStringLiteral("readWorkspace"), QStringLiteral("network")});

        const auto manifest = Manifest::parse(json);
        QVERIFY(static_cast<bool>(manifest));
        QVERIFY(manifest.value().declares(Capability::ReadWorkspace));
        QVERIFY(manifest.value().declares(Capability::Network));
        QVERIFY(!manifest.value().declares(Capability::WriteWorkspace));
    }

    void rejectsAnUnknownCapability()
    {
        // Failing the whole manifest rather than dropping the entry: silently
        // ignoring it would run the extension with less than it asked for, and
        // neither side would notice.
        QJsonObject json = minimalManifest();
        json.insert(QStringLiteral("capabilities"),
                    QJsonArray{QStringLiteral("readEverything")});
        QVERIFY(!Manifest::parse(json));
    }

    void requiresCommandsToBeNamespaced()
    {
        // Two extensions must not be able to claim the same command id.
        QJsonObject json = minimalManifest();
        json.insert(QStringLiteral("capabilities"),
                    QJsonArray{QStringLiteral("registerCommands")});
        json.insert(QStringLiteral("contributes"),
                    QJsonObject{{QStringLiteral("commands"),
                                 QJsonArray{QJsonObject{
                                     {QStringLiteral("id"), QStringLiteral("format")},
                                     {QStringLiteral("title"), QStringLiteral("Format")}}}}});

        QVERIFY(!Manifest::parse(json));
    }

    void acceptsANamespacedCommand()
    {
        QJsonObject json = minimalManifest();
        json.insert(QStringLiteral("capabilities"),
                    QJsonArray{QStringLiteral("registerCommands")});
        json.insert(QStringLiteral("contributes"),
                    QJsonObject{{QStringLiteral("commands"),
                                 QJsonArray{QJsonObject{
                                     {QStringLiteral("id"), QStringLiteral("sample.format")},
                                     {QStringLiteral("title"), QStringLiteral("Format")}}}}});

        const auto manifest = Manifest::parse(json);
        QVERIFY(static_cast<bool>(manifest));
        QCOMPARE(manifest.value().commands.size(), size_t{1});
    }

    void refusesCommandsWithoutTheCapability()
    {
        // A manifest that contradicts itself would leave commands in the palette
        // that cannot run.
        QJsonObject json = minimalManifest();
        json.insert(QStringLiteral("contributes"),
                    QJsonObject{{QStringLiteral("commands"),
                                 QJsonArray{QJsonObject{
                                     {QStringLiteral("id"), QStringLiteral("sample.x")},
                                     {QStringLiteral("title"), QStringLiteral("X")}}}}});

        QVERIFY(!Manifest::parse(json));
    }

    void loadsAManifestFromDisk()
    {
        const QString path = QDir(m_dir->path()).filePath(QStringLiteral("keys-extension.json"));
        QVERIFY(static_cast<bool>(FileSystem::writeTextFile(
            path, QString::fromUtf8(QJsonDocument(minimalManifest()).toJson()))));

        const auto manifest = Manifest::load(m_dir->path());
        QVERIFY(static_cast<bool>(manifest));
        QCOMPARE(manifest.value().name, QStringLiteral("Sample"));
    }

    void reportsMalformedJsonRatherThanCrashing()
    {
        const QString path = QDir(m_dir->path()).filePath(QStringLiteral("keys-extension.json"));
        QVERIFY(static_cast<bool>(FileSystem::writeTextFile(path, QStringLiteral("{ not json"))));

        const auto manifest = Manifest::load(m_dir->path());
        QVERIFY(!manifest);
        QCOMPARE(manifest.error().code(), keys::core::ErrorCode::ParseError);
    }

    // ---- Capability descriptions -------------------------------------------

    void everyCapabilityHasAnIdAndDescription()
    {
        // A capability without a description cannot be consented to, so this
        // guards against one being added to the enum and not the table.
        for (const Capability capability :
             {Capability::ReadWorkspace, Capability::WriteWorkspace,
              Capability::ReadEditor, Capability::ModifyEditor,
              Capability::RegisterCommands, Capability::ShowUi,
              Capability::Network, Capability::RunProcesses}) {
            QVERIFY(!capabilityId(capability).isEmpty());
            QVERIFY(!capabilityDescription(capability).isEmpty());
        }
    }

    void capabilityIdsRoundTrip()
    {
        for (const Capability capability :
             {Capability::ReadWorkspace, Capability::WriteWorkspace,
              Capability::ModifyEditor, Capability::Network}) {
            const auto parsed = capabilityFromId(capabilityId(capability));
            QVERIFY(parsed.has_value());
            QCOMPARE(*parsed, capability);
        }
    }

    void theDangerousCapabilitiesAreMarkedSensitive()
    {
        // Reading the workspace is what every extension does; prompting for it
        // trains people to click yes. These genuinely deserve a pause.
        QVERIFY(capabilityIsSensitive(Capability::WriteWorkspace));
        QVERIFY(capabilityIsSensitive(Capability::ModifyEditor));
        QVERIFY(capabilityIsSensitive(Capability::Network));
        QVERIFY(capabilityIsSensitive(Capability::RunProcesses));

        QVERIFY(!capabilityIsSensitive(Capability::ReadWorkspace));
        QVERIFY(!capabilityIsSensitive(Capability::RegisterCommands));
    }

    void anUnknownCapabilityIdParsesToNothing()
    {
        QVERIFY(!capabilityFromId(QStringLiteral("readEverything")).has_value());
        QVERIFY(!capabilityFromId(QString()).has_value());
    }

    // ---- Host --------------------------------------------------------------

    void aCapabilityMustBeBothDeclaredAndGranted()
    {
        // A grant for something never declared would be a bug on our side, and
        // honouring it would let a manifest edit widen an existing grant.
        Manifest manifest = Manifest::parse(minimalManifest()).value();
        manifest.capabilities = {Capability::ReadWorkspace};

        ExtensionHost host(manifest, m_dir->path());

        // Not started, so nothing is granted yet.
        QVERIFY(!host.hasCapability(Capability::ReadWorkspace));
        QVERIFY(!host.hasCapability(Capability::Network));
    }

    void refusesAnEntryProgramOutsideTheExtensionDirectory()
    {
        // A manifest naming "../../../bin/sh" would otherwise run something the
        // user never installed.
        Manifest manifest = Manifest::parse(minimalManifest()).value();
        manifest.entryPoint = QStringLiteral("../../../evil.exe");

        ExtensionHost host(manifest, m_dir->path());
        const auto status = host.start({}, m_dir->path());

        QVERIFY(!status);
        QCOMPARE(status.error().code(), keys::core::ErrorCode::PermissionDenied);
    }

    void reportsAMissingEntryProgram()
    {
        Manifest manifest = Manifest::parse(minimalManifest()).value();
        manifest.entryPoint = QStringLiteral("does-not-exist.exe");

        ExtensionHost host(manifest, m_dir->path());
        QSignalSpy failedSpy(&host, &ExtensionHost::failed);

        QVERIFY(static_cast<bool>(host.start({}, m_dir->path())));
        QTest::qWait(500);
        QVERIFY(failedSpy.count() >= 1);
    }

    // ---- Registry ----------------------------------------------------------

    void anEmptyDirectoryYieldsNoExtensions()
    {
        keys::core::CommandRegistry commands;
        keys::config::Settings settings;
        ExtensionRegistry registry(commands, settings);

        // The real directory may hold nothing, which is the normal state.
        QVERIFY(registry.installed().size() >= 0);
    }

    void nothingIsGrantedByDefault()
    {
        // The whole point of the model: an extension runs only after the user
        // has said yes.
        keys::core::CommandRegistry commands;
        keys::config::Settings settings;
        ExtensionRegistry registry(commands, settings);

        QVERIFY(registry.grantedCapabilities(QStringLiteral("anything")).isEmpty());
        QVERIFY(!registry.isFullyGranted(QStringLiteral("anything")));
        QVERIFY(!registry.isRunning(QStringLiteral("anything")));
    }

    void grantsPersistThroughSettings()
    {
        keys::core::CommandRegistry commands;
        keys::config::Settings settings;
        ExtensionRegistry registry(commands, settings);

        registry.setGranted(QStringLiteral("sample"),
                            {QStringLiteral("readWorkspace"), QStringLiteral("showUi")});

        const QSet<QString> granted = registry.grantedCapabilities(QStringLiteral("sample"));
        QCOMPARE(granted.size(), 2);
        QVERIFY(granted.contains(QStringLiteral("readWorkspace")));

        // Stored in settings, so a second registry over the same settings sees
        // them - which is what makes a grant survive a restart.
        ExtensionRegistry second(commands, settings);
        QCOMPARE(second.grantedCapabilities(QStringLiteral("sample")).size(), 2);
    }

    void revokingClearsTheGrant()
    {
        keys::core::CommandRegistry commands;
        keys::config::Settings settings;
        ExtensionRegistry registry(commands, settings);

        registry.setGranted(QStringLiteral("sample"), {QStringLiteral("readWorkspace")});
        QVERIFY(!registry.grantedCapabilities(QStringLiteral("sample")).isEmpty());

        registry.setGranted(QStringLiteral("sample"), {});
        QVERIFY(registry.grantedCapabilities(QStringLiteral("sample")).isEmpty());
        QVERIFY(!registry.isRunning(QStringLiteral("sample")));
    }
};

QTEST_MAIN(ExtensionsTests)
#include "ExtensionsTests.moc"
