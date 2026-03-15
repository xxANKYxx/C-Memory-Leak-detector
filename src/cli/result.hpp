// ╔══════════════════════════════════════════════════════════════════════════════╗
// ║  MemSentry CLI — Result<T> type (C++20 replacement for std::expected)       ║
// ╚══════════════════════════════════════════════════════════════════════════════╝
#pragma once

#include <stdexcept>
#include <string>
#include <variant>

namespace memsentry::cli {

namespace detail {
    struct ErrorHolder {
        std::string message;
    };
}  // namespace detail

/// A simple Result type: holds either a value of type T or an error string.
/// Uses std::variant<T, detail::ErrorHolder> internally to avoid ambiguity
/// when T = std::string.
template <typename T>
class Result {
public:
    // ── Success construction (implicit) ──────────────────────────────────────
    Result(const T& value) : data_(value) {}
    Result(T&& value)      : data_(std::move(value)) {}

    // ── Error factory ────────────────────────────────────────────────────────
    static Result err(std::string msg) {
        return Result(detail::ErrorHolder{std::move(msg)});
    }

    // ── Observers ────────────────────────────────────────────────────────────
    [[nodiscard]] bool has_value() const noexcept {
        return std::holds_alternative<T>(data_);
    }

    [[nodiscard]] explicit operator bool() const noexcept { return has_value(); }

    [[nodiscard]] T& value() {
        if (!has_value())
            throw std::runtime_error("Result has no value: " + error());
        return std::get<T>(data_);
    }
    [[nodiscard]] const T& value() const {
        if (!has_value())
            throw std::runtime_error("Result has no value: " + error());
        return std::get<T>(data_);
    }

    [[nodiscard]] const std::string& error() const {
        return std::get<detail::ErrorHolder>(data_).message;
    }

    // ── Convenience operators ────────────────────────────────────────────────
    T&       operator*()        { return value(); }
    const T& operator*()  const { return value(); }
    T*       operator->()       { return &value(); }
    const T* operator->() const { return &value(); }

private:
    explicit Result(detail::ErrorHolder e) : data_(std::move(e)) {}
    std::variant<T, detail::ErrorHolder> data_;
};

}  // namespace memsentry::cli
