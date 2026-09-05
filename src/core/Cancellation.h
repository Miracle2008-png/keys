#pragma once

#include <atomic>
#include <memory>

namespace keys::core {

/// Shared cancellation flag. Separated from the token so a caller can hold the
/// source and hand out cheap copies of the token.
class CancellationSource;

/// A copyable, thread-safe handle a worker polls to learn it has been superseded.
///
/// The architecture requires cancellation on anything the user can retrigger —
/// search, indexing, completion. A superseded task must stop promptly rather than
/// race its replacement to the UI.
class CancellationToken {
public:
    /// A token that is never cancelled. For operations with no cancelling owner.
    CancellationToken() = default;

    [[nodiscard]] bool isCancelled() const noexcept
    {
        return m_flag && m_flag->load(std::memory_order_acquire);
    }

private:
    friend class CancellationSource;
    explicit CancellationToken(std::shared_ptr<std::atomic_bool> flag)
        : m_flag(std::move(flag)) {}

    std::shared_ptr<std::atomic_bool> m_flag;
};

/// Owns the cancellation state and produces tokens for it.
///
/// Destroying the source does not cancel: workers may legitimately outlive the
/// requester, and silent cancellation on destruction would be a subtle bug. Call
/// cancel() to mean it.
class CancellationSource {
public:
    CancellationSource() : m_flag(std::make_shared<std::atomic_bool>(false)) {}

    CancellationSource(const CancellationSource&) = delete;
    CancellationSource& operator=(const CancellationSource&) = delete;
    CancellationSource(CancellationSource&&) noexcept = default;
    CancellationSource& operator=(CancellationSource&&) noexcept = default;

    void cancel() noexcept { m_flag->store(true, std::memory_order_release); }

    [[nodiscard]] bool isCancelled() const noexcept
    {
        return m_flag->load(std::memory_order_acquire);
    }

    [[nodiscard]] CancellationToken token() const { return CancellationToken(m_flag); }

private:
    std::shared_ptr<std::atomic_bool> m_flag;
};

} // namespace keys::core
