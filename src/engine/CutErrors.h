#pragma once
#include <QString>
#include <atomic>
#include <stdexcept>

namespace vidcut {

// Recoverable, user-presentable error (message is UTF-8 inside).
class CutError : public std::runtime_error {
public:
    explicit CutError(const QString& message)
        : std::runtime_error(message.toStdString()) {}
};

// Thrown when the user pressed Cancel.
struct CutCancelled {};

// Thread-safe cancel flag shared between the UI thread and the worker.
class CancelToken {
public:
    void cancel() { m_flag.store(true); }
    bool isCancelled() const { return m_flag.load(); }

private:
    std::atomic<bool> m_flag{false};
};

}  // namespace vidcut
