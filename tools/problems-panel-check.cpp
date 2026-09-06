// Renders the real ProblemsPanel against real diagnostics.
//
// The panel cannot be seen doing its job on a machine with no language server
// installed - it correctly shows "nothing analysed yet" - so this publishes
// diagnostics through the same signal a server would and reports what the panel
// drew. What it proves is the part a model test cannot: that the delegate's
// required properties match the model's role names, and that the QML compiles
// and instantiates.
//
//     cmake --build build --target keys_problems_panel_check
//     powershell -Command "Start-Process build/bin/keys_problems_panel_check.exe"

#include "config/Settings.h"
#include "langsvc/LanguageServiceManager.h"
#include "project/Project.h"
#include "ui/ProblemsModel.h"
#include "ui/Theme.h"

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

langsvc::Diagnostic make(int line, langsvc::DiagnosticSeverity severity,
                         const QString& message)
{
    langsvc::Diagnostic diagnostic;
    diagnostic.range.start.line = line;
    diagnostic.range.start.character = 8;
    diagnostic.severity = severity;
    diagnostic.message = message;
    diagnostic.source = QStringLiteral("clangd");
    return diagnostic;
}

} // namespace

int main(int argc, char* argv[])
{
    QGuiApplication app(argc, argv);

    QFile report(QCoreApplication::applicationDirPath()
                 + QStringLiteral("/problems-panel-report.txt"));
    if (!report.open(QIODevice::WriteOnly | QIODevice::Text)) {
        return 2;
    }
    QTextStream out(&report);

    config::Settings settings;
    ui::Theme theme(settings);
    ui::Theme::setInstance(&theme);

    langsvc::LanguageServiceManager services;
    project::Project projectRoot;

    ui::ProblemsModel model(services, projectRoot);
    ui::ProblemsModel::setInstance(&model);

    QQmlEngine engine;
    engine.addImportPath(QCoreApplication::applicationDirPath()
                         + QStringLiteral("/../qml"));

    // Exactly what a server publishes: per file, with an empty list meaning
    // clean.
    emit services.diagnosticsPublished(
        QStringLiteral("/project/src/main.cpp"),
        {make(11, langsvc::DiagnosticSeverity::Error,
              QStringLiteral("use of undeclared identifier 'widht'")),
         make(40, langsvc::DiagnosticSeverity::Warning,
              QStringLiteral("unused variable 'scratch'"))});

    emit services.diagnosticsPublished(
        QStringLiteral("/project/src/parser.cpp"),
        {make(3, langsvc::DiagnosticSeverity::Warning,
              QStringLiteral("comparison of integers of different signs"))});

    out << "--- model ---\n";
    out << "files: " << model.fileCount() << "\n";
    out << "errors: " << model.errorCount()
        << " warnings: " << model.warningCount() << "\n";
    out << "rows: " << model.rowCount() << "\n";
    out << "analysed: " << (model.hasAnalysed() ? "yes" : "no") << "\n";

    QQmlComponent component(&engine);
    component.setData(
        "import QtQuick\n"
        "import Keys.Ui\n"
        "ProblemsPanel { width: 900; height: 300 }\n",
        QUrl(QStringLiteral("qrc:/problems-check.qml")));

    out << "--- panel ---\n";
    if (component.isError()) {
        for (const QQmlError& error : component.errors()) {
            out << "  QML ERROR: " << error.toString() << "\n";
        }
        out << "VERDICT: FAILED - the panel did not compile.\n";
        return 1;
    }

    const std::unique_ptr<QObject> object(component.create());
    auto* panel = qobject_cast<QQuickItem*>(object.get());
    if (!panel) {
        out << "VERDICT: FAILED - the panel did not instantiate.\n";
        return 1;
    }

    QCoreApplication::processEvents();

    QStringList texts;
    const int drawn = countVisibleTexts(panel, texts);
    out << "visible text items: " << drawn << "\n";
    for (const QString& text : texts.mid(0, 20)) {
        out << "  " << text.left(80) << "\n";
    }

    const bool ok = model.fileCount() == 2 && model.errorCount() == 1
                    && model.warningCount() == 2 && drawn > 0;
    out << (ok ? "VERDICT: the panel rendered the diagnostics.\n"
               : "VERDICT: FAILED - see above.\n");
    out.flush();
    return ok ? 0 : 1;
}
