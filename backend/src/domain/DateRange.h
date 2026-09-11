#pragma once

#include <cctype>
#include <string>
#include <utility>

#include "domain/Errors.h"

namespace currency::domain {

namespace detail {

// Structural check only (YYYY-MM-DD). Calendar validity (e.g. Feb 30) and
// whether the date actually has published data are frankfurter.dev's
// concern, not ours to re-validate.
inline bool isIsoDate(const std::string& s) {
    if (s.size() != 10 || s[4] != '-' || s[7] != '-') {
        return false;
    }
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (i == 4 || i == 7) continue;
        if (!std::isdigit(static_cast<unsigned char>(s[i]))) {
            return false;
        }
    }
    return true;
}

} // namespace detail

// A closed date interval [startDate, endDate], both ISO 8601 (YYYY-MM-DD).
// Enforces the one invariant that doesn't depend on external context (valid
// format, start <= end). Bounds against frankfurter.dev's actual supported
// range are enforced in the application layer, which has that context via
// IExchangeRateProvider.
class DateRange {
public:
    DateRange(std::string startDate, std::string endDate)
        : startDate_(std::move(startDate)), endDate_(std::move(endDate)) {
        if (!detail::isIsoDate(startDate_) || !detail::isIsoDate(endDate_)) {
            throw InvalidArgumentException("dates must be in YYYY-MM-DD format");
        }
        if (endDate_ < startDate_) {
            throw InvalidArgumentException("start_date must not be after end_date");
        }
    }

    const std::string& startDate() const { return startDate_; }
    const std::string& endDate() const { return endDate_; }

private:
    std::string startDate_;
    std::string endDate_;
};

} // namespace currency::domain
