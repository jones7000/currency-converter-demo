#pragma once

#include "domain/Constraints.h"

namespace currency::application {

// Server-declared UI bounds. No external dependency: the amount bounds
// are application-defined policy, and
// frankfurter.dev's earliest supported date (1999-01-04) is a stable,
// documented fact of that API rather than something worth an extra network
// round-trip to rediscover.
class ConstraintsService {
public:
    domain::Constraints getConstraints() const;

private:
    static constexpr double kMinAmount = 0.01;
    static constexpr double kMaxAmount = 1'000'000'000.0;
};

} // namespace currency::application
