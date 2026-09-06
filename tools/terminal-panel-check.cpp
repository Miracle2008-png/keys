// Drives the real TerminalModel against a real shell and reports what it drew.
//
// The terminal is the one panel whose correctness cannot be established without
// running it: a shell has to start, ConPTY has to deliver its output, the parser
// has to turn escape sequences into runs, and the model has to turn runs into
// markup. Each of those has failed silently at some point, and each failure
// looks identical from outside - an empty panel.
//
// Run detached. ConPTY behaves differently when the parent owns a console; see
// tools/pty-check.cpp for the measurement.
//
//     cmake --build build --target keys_terminal_panel_check
//     powershell -Command "Start-Process build/bin/keys_terminal_panel_check.exe"

#include "config/Settings.h"
#include "ui/TerminalModel.h"
#include "ui/Theme.h"

#include <QDir>
#include <QFile>
#include <QGuiApplication>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQmlError>
#include <QQuickItem>
#include <QTextStream>
#include <QTimer>

#include <memory>

using namespace keys;

namespace {

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
                 + QStringLiteral("/terminal-panel-report.txt"));
    if (!report.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return 2;
    }
    QTextStream out(&report);

    config::Settings settings;
    ui::Theme theme(settings);
    ui::Theme::setInstance(&theme);

    ui::TerminalModel model(theme);
    ui::TerminalModel::setInstance(&model);
    model.setWorkingDirectory(QDir::current().absolutePath());

    QObject::connect(&model, &ui::TerminalModel::errorOccurred,
                     [&out](const QString& message) {
                         out << "ERROR: " << message << "\n";
                         out.flush();
                     });

    QQmlEngine engine;
    engine.addImportPath(QCoreApplication::applicationDirPath()
                         + QStringLiteral("/../qml"));

    out << "starting a shell in " << QDir::current().absolutePath() << "\n";
    model.openSession();
    out << "sessions: " << model.sessionCount() << "\n";
    out << "title: " << model.title() << "\n";
    out.flush();

    // Let the shell print its banner and prompt, then run something whose
    // output is unmistakable.
    QTimer::singleShot(3000, [&] {
        out << "lines after the banner: " << model.rowCount() << "\n";
        model.sendText(QStringLiteral("echo KEYS_TERMINAL_OK"));
        model.sendKey(Qt::Key_Return, Qt::NoModifier);
        out.flush();
    });

    QTimer::singleShot(7000, [&] {
        out << "--- model ---\n";
        out << "running: " << (model.isRunning() ? "yes" : "no") << "\n";
        out << "lines: " << model.rowCount() << "\n";
        out << "revision: " << model.revision() << "\n";

        const QString all = model.plainText();
        const bool echoed = all.contains(QStringLiteral("KEYS_TERMINAL_OK"));

        out << "--- screen ---\n";
        for (const QString& line : all.split(QLatin1Char('\n'))) {
            if (!line.trimmed().isEmpty()) {
                out << "  " << line.left(90) << "\n";
            }
        }

        // The panel itself: whether TerminalPanel instantiates against this
        // model and puts the shell's output on screen.
        QQmlComponent component(&engine);
        component.setData(
            "import QtQuick\n"
            "import Keys.Ui\n"
            "TerminalPanel { width: 900; height: 400 }\n",
            QUrl(QStringLiteral("qrc:/terminal-check.qml")));

        out << "--- panel ---\n";
        if (component.isError()) {
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

        QCoreApplication::processEvents();

        QStringList texts;
        const int drawn = countVisibleTexts(panel, texts);
        out << "visible text items: " << drawn << "\n";

        out << (echoed && model.rowCount() > 0
                    ? "VERDICT: the shell ran a command and the model has its output.\n"
                    : "VERDICT: FAILED - the command was not echoed back.\n");
        out.flush();

        model.closeSession(0);
        app.quit();
    });

    return app.exec();
}
