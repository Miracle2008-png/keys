// Saves a document to every kind of file Keys claims to handle, and reads it
// back.
//
// The question this answers is "can I save a file with any extension, in any
// language" - and the honest way to answer it is to write one of each and
// compare bytes, not to reason about the code. It goes through the same
// Workspace::saveFile the editor uses, so the on-save transformations and the
// encoding are exercised too, rather than a direct FileSystem call that would
// skip both.
//
//     cmake --build build --target keys_save_check
//     build/bin/keys_save_check

#include "config/Settings.h"
#include "core/TaskScheduler.h"
#include "editor/SyntaxHighlighter.h"
#include "workspace/Workspace.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QTemporaryDir>
#include <QTextStream>

using namespace keys;

namespace {

int g_failures = 0;

void check(QTextStream& out, const QString& name, bool ok, const QString& detail)
{
    out << (ok ? QStringLiteral("  ok  ") : QStringLiteral("  FAIL")) << "  "
        << name.leftJustified(22) << detail << "\n";
    if (!ok) {
        ++g_failures;
    }
}

} // namespace

int main(int argc, char* argv[])
{
    QCoreApplication app(argc, argv);
    QTextStream out(stdout);

    QTemporaryDir dir;
    if (!dir.isValid()) {
        out << "could not create a scratch directory\n";
        return 2;
    }

    config::Settings settings;
    core::TaskScheduler scheduler;
    workspace::Workspace workspace(settings, scheduler);

    if (const core::Status status = workspace.openProject(dir.path()); !status) {
        out << "could not open the scratch project: "
            << status.error().toString() << "\n";
        return 1;
    }

    // One file per language, with content that actually belongs to it - so a
    // save that mangled quotes, braces or non-ASCII would show up.
    const std::vector<std::pair<QString, QString>> samples = {
        {QStringLiteral("main.cpp"),   QStringLiteral("#include <string>\nint main() { return 0; }\n")},
        {QStringLiteral("app.py"),     QStringLiteral("def greet(name: str) -> str:\n    return f\"hi {name}\"\n")},
        {QStringLiteral("lib.rs"),     QStringLiteral("pub fn add(a: i32, b: i32) -> i32 { a + b }\n")},
        {QStringLiteral("main.go"),    QStringLiteral("package main\n\nfunc main() {}\n")},
        {QStringLiteral("App.java"),   QStringLiteral("public class App { }\n")},
        {QStringLiteral("Program.cs"), QStringLiteral("public record Invoice(int Id);\n")},
        {QStringLiteral("app.swift"),  QStringLiteral("struct A { let x: Int }\n")},
        {QStringLiteral("index.php"),  QStringLiteral("<?php\ndeclare(strict_types=1);\n")},
        {QStringLiteral("Main.kt"),    QStringLiteral("fun main() = println(\"hi\")\n")},
        {QStringLiteral("main.dart"),  QStringLiteral("void main() { print('hi'); }\n")},
        {QStringLiteral("App.scala"),  QStringLiteral("case class A(x: Int)\n")},
        {QStringLiteral("init.lua"),   QStringLiteral("local function f() return nil end\n")},
        {QStringLiteral("run.pl"),     QStringLiteral("use strict;\nprint \"hi\\n\";\n")},
        {QStringLiteral("plot.r"),     QStringLiteral("f <- function(x) x + 1\n")},
        {QStringLiteral("Main.hs"),    QStringLiteral("main :: IO ()\nmain = putStrLn \"hi\"\n")},
        {QStringLiteral("app.ex"),     QStringLiteral("defmodule A do\nend\n")},
        {QStringLiteral("lib.ml"),     QStringLiteral("let f x = x + 1\n")},
        {QStringLiteral("Lib.fs"),     QStringLiteral("let f x = x + 1\n")},
        {QStringLiteral("main.zig"),   QStringLiteral("pub fn main() void {}\n")},
        {QStringLiteral("app.nim"),    QStringLiteral("proc f(): int = 1\n")},
        {QStringLiteral("build.groovy"), QStringLiteral("class A { def b() {} }\n")},
        {QStringLiteral("run.jl"),     QStringLiteral("function f(x) x end\n")},
        {QStringLiteral("boot.asm"),   QStringLiteral("section .text\n    mov rax, 1\n")},
        {QStringLiteral("view.m"),     QStringLiteral("@interface A : NSObject\n@end\n")},
        {QStringLiteral("app.ts"),     QStringLiteral("export const a: number = 1;\n")},
        {QStringLiteral("app.js"),     QStringLiteral("export const a = 1;\n")},
        {QStringLiteral("page.html"),  QStringLiteral("<!doctype html>\n<p>hi</p>\n")},
        {QStringLiteral("style.css"),  QStringLiteral(".a { color: #3d7eff; }\n")},
        {QStringLiteral("data.json"),  QStringLiteral("{ \"a\": 1 }\n")},
        {QStringLiteral("conf.yaml"),  QStringLiteral("a: 1\nb: [1, 2]\n")},
        {QStringLiteral("conf.toml"),  QStringLiteral("[a]\nb = 1\n")},
        {QStringLiteral("query.sql"),  QStringLiteral("SELECT 1 FROM t;\n")},
        {QStringLiteral("notes.md"),   QStringLiteral("# Title\n\n**bold** and `code`\n")},
        {QStringLiteral("run.sh"),     QStringLiteral("#!/bin/sh\necho hi\n")},
        {QStringLiteral("Main.qml"),   QStringLiteral("import QtQuick\nItem { }\n")},
        {QStringLiteral("app.rb"),     QStringLiteral("def f = 1\n")},

        // Not a language Keys highlights. Saving must still work: an editor
        // that refuses an extension it has no colours for is broken.
        {QStringLiteral("notes.unknown-ext"), QStringLiteral("plain text\n")},
        {QStringLiteral("Makefile"),   QStringLiteral("all:\n\techo hi\n")},

        // Non-ASCII, which is where an encoding bug would show.
        {QStringLiteral("i18n.txt"),   QStringLiteral("héllo wörld — ünïcode ✓ 日本語\n")},
    };

    out << "saving " << samples.size() << " files through Workspace::saveFile\n\n";

    for (const auto& [name, contents] : samples) {
        const QString path = QDir(dir.path()).filePath(name);

        // Create, open, type, save - the sequence the editor performs.
        if (const core::Status created = workspace.createFile(path); !created) {
            check(out, name, false, created.error().toString());
            continue;
        }
        if (const core::Status opened = workspace.openFile(path); !opened) {
            check(out, name, false, opened.error().toString());
            continue;
        }

        editor::TextDocument* document = workspace.activeDocument();
        if (!document) {
            check(out, name, false, QStringLiteral("no active document"));
            continue;
        }

        document->setText(contents);

        if (const core::Status saved = workspace.saveFile(); !saved) {
            check(out, name, false, saved.error().toString());
            continue;
        }

        // Read it back off disk and compare.
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            check(out, name, false, QStringLiteral("could not reopen"));
            continue;
        }
        const QString readBack = QString::fromUtf8(file.readAll());
        file.close();

        const bool identical = readBack == contents;
        const auto language = editor::SyntaxHighlighter::languageForPath(path);
        const bool highlighted =
            language != editor::SyntaxHighlighter::Language::None;

        check(out, name, identical,
              identical
                  ? QStringLiteral("%1 bytes, %2")
                        .arg(contents.toUtf8().size())
                        .arg(highlighted ? QStringLiteral("highlighted")
                                         : QStringLiteral("plain text"))
                  : QStringLiteral("content changed on save"));
    }

    out << "\n"
        << (g_failures == 0
                ? QStringLiteral("VERDICT: every file saved and read back byte-identical.\n")
                : QStringLiteral("VERDICT: FAILED - %1 file(s).\n").arg(g_failures));

    return g_failures == 0 ? 0 : 1;
}
