#pragma once

#include <stdexcept>
#include <string>

namespace currency::domain {

// Base for every domain-level error. Caught exactly once, at the gRPC
// boundary (currency::grpcapi::CurrencyServiceImpl), and mapped to a
// grpc::StatusCode.
class DomainException : public std::runtime_error {
public:
    explicit DomainException(const std::string& message) : std::runtime_error(message) {}
};

// Bad input from the caller: unknown currency code, amount out of the
// configured bounds, malformed/inverted date range, etc.
// -> grpc::StatusCode::INVALID_ARGUMENT
class InvalidArgumentException : public DomainException {
public:
    explicit InvalidArgumentException(const std::string& message) : DomainException(message) {}
};

// The currency code is well-formed but not one frankfurter.dev knows about.
// -> grpc::StatusCode::NOT_FOUND
class CurrencyNotFoundException : public DomainException {
public:
    explicit CurrencyNotFoundException(const std::string& message) : DomainException(message) {}
};

// frankfurter.dev could not be reached (network error, timeout, non-2xx
// status) and no usable cache entry exists.
// -> grpc::StatusCode::UNAVAILABLE
class ExternalApiUnavailableException : public DomainException {
public:
    explicit ExternalApiUnavailableException(const std::string& message) : DomainException(message) {}
};

// frankfurter.dev responded, but with a payload this client doesn't
// understand (schema drift). Detail is logged server-side only; the client
// only ever sees a generic message. -> grpc::StatusCode::INTERNAL
class ExternalApiUnexpectedResponseException : public DomainException {
public:
    explicit ExternalApiUnexpectedResponseException(const std::string& message) : DomainException(message) {}
};

} // namespace currency::domain
