#include "application/ConstraintsService.h"

#include <chrono>
#include <fmt/chrono.h>

namespace currency::application {

namespace {

std::string todayIsoDate() {
    const auto today = std::chrono::floor<std::chrono::days>(std::chrono::system_clock::now());
    return fmt::format("{:%Y-%m-%d}", today);
}

} // namespace

domain::Constraints ConstraintsService::getConstraints() const {
    return domain::Constraints{
        .minAmount = kMinAmount,
        .maxAmount = kMaxAmount,
        .minDate = "1999-01-04",
        .maxDate = todayIsoDate(),
    };
}

} // namespace currency::application
