// Renders the real SearchPanel against a real project and reports what it drew.
//
// Not a unit test: SearchUiTests covers the model. This loads the actual QML -
// SearchPanel, SearchResultRow, the delegate bindings, the required properties -
// against a live SearchModel, and fails if anything in that chain is wrong. A
// missing `required property`, a role name that does not match, a binding to a
// property that does not exist: all of it is silent at build time and only shows
// up as an empty or broken panel at runtime.
//
// Run detached; it writes its report beside the executable.
//
//     cmake --build build --target keys_search_panel_check
//     powershell -Command "Start-Process build/bin/keys_search_panel_check.exe"

#include "core/TaskScheduler.h"
#include "project/Project.h"
#include "search/FileIndex.h"
#include "search/TextSearch.h"
#include "ui/AppController.h"
#include "ui/SearchModel.h"

#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QTextStream>
#include <QQmlError>
#include <QTimer>

#include <memory>

using namespace keys;

namespace {

/// Walks the item tree counting what was actually instantiated, since a panel
/// that renders nothing and a panel that renders correctly both "load".
int countVisibleTexts(QQuickItem* item, QStringList& texts)
{
    if (!item) {
        return 0;
    }

    int found = 0;
    if (item->metaObject()->indexOfProperty("text") >= 0 && item->isVisible()) {
        const QString text = item->property("text").toString();
        if (!text.isEmpty()) {
            texts.append(text);
            ++found;
        }
    }
    for (QQuickItem* child : item->childItems()) {
        found += countVisibleTexts(child, texts);
    }
    return found;
}

} // namespace

int main(int argc, char* argv[])
{
    QGuiApplication app(argc, argv);

    QFile report(QCoreApplication::applicationDirPath()
                 + QStringLiteral("/search-panel-report.txt"));
    if (!report.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return 2;
    }
    QTextStream out(&report);

    // A real project: this repository, which is guaranteed to contain the
    // string being searched for.
    const QString root = QDir::current().absolutePath();
    out << "project: " << root << "\n";

    core::TaskScheduler scheduler;
    project::Project projectRoot;
    if (const core::Status status = projectRoot.open(root); !status) {
        out << "FAILED to open the project: " << status.error().toString() << "\n";
        return 1;
    }

    search::FileIndex index(projectRoot, scheduler);
    search::TextSearch textSearch(projectRoot, index, scheduler);
    ui::SearchModel model(textSearch);
    ui::SearchModel::setInstance(&model);

    index.rebuild();

    QQmlEngine engine;

    // The QML module is built into build/qml, not installed beside this tool.
    // Without this the import resolves to nothing and every panel check would
    // report a failure that is the harness, not the panel.
    engine.addImportPath(QCoreApplication::applicationDirPath()
                         + QStringLiteral("/../qml"));

    // The panel reads App for its animation durations and hasProject, and Theme
    // for its colours; both are singletons the application publishes.
    QObject::connect(&index, &search::FileIndex::changed, &app, [&] {
        out << "indexed " << index.files().size() << " files\n";
        out.flush();
    });

    QTimer::singleShot(2000, [&] {
        model.setQuery(QStringLiteral("escapeForDisplay"));
        model.searchNow();
    });

    QTimer::singleShot(5000, [&] {
        out << "--- model ---\n";
        out << "searching: " << (model.isSearching() ? "yes" : "no") << "\n";
        out << "matches: " << model.matchCount() << " in "
            << model.fileCount() << " files\n";
        out << "rows: " << model.rowCount() << "\n";

        const QHash<int, QByteArray> names = model.roleNames();
        auto role = [&](int row, const char* name) {
            for (auto it = names.cbegin(); it != names.cend(); ++it) {
                if (it.value() == name) {
                    return model.data(model.index(row, 0), it.key()).toString();
                }
            }
            return QString();
        };

        for (int row = 0; row < model.rowCount() && row < 12; ++row) {
            const bool isFile = role(row, "kind").toInt() == ui::SearchModel::FileRow;
            if (isFile) {
                out << "  FILE  " << role(row, "path")
                    << "  (" << role(row, "matchCount") << ")\n";
            } else {
                out << "    " << role(row, "line") << ": "
                    << role(row, "lineText").trimmed().left(70) << "\n";
            }
        }

        // Now the panel itself. This is the part a model test cannot reach:
        // whether SearchPanel and SearchResultRow instantiate against this
        // model and put text on screen.
        QQmlComponent component(&engine);
        component.setData(
            "import QtQuick\n"
            "import Keys.Ui\n"
            "SearchPanel { width: 300; height: 600 }\n",
            QUrl(QStringLiteral("qrc:/panel-check.qml")));

        if (component.isError()) {
            out << "--- panel ---\n";
            for (const QQmlError& error : component.errors()) {
                out << "  QML ERROR: " << error.toString() << "\n";
            }
            out << "VERDICT: FAILED - the panel did not compile.\n";
            out.flush();
            app.quit();
            return;
        }

        const std::unique_ptr<QObject> object(component.create());
        auto* panel = qobject_cast<QQuickItem*>(object.get());
        if (!panel) {
            out << "VERDICT: FAILED - the panel did not instantiate.\n";
            out.flush();
            app.quit();
            return;
        }

        // Delegates are created lazily, so the view needs a turn of the event
        // loop at a real size before there is anything to count.
        QCoreApplication::processEvents();

        QStringList texts;
        const int drawn = countVisibleTexts(panel, texts);

        out << "--- panel ---\n";
        out << "visible text items: " << drawn << "\n";
        for (const QString& text : texts.mid(0, 20)) {
            out << "  " << text.left(70) << "\n";
        }

        out << (model.rowCount() > 0 && drawn > 0
                    ? "VERDICT: the panel rendered the results.\n"
                    : "VERDICT: FAILED - model rows or panel text missing.\n");
        out.flush();
        app.quit();
    });

    return app.exec();
}
