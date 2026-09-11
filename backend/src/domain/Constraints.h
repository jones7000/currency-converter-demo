#pragma once

#include <string>

namespace currency::domain {

// UI bounds served to the client via GetConstraints so the frontend never
// hardcodes business rules (dumb-client principle).
struct Constraints {
    double minAmount = 0.0;
    double maxAmount = 0.0;
    std::string minDate;  // earliest date frankfurter.dev has data for
    std::string maxDate;  // latest selectable date (today)
};

} // namespace currency::domain
