#include "config/Settings.h"
#include "core/CommandRegistry.h"
#include "extensions/ExtensionRegistry.h"

#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTest>
#include <QTextStream>

#include <memory>

using keys::config::Settings;
using keys::core::CommandRegistry;
using keys::extensions::ExtensionRegistry;

/// Installing and removing extensions.
///
/// Before this, the only way to install one was to copy a folder into a
/// directory by hand - which is not a feature, it is the absence of one.
///
/// What matters most here is what happens when installation fails. A manifest
/// that will not parse, or an entry point that is not there, must leave nothing
/// behind: a half-copied directory appears in the panel as a broken extension
/// the user never asked for, and they have no way to tell wreckage from a bad
/// extension.
class ExtensionInstallTests : public QObject {
    Q_OBJECT

private:
    std::unique_ptr<QTemporaryDir> m_source;
    std::unique_ptr<CommandRegistry> m_commands;
    std::unique_ptr<Settings> m_settings;
    std::unique_ptr<ExtensionRegistry> m_registry;

    static void write(const QString& path, const QString& contents)
    {
        QDir().mkpath(QFileInfo(path).path());
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly | QIODevice::Text));
        QTextStream stream(&file);
        stream << contents;
    }

    /// A complete, valid extension folder.
    [[nodiscard]] QString makeExtension(const QString& id, const QString& name,
                                        const QString& version = QStringLiteral("1.0.0"),
                                        bool withEntryPoint = true) const
    {
        const QString directory = QDir(m_source->path()).filePath(id);
        QDir().mkpath(directory);

        write(QDir(directory).filePath(QStringLiteral("keys-extension.json")),
              QStringLiteral(R"({
    "id": "%1",
    "name": "%2",
    "version": "%3",
    "description": "A test extension",
    "author": "Tests",
    "entry": { "program": "run.js" },
    "capabilities": ["readWorkspace", "network"]
})").arg(id, name, version));

        if (withEntryPoint) {
            write(QDir(directory).filePath(QStringLiteral("run.js")),
                  QStringLiteral("// entry point\n"));
        }
        return directory;
    }

    [[nodiscard]] bool isInstalled(const QString& id) const
    {
        for (const auto& extension : m_registry->installed()) {
            if (extension.manifest.id == id) {
                return true;
            }
        }
        return false;
    }

private slots:
    void initTestCase()
    {
        // Test mode moves AppLocalDataLocation to a scratch path, so nothing
        // here touches a real installation. It has to be set before any
        // location is resolved: QStandardPaths caches, so doing it per-test
        // would have no effect after the first.
        QStandardPaths::setTestModeEnabled(true);
    }

    void init()
    {
        // Each test starts with an empty extensions directory. Without this the
        // tests see each other's installs, and the ones that assert "nothing
        // was installed" fail depending on the order they run in.
        QDir(ExtensionRegistry::extensionsDirectory()).removeRecursively();

        m_source = std::make_unique<QTemporaryDir>();
        QVERIFY(m_source->isValid());

        m_commands = std::make_unique<CommandRegistry>();
        m_settings = std::make_unique<Settings>();
        m_registry = std::make_unique<ExtensionRegistry>(*m_commands, *m_settings);
    }

    void cleanup()
    {
        m_registry.reset();
        m_settings.reset();
        m_commands.reset();
        m_source.reset();

        QDir(ExtensionRegistry::extensionsDirectory()).removeRecursively();
    }

    // ---- Installing --------------------------------------------------------

    void installsAValidExtension()
    {
        const QString source = makeExtension(QStringLiteral("test.alpha"),
                                             QStringLiteral("Alpha"));

        const auto id = m_registry->installFromDirectory(source);
        QVERIFY2(id.hasValue(), qPrintable(id ? QString() : id.error().message()));
        QCOMPARE(id.value(), QStringLiteral("test.alpha"));
        QVERIFY(isInstalled(QStringLiteral("test.alpha")));
    }

    void theInstalledCopyIsIndependentOfTheSource()
    {
        // Installing copies. If it linked or referenced, deleting the folder
        // the user installed from would break the extension later, with nothing
        // to connect the two events.
        const QString source = makeExtension(QStringLiteral("test.alpha"),
                                             QStringLiteral("Alpha"));
        QVERIFY(m_registry->installFromDirectory(source).hasValue());

        QVERIFY(QDir(source).removeRecursively());
        m_registry->discover();

        QVERIFY(isInstalled(QStringLiteral("test.alpha")));
    }

    void everyFileIsCopiedNotJustTheManifest()
    {
        const QString source = makeExtension(QStringLiteral("test.alpha"),
                                             QStringLiteral("Alpha"));
        write(QDir(source).filePath(QStringLiteral("lib/helper.js")),
              QStringLiteral("// nested\n"));

        QVERIFY(m_registry->installFromDirectory(source).hasValue());

        for (const auto& extension : m_registry->installed()) {
            if (extension.manifest.id == QStringLiteral("test.alpha")) {
                QVERIFY(QFileInfo::exists(
                    QDir(extension.directory).filePath(QStringLiteral("run.js"))));
                QVERIFY(QFileInfo::exists(
                    QDir(extension.directory).filePath(QStringLiteral("lib/helper.js"))));
                return;
            }
        }
        QFAIL("the extension was not installed");
    }

    // ---- Refusing ----------------------------------------------------------

    void refusesAFolderWithNoManifest()
    {
        const QString source = QDir(m_source->path()).filePath(QStringLiteral("plain"));
        QDir().mkpath(source);
        write(QDir(source).filePath(QStringLiteral("readme.txt")),
              QStringLiteral("not an extension"));

        QVERIFY(!m_registry->installFromDirectory(source));
        QCOMPARE(static_cast<int>(m_registry->installed().size()), 0);
    }

    void refusesAManifestThatNamesAMissingEntryPoint()
    {
        // The check that matters: a manifest can be perfectly well-formed and
        // still describe an extension that cannot run.
        const QString source = makeExtension(QStringLiteral("test.broken"),
                                             QStringLiteral("Broken"),
                                             QStringLiteral("1.0.0"),
                                             /*withEntryPoint=*/false);

        const auto id = m_registry->installFromDirectory(source);
        QVERIFY(!id);
        QVERIFY(id.error().message().contains(QStringLiteral("entry point")));
        QCOMPARE(static_cast<int>(m_registry->installed().size()), 0);
    }

    void aRefusedInstallLeavesNothingBehind()
    {
        // The point of validating before copying. A half-installed directory
        // shows up as a broken extension nobody asked for.
        const QString source = makeExtension(QStringLiteral("test.broken"),
                                             QStringLiteral("Broken"),
                                             QStringLiteral("1.0.0"), false);
        QVERIFY(!m_registry->installFromDirectory(source));

        const QDir installRoot(ExtensionRegistry::extensionsDirectory());
        QVERIFY(!installRoot.exists()
                || installRoot.entryList(QDir::Dirs | QDir::NoDotAndDotDot).isEmpty());
    }

    void refusesAPathThatIsNotAFolder()
    {
        const QString file = QDir(m_source->path()).filePath(QStringLiteral("a.txt"));
        write(file, QStringLiteral("x"));

        QVERIFY(!m_registry->installFromDirectory(file));
        QVERIFY(!m_registry->installFromDirectory(
            QDir(m_source->path()).filePath(QStringLiteral("nowhere"))));
    }

    // ---- Upgrading ---------------------------------------------------------

    void installingTheSameIdReplacesIt()
    {
        QVERIFY(m_registry->installFromDirectory(
            makeExtension(QStringLiteral("test.alpha"), QStringLiteral("Alpha"),
                          QStringLiteral("1.0.0"))).hasValue());

        // A second source folder, same id, later version.
        const QString newer = QDir(m_source->path()).filePath(QStringLiteral("v2"));
        QDir().mkpath(newer);
        write(QDir(newer).filePath(QStringLiteral("keys-extension.json")),
              QStringLiteral(R"({
    "id": "test.alpha",
    "name": "Alpha",
    "version": "2.0.0",
    "entry": { "program": "run.js" }
})"));
        write(QDir(newer).filePath(QStringLiteral("run.js")), QStringLiteral("//\n"));

        QVERIFY(m_registry->installFromDirectory(newer).hasValue());

        // One entry, at the new version - not two, and not the old one.
        int count = 0;
        QString version;
        for (const auto& extension : m_registry->installed()) {
            if (extension.manifest.id == QStringLiteral("test.alpha")) {
                ++count;
                version = extension.manifest.version;
            }
        }
        QCOMPARE(count, 1);
        QCOMPARE(version, QStringLiteral("2.0.0"));
    }

    void anUpgradeDoesNotCarryForwardAGrant()
    {
        // A new version can ask for capabilities the old one did not. Keeping a
        // previous yes would grant them with nobody being asked.
        const QString source = makeExtension(QStringLiteral("test.alpha"),
                                             QStringLiteral("Alpha"));
        QVERIFY(m_registry->installFromDirectory(source).hasValue());

        m_registry->setGranted(QStringLiteral("test.alpha"),
                               {QStringLiteral("readWorkspace"),
                                QStringLiteral("network")});
        QVERIFY(!m_registry->grantedCapabilities(QStringLiteral("test.alpha")).isEmpty());

        QVERIFY(m_registry->installFromDirectory(source).hasValue());
        QVERIFY(m_registry->grantedCapabilities(QStringLiteral("test.alpha")).isEmpty());
    }

    // ---- Uninstalling ------------------------------------------------------

    void uninstallRemovesTheFilesAndTheEntry()
    {
        const QString source = makeExtension(QStringLiteral("test.alpha"),
                                             QStringLiteral("Alpha"));
        QVERIFY(m_registry->installFromDirectory(source).hasValue());

        QString directory;
        for (const auto& extension : m_registry->installed()) {
            if (extension.manifest.id == QStringLiteral("test.alpha")) {
                directory = extension.directory;
            }
        }
        QVERIFY(!directory.isEmpty());

        QVERIFY(m_registry->uninstall(QStringLiteral("test.alpha")));
        QVERIFY(!isInstalled(QStringLiteral("test.alpha")));
        QVERIFY(!QFileInfo::exists(directory));
    }

    void uninstallForgetsWhatWasGranted()
    {
        const QString source = makeExtension(QStringLiteral("test.alpha"),
                                             QStringLiteral("Alpha"));
        QVERIFY(m_registry->installFromDirectory(source).hasValue());

        m_registry->setGranted(QStringLiteral("test.alpha"),
                               {QStringLiteral("network")});
        QVERIFY(m_registry->uninstall(QStringLiteral("test.alpha")));

        // Reinstalling must ask again rather than inheriting a yes given to a
        // version that is gone.
        QVERIFY(m_registry->grantedCapabilities(QStringLiteral("test.alpha")).isEmpty());
    }

    void uninstallingSomethingNotInstalledFails()
    {
        QVERIFY(!m_registry->uninstall(QStringLiteral("test.nothing")));
    }
};

QTEST_MAIN(ExtensionInstallTests)
#include "ExtensionInstallTests.moc"
