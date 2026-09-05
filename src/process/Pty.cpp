// The platform-independent parts of Pty live in the platform implementation
// files: create() and defaultShell() are defined by whichever backend is built.
// This translation unit exists so the module has a source file on every
// platform, keeping the CMake target uniform.

#include "process/Pty.h"
