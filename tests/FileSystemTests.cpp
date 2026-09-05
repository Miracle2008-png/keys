#include "core/Cancellation.h"
#include "filesystem/FileSystem.h"
#include "filesystem/FileWatcher.h"

#include <QDir>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTest>

#include <memory>

using namespace keys::fs;
using keys::core::CancellationSource;
using keys::core::ErrorCode;
using keys::core::Result;
using keys::core::Status;

class FileSystemTests : public QObject {
    Q_OBJECT

private:
    /// A fresh directory per test. Sharing one across tests lets files created by
    /// an earlier case leak into a later one's listing assertions, which makes
    /// failures depend on execution order rather than on the code under test.
    std::unique_ptr<QTemporaryDir> m_dir;

    [[nodiscard]] QString path(const QString& relative) const
    {
        return QDir(m_dir->path()).filePath(relative);
    }

private slots:
    void init()
    {
        m_dir = std::make_unique<QTemporaryDir>();
        QVERIFY(m_dir->isValid());
    }

    void cleanup()
    {
        m_dir.reset();
    }

    // ---- Path handling ----------------------------------------------------

    void normalizeUsesForwardSlashes()
    {
        // Keys uses forward slashes everywhere, including on Windows, so paths
        // compare and hash consistently regardless of where they came from.
        QCOMPARE(FileSystem::normalize(QStringLiteral("C:\\a\\b")),
                 QStringLiteral("C:/a/b"));
    }

    void normalizeResolvesRelativeElements()
    {
        QCOMPARE(FileSystem::normalize(QStringLiteral("/a/b/../c")),
                 QStringLiteral("/a/c"));
        QCOMPARE(FileSystem::normalize(QStringLiteral("/a/./b")),
                 QStringLiteral("/a/b"));
    }

    void normalizeMakesRelativePathsAbsolute()
    {
        // `keys .` must open the working directory under its real name. Leaving
        // "." unresolved would make the project's name literally "." and make the
        // same folder compare unequal when reached by its full path.
        const QString here = FileSystem::normalize(QStringLiteral("."));
        QVERIFY(QDir::isAbsolutePath(here));
        QCOMPARE(here, QDir::cleanPath(QDir::currentPath()));

        const QString child = FileSystem::normalize(QStringLiteral("sub/dir"));
        QVERIFY(QDir::isAbsolutePath(child));
        QVERIFY(child.endsWith(QStringLiteral("/sub/dir")));
    }

    void normalizeLeavesAbsolutePathsUnchanged()
    {
        // An already-absolute path must not be re-resolved against the working
        // directory, or opening a project would depend on where Keys was started.
        QCOMPARE(FileSystem::normalize(QStringLiteral("/already/absolute")),
                 QStringLiteral("/already/absolute"));
    }

    void normalizeLeavesEmptyPathAlone()
    {
        QVERIFY(FileSystem::normalize(QString()).isEmpty());
    }

    // ---- Read and write ---------------------------------------------------

    void writeThenReadRoundTrips()
    {
        const QString file = path(QStringLiteral("round-trip.txt"));
        const QString content = QStringLiteral("line one\nline two\n");

        QVERIFY(static_cast<bool>(FileSystem::writeTextFile(file, content)));

        const Result<QString> read = FileSystem::readTextFile(file);
        QVERIFY(static_cast<bool>(read));
        QCOMPARE(read.value(), content);
    }

    void writeCreatesMissingDirectories()
    {
        const QString file = path(QStringLiteral("deeply/nested/new.txt"));
        QVERIFY(static_cast<bool>(
            FileSystem::writeTextFile(file, QStringLiteral("content"))));
        QVERIFY(FileSystem::exists(file));
    }

    void writePreservesUtf8()
    {
        // Source files contain far more than ASCII; a lossy write would corrupt
        // the user's file silently.
        const QString file = path(QStringLiteral("utf8.txt"));
        const QString content = QStringLiteral("caf\u00e9 \u2014 \u65e5\u672c\u8a9e \U0001F600");

        QVERIFY(static_cast<bool>(FileSystem::writeTextFile(file, content)));
        const Result<QString> read = FileSystem::readTextFile(file);
        QVERIFY(static_cast<bool>(read));
        QCOMPARE(read.value(), content);
    }

    void readingMissingFileReportsNotFound()
    {
        const Result<QString> read =
            FileSystem::readTextFile(path(QStringLiteral("absent.txt")));
        QVERIFY(!static_cast<bool>(read));
        QCOMPARE(read.error().code(), ErrorCode::NotFound);
    }

    void readingDirectoryIsRejected()
    {
        const Result<QString> read = FileSystem::readTextFile(m_dir->path());
        QVERIFY(!static_cast<bool>(read));
        QCOMPARE(read.error().code(), ErrorCode::InvalidArgument);
    }

    void oversizedFileIsRefusedRatherThanLoaded()
    {
        // The editor cannot usefully open an enormous file; failing cleanly beats
        // exhausting memory trying.
        const QString file = path(QStringLiteral("big.bin"));
        QVERIFY(static_cast<bool>(
            FileSystem::writeTextFile(file, QString(4096, QLatin1Char('x')))));

        const Result<QString> read = FileSystem::readTextFile(file, 1024);
        QVERIFY(!static_cast<bool>(read));
        QCOMPARE(read.error().code(), ErrorCode::NotSupported);
    }

    void overwriteLeavesNoTemporaryFilesBehind()
    {
        // Atomic writes go through a temporary; it must not survive the commit.
        const QString file = path(QStringLiteral("atomic.txt"));
        QVERIFY(static_cast<bool>(FileSystem::writeTextFile(file, QStringLiteral("v1"))));
        QVERIFY(static_cast<bool>(FileSystem::writeTextFile(file, QStringLiteral("v2"))));

        const Result<QString> read = FileSystem::readTextFile(file);
        QVERIFY(static_cast<bool>(read));
        QCOMPARE(read.value(), QStringLiteral("v2"));

        const Result<QList<DirEntry>> entries = FileSystem::listDirectory(m_dir->path());
        QVERIFY(static_cast<bool>(entries));
        for (const DirEntry& entry : entries.value()) {
            QVERIFY2(!entry.name.contains(QStringLiteral(".tmp")),
                     qPrintable(entry.name));
        }
    }

    // ---- Directory listing ------------------------------------------------

    void listingSortsDirectoriesBeforeFiles()
    {
        // The explorer reads as a tree only if folders lead; an alphabetised mix
        // hides the project's shape.
        QVERIFY(static_cast<bool>(
            FileSystem::createDirectory(path(QStringLiteral("zzz-folder")))));
        QVERIFY(static_cast<bool>(
            FileSystem::createFile(path(QStringLiteral("aaa-file.txt")))));

        const Result<QList<DirEntry>> entries = FileSystem::listDirectory(m_dir->path());
        QVERIFY(static_cast<bool>(entries));
        QCOMPARE(entries.value().size(), 2);
        QCOMPARE(entries.value().at(0).name, QStringLiteral("zzz-folder"));
        QVERIFY(entries.value().at(0).isDirectory);
        QCOMPARE(entries.value().at(1).name, QStringLiteral("aaa-file.txt"));
    }

    void listingIsCaseInsensitiveButStable()
    {
        for (const QString& name : {QStringLiteral("Beta.txt"), QStringLiteral("alpha.txt"),
                                    QStringLiteral("Gamma.txt")}) {
            QVERIFY(static_cast<bool>(FileSystem::createFile(path(name))));
        }

        const Result<QList<DirEntry>> entries = FileSystem::listDirectory(m_dir->path());
        QVERIFY(static_cast<bool>(entries));
        QCOMPARE(entries.value().size(), 3);
        QCOMPARE(entries.value().at(0).name, QStringLiteral("alpha.txt"));
        QCOMPARE(entries.value().at(1).name, QStringLiteral("Beta.txt"));
        QCOMPARE(entries.value().at(2).name, QStringLiteral("Gamma.txt"));
    }

    void listingIncludesHiddenFiles()
    {
        // A developer's project is full of dotfiles that matter. Hiding them by
        // default would make the explorer lie about what is on disk.
        QVERIFY(static_cast<bool>(
            FileSystem::createFile(path(QStringLiteral(".gitignore")))));

        const Result<QList<DirEntry>> entries = FileSystem::listDirectory(m_dir->path());
        QVERIFY(static_cast<bool>(entries));
        QCOMPARE(entries.value().size(), 1);
        QCOMPARE(entries.value().at(0).name, QStringLiteral(".gitignore"));
    }

    void listingMissingDirectoryReportsNotFound()
    {
        const Result<QList<DirEntry>> entries =
            FileSystem::listDirectory(path(QStringLiteral("absent")));
        QVERIFY(!static_cast<bool>(entries));
        QCOMPARE(entries.error().code(), ErrorCode::NotFound);
    }

    void listingHonoursCancellation()
    {
        QVERIFY(static_cast<bool>(FileSystem::createFile(path(QStringLiteral("a.txt")))));

        CancellationSource source;
        source.cancel();

        const Result<QList<DirEntry>> entries =
            FileSystem::listDirectory(m_dir->path(), source.token());
        QVERIFY(!static_cast<bool>(entries));
        QCOMPARE(entries.error().code(), ErrorCode::Cancelled);
    }

    // ---- Mutating operations ----------------------------------------------

    void createRefusesToClobberExistingPath()
    {
        const QString file = path(QStringLiteral("existing.txt"));
        QVERIFY(static_cast<bool>(FileSystem::createFile(file)));

        const Status again = FileSystem::createFile(file);
        QVERIFY(!static_cast<bool>(again));
        QCOMPARE(again.error().code(), ErrorCode::AlreadyExists);
    }

    void renameMovesTheFile()
    {
        const QString from = path(QStringLiteral("before.txt"));
        const QString to = path(QStringLiteral("after.txt"));
        QVERIFY(static_cast<bool>(FileSystem::writeTextFile(from, QStringLiteral("body"))));

        QVERIFY(static_cast<bool>(FileSystem::rename(from, to)));
        QVERIFY(!FileSystem::exists(from));

        const Result<QString> read = FileSystem::readTextFile(to);
        QVERIFY(static_cast<bool>(read));
        QCOMPARE(read.value(), QStringLiteral("body"));
    }

    void renameRefusesToOverwriteExistingFile()
    {
        // Renaming over a file would destroy it. Refusing and letting the caller
        // decide is the only safe default.
        const QString from = path(QStringLiteral("source.txt"));
        const QString to = path(QStringLiteral("target.txt"));
        QVERIFY(static_cast<bool>(FileSystem::writeTextFile(from, QStringLiteral("a"))));
        QVERIFY(static_cast<bool>(FileSystem::writeTextFile(to, QStringLiteral("b"))));

        const Status status = FileSystem::rename(from, to);
        QVERIFY(!static_cast<bool>(status));
        QCOMPARE(status.error().code(), ErrorCode::AlreadyExists);

        // The would-be victim must be untouched.
        const Result<QString> read = FileSystem::readTextFile(to);
        QVERIFY(static_cast<bool>(read));
        QCOMPARE(read.value(), QStringLiteral("b"));
    }

    void renamingMissingPathReportsNotFound()
    {
        const Status status = FileSystem::rename(path(QStringLiteral("absent.txt")),
                                                 path(QStringLiteral("new.txt")));
        QVERIFY(!static_cast<bool>(status));
        QCOMPARE(status.error().code(), ErrorCode::NotFound);
    }

    // ---- Watching ---------------------------------------------------------

    void watcherCoalescesABurstIntoOneSignal()
    {
        // A build or branch switch touches many files at once. Reacting per event
        // would rebuild the explorer hundreds of times.
        FileWatcher watcher;
        watcher.watchDirectory(m_dir->path());
        QCOMPARE(watcher.watchedDirectoryCount(), 1);

        QSignalSpy spy(&watcher, &FileWatcher::directoriesChanged);

        for (int i = 0; i < 20; ++i) {
            QVERIFY(static_cast<bool>(FileSystem::createFile(
                path(QStringLiteral("burst-%1.txt").arg(i)))));
        }

        QVERIFY(spy.wait(FileWatcher::kCoalesceIntervalMs * 12));
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toStringList().size(), 1);
    }

    void watcherIgnoresSuppressedPaths()
    {
        // Keys must not report its own writes back to itself as external changes.
        FileWatcher watcher;
        watcher.watchDirectory(m_dir->path());
        watcher.suppress(m_dir->path());

        QSignalSpy spy(&watcher, &FileWatcher::directoriesChanged);
        QVERIFY(static_cast<bool>(
            FileSystem::createFile(path(QStringLiteral("suppressed.txt")))));

        QVERIFY(!spy.wait(FileWatcher::kCoalesceIntervalMs * 4));
        QCOMPARE(spy.count(), 0);
    }

    void clearReleasesEveryWatch()
    {
        FileWatcher watcher;
        watcher.watchDirectory(m_dir->path());
        QVERIFY(static_cast<bool>(FileSystem::createFile(path(QStringLiteral("f.txt")))));
        watcher.watchFile(path(QStringLiteral("f.txt")));

        QCOMPARE(watcher.watchedDirectoryCount(), 1);
        QCOMPARE(watcher.watchedFileCount(), 1);

        watcher.clear();
        QCOMPARE(watcher.watchedDirectoryCount(), 0);
        QCOMPARE(watcher.watchedFileCount(), 0);
    }

    void watchingTheSamePathTwiceIsHarmless()
    {
        FileWatcher watcher;
        watcher.watchDirectory(m_dir->path());
        watcher.watchDirectory(m_dir->path());
        QCOMPARE(watcher.watchedDirectoryCount(), 1);
    }
};

QTEST_MAIN(FileSystemTests)
#include "FileSystemTests.moc"
