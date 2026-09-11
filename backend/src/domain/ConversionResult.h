#pragma once

#include <string>

namespace currency::domain {

// Result of a conversion, computed by application::ConversionService.
// Deliberately mirrors currency.ConvertResponse field-for-field so the gRPC
// layer's job is a pure, mechanical mapping -- the conversion math itself
// (amount * rate) happens here, not in the transport layer.
struct ConversionResult {
    double sourceAmount = 0.0;
    double convertedAmount = 0.0;
    double exchangeRate = 0.0;
    std::string rateDate;
};

} // namespace currency::domain
