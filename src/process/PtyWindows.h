#pragma once

#include "process/Pty.h"

#include <QThread>

#include <atomic>
#include <memory>

namespace keys::process {

/// Reads a pipe on its own thread and reports what it finds.
///
/// ConPTY's output pipe has no readiness notification that Qt's event loop can
/// wait on, so the read has to block. It therefore runs on a dedicated thread
/// rather than the UI thread — a blocking read there would freeze the window
/// between the shell's outputs.
class PtyReader : public QThread {
    Q_OBJECT

public:
    PtyReader(void* pipe, QObject* parent = nullptr);

    /// Asks the loop to stop. The blocking read is released by closing the pipe
    /// from the owner, which makes ReadFile return.
    void requestStop() { m_stop.store(true, std::memory_order_release); }

signals:
    /// Emitted from the reader thread; consumers connect queued so the bytes
    /// arrive on the UI thread.
    void dataRead(const QByteArray& data);
    void pipeClosed();

protected:
    void run() override;

private:
    void* m_pipe = nullptr;
    std::atomic_bool m_stop{false};
};

/// A pseudo-terminal over the Windows ConPTY API.
///
/// The lifecycle is fixed by the API and easy to get subtly wrong, so it is
/// spelled out here:
///
///   1. Two pipes are created — one Keys writes to and the console reads, one
///      the console writes to and Keys reads.
///   2. CreatePseudoConsole takes the console's ends of those pipes. Keys keeps
///      only its own ends; the console's are closed immediately, because ConPTY
///      duplicates them and a lingering handle prevents the pipe from ever
///      reporting end-of-file.
///   3. The child is started with the pseudo-console attached through a
///      thread-attribute list, which is what makes it a real terminal session
///      rather than a process with redirected stdio.
///   4. On teardown the write pipe is closed first, then the console, then the
///      process is waited on — closing the console while the child still writes
///      is what produces the classic ConPTY hang.
class PtyWindows : public Pty {
    Q_OBJECT

public:
    explicit PtyWindows(QObject* parent = nullptr);
    ~PtyWindows() override;

    core::Status start(const PtyOptions& options) override;
    core::Status write(const QByteArray& data) override;
    core::Status resize(int columns, int rows) override;
    void terminate() override;

    [[nodiscard]] bool isRunning() const override;

private:
    /// Releases every handle in the correct order. Safe to call twice.
    void cleanup();

    /// Waits for the child and reports its exit code, once.
    void reportExit();

    /// Opaque so this header does not drag <windows.h> into the rest of Keys:
    /// that header defines macros that collide with ordinary identifiers.
    struct Impl;
    std::unique_ptr<Impl> m_impl;

    PtyReader* m_reader = nullptr;
    bool m_exitReported = false;
};

} // namespace keys::process
