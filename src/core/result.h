#pragma once

#include <string>
#include <utility>
#include <variant>

namespace imdj {

class Error {
public:
    Error() = default;
    explicit Error(std::string message) : message_(std::move(message)) {}

    const std::string& message() const { return message_; }

private:
    std::string message_;
};

template <typename T = void>
class Result;

template <>
class Result<void> {
public:
    Result() = default;
    Result(Error error) : error_(std::move(error)) {}

    static Result Ok() { return Result(); }
    static Result Fail(std::string message) { return Result(Error(std::move(message))); }

    bool ok() const { return error_.message().empty(); }
    explicit operator bool() const { return ok(); }
    const std::string& error() const { return error_.message(); }

private:
    Error error_;
};

using Status = Result<void>;

template <typename T>
class Result {
public:
    Result(T value) : storage_(std::move(value)) {}
    Result(Error error) : storage_(std::move(error)) {}

    static Result Fail(std::string message) { return Result(Error(std::move(message))); }

    bool ok() const { return std::holds_alternative<T>(storage_); }
    explicit operator bool() const { return ok(); }

    const T& value() const { return std::get<T>(storage_); }
    T& value() { return std::get<T>(storage_); }
    T valueOr(T fallback) const { return ok() ? std::get<T>(storage_) : std::move(fallback); }

    const std::string& error() const
    {
        static const std::string EMPTY_STRING;
        return ok() ? EMPTY_STRING : std::get<Error>(storage_).message();
    }

private:
    std::variant<T, Error> storage_;
};

} // namespace imdj
