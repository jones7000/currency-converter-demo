#pragma once

#include <string>

namespace currency::domain {

// 1 unit of `base` equals `rate` units of `quote`, valid as of `date`
// (ISO 8601, YYYY-MM-DD). Used both for a single "latest" rate and as one
// point in a historical series.
struct ExchangeRate {
    std::string base;
    std::string quote;
    double rate = 0.0;
    std::string date;
};

} // namespace currency::domain
