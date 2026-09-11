#pragma once

#include <optional>
#include <string>

namespace currency::cache {

// Generic cache abstraction so application services depend on this
// interface, not a concrete cache implementation -- same Dependency
// Inversion reasoning as external::IExchangeRateProvider. Keyed by
// std::string: callers build a human-readable, domain-specific key (e.g.
// "USD|EUR|2024-01-01|2024-02-01") so cache log lines stay meaningful
// without the cache itself needing to understand what a key means.
template <typename Value>
class ICache {
public:
    virtual ~ICache() = default;

    virtual std::optional<Value> get(const std::string& key) = 0;
    virtual void put(const std::string& key, Value value) = 0;
};

} // namespace currency::cache
