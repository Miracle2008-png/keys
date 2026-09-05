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
| 6. Terminal | Next |
| 7-15 | Planned |

Features that are not implemented yet are visibly absent or explicitly
disabled — nothing in the interface pretends to work.

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
