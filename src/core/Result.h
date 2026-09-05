#pragma once

#include <QString>

#include <optional>
#include <utility>
#include <variant>

namespace keys::core {

/// Why an operation failed. Kept coarse deliberately: callers branch on category,
/// and show `message` to the user. Adding a category is a considered change, not
/// something done per call site.
enum class ErrorCode {
    Unknown,
    NotFound,
    PermissionDenied,
    AlreadyExists,
    InvalidArgument,
    IoError,
    Cancelled,
    Timeout,
    NotSupported,
    ParseError,
    ProcessFailed,
};

/// A failure with a human-readable message. `context` names what was being
/// attempted (a path, a command) so logs are useful without string-building at
/// every throw site.
class Error {
public:
    Error() = default;
    Error(ErrorCode code, QString message, QString context = {})
        : m_code(code), m_message(std::move(message)), m_context(std::move(context)) {}

    [[nodiscard]] ErrorCode code() const noexcept { return m_code; }
    [[nodiscard]] const QString& message() const noexcept { return m_message; }
    [[nodiscard]] const QString& context() const noexcept { return m_context; }

    [[nodiscard]] QString toString() const
    {
        return m_context.isEmpty() ? m_message
                                   : QStringLiteral("%1: %2").arg(m_context, m_message);
    }

    [[nodiscard]] bool isCancellation() const noexcept { return m_code == ErrorCode::Cancelled; }

private:
    ErrorCode m_code = ErrorCode::Unknown;
    QString m_message;
    QString m_context;
};

/// Either a value or an Error.
///
/// Keys does not use exceptions for expected failures — a missing file or a
/// declined permission is an ordinary outcome, not an exceptional one, and making
/// it a return value means callers cannot silently ignore it. Exceptions remain
/// reserved for genuine programming errors.
template <typename T>
class Result {
public:
    Result(T value) : m_data(std::move(value)) {}          // NOLINT(google-explicit-constructor)
    Result(Error error) : m_data(std::move(error)) {}      // NOLINT(google-explicit-constructor)

    [[nodiscard]] bool hasValue() const noexcept { return std::holds_alternative<T>(m_data); }
    [[nodiscard]] explicit operator bool() const noexcept { return hasValue(); }

    [[nodiscard]] T& value() & { return std::get<T>(m_data); }
    [[nodiscard]] const T& value() const& { return std::get<T>(m_data); }
    [[nodiscard]] T&& value() && { return std::get<T>(std::move(m_data)); }

    [[nodiscard]] const Error& error() const& { return std::get<Error>(m_data); }

    /// The value if present, otherwise `fallback`. For paths where a failure has a
    /// sensible default and the caller genuinely does not need to know.
    [[nodiscard]] T valueOr(T fallback) const&
    {
        return hasValue() ? std::get<T>(m_data) : std::move(fallback);
    }

private:
    std::variant<T, Error> m_data;
};

/// Result for operations that either succeed or fail with no value to return.
template <>
class Result<void> {
public:
    Result() = default;
    Result(Error error) : m_error(std::move(error)) {}     // NOLINT(google-explicit-constructor)

    [[nodiscard]] bool hasValue() const noexcept { return !m_error.has_value(); }
    [[nodiscard]] explicit operator bool() const noexcept { return hasValue(); }
    [[nodiscard]] const Error& error() const& { return *m_error; }

private:
    std::optional<Error> m_error;
};

using Status = Result<void>;

/// Reads at call sites as `return Ok();` — clearer than a bare `return {};`.
[[nodiscard]] inline Status Ok() { return {}; }

[[nodiscard]] inline Error Err(ErrorCode code, QString message, QString context = {})
{
    return Error(code, std::move(message), std::move(context));
}

} // namespace keys::core
