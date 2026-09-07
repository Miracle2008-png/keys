# Keys

A fast, focused editor for people who build software for a living.

Keys is a native desktop IDE built on C++23, Qt 6 and QML. It is under active
development; see [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) for the module
layout, the decisions behind it, and the milestone plan.

## Status

| Milestone | State |
|---|---|
| 1. Application shell | Done |
| 2. Project & workspace management | Done |
| 3. File explorer | Done |
| 4. Editor | Done |
| 5. Tabs & editor splits | Done |
| 6. Terminal | Done |
| 7. Search | Done |
| 8. Command palette | Done |
| 9. Settings & themes | Done |
| 10. Git | Done |
| 11. Build/run | Done |
| 12. Language services | Done |
| 13. Debugger | Done |
| 14. Extensions | Done |
| 15. Performance & polish | Done |

Syntax highlighting covers 36 languages across about 300 file mappings,
checked over whole files of idiomatic code rather than snippets
(`tools/language-check.cpp`): Assembly, C/C++, C#, CMake, CSS, Dart, Elixir,
F#, Go, Groovy, Haskell, HTML/XML, Java, JavaScript/JSON, Julia, Kotlin, Lua,
Markdown, Nim, Objective-C, OCaml, Perl, PHP, Python, QML, R, Ruby, Rust,
Scala, Shell, SQL, Swift, TOML/INI, TypeScript, YAML and Zig.

Completion works with or without a language server. With one, it is the
server's - types, scope, real symbols. Without one, it offers words already in
the file plus the language's own reserved words and type names, ranked by
distance from the caret. Nothing is inferred, so nothing can be confidently
wrong, which is the failure that makes a bad autocomplete worse than none.

An extension is mapped only where the file's shape genuinely matches the lexer
it points at - a shader is C, a .csproj is XML, a Jupyter notebook is JSON on
disk. One pointed at the wrong lexer produces confident wrong colour, which
reads as a bug; an unmapped one produces plain text, which reads as a type Keys
does not know yet.

The terminal runs a real shell through ConPTY. The attachment appeared broken
for a long time and was not: **ConPTY behaves differently depending on whether
the parent process owns a console.** From a console parent - and from an
MSYS/Git-Bash pty in particular - the child joins the parent's console instead
of the pseudo-console, and only the handful of bytes ConPTY writes itself ever
reach the pipe. Measured with a minimal program independent of Keys, same
machine, only the subsystem changed: 16 bytes from a console parent, 222 from a
GUI parent. `keys.exe` is a GUI process, so the terminal works; the probe that
said otherwise was a console application measuring its own environment.

`tools/pty-check.cpp` must therefore be run detached, never from a shell.

Extensions run out of process and declare what they need - reading the
workspace, reaching the network, spawning processes - which the user reads in
plain language and grants or does not. Nothing runs before that. They install
from a folder containing a `keys-extension.json`, and uninstalling removes the
files and forgets the grant, so reinstalling asks again.

There is no marketplace. Browsing and installing from a registry needs a server
to host packages and signing so that "install" is not "download and run
arbitrary code with filesystem access", and neither is something the editor can
provide by itself.

Features that are not implemented yet are visibly absent or explicitly
disabled — nothing in the interface pretends to work. The same rule governs
settings: a setting is declared only once something honours it, so the settings
page cannot show a control that changes nothing.

## Building

Requirements:

- CMake 3.28+
- Qt 6.8 or newer (6.10.3 is what development uses)
- A C++23 compiler — MSVC 2022 on Windows, GCC 13+ or Clang 17+ elsewhere
- Ninja

```bash
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=/path/to/Qt/6.10.3/msvc2022_64
cmake --build build
ctest --test-dir build --output-on-failure
```

On Windows, `build.ps1` does the same thing and enters the MSVC environment for
you:

```powershell
.\build.ps1 -Test
```

## Packaging

```bash
cmake --build build --target package
```

Produces a per-user Windows installer that needs no administrator rights. See
[cmake/KeysPackaging.cmake](cmake/KeysPackaging.cmake) for what is included and,
more importantly, what is deliberately left out.

## Icons

The application mark lives in `resources/icons/keys-mark.svg`, with a simplified
variant for small sizes. Regenerate every raster size and the Windows `.ico`
from source with:

```bash
cmake --build build --target icons
```

## Layout

```
src/core         Result types, cancellation, task scheduler, command registry
src/config       Layered settings, schema, animation policy
src/filesystem   Async file I/O, directory listing, file watching
src/project      Root detection, ignore rules, project metadata
src/editor       Piece-table buffer, cursors, selections, undo
src/process      Child processes and pseudo-terminals (ConPTY)
src/terminal     VT parsing and the terminal screen model
src/search       File index, fuzzy matching, project-wide text search
src/workspace    Open project, session state, editor groups and tabs
src/ui           QML views, theme, view models
src/app          Composition root
tests            One test binary per module
tools            Build-time tooling (icon rendering)
```

Dependencies point downward only, and the rule is enforced by CMake rather than
convention: a module outside the UI layer that asks for a Qt GUI component fails
configuration.

## Licence

MIT. See [LICENSE](LICENSE).
