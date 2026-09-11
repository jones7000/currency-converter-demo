#pragma once

#include <string>

namespace currency::domain {

// A single ISO 4217 currency, as returned by the currency overview listing.
struct Currency {
    std::string code;      // e.g. "USD"
    std::string fullName;  // e.g. "United States Dollar"
};

} // namespace currency::domain
