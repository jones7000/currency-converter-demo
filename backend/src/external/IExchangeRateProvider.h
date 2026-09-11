#pragma once

#include <string>
#include <vector>

#include "domain/Currency.h"
#include "domain/DateRange.h"
#include "domain/ExchangeRate.h"

namespace currency::external {

// Abstraction over the upstream exchange-rate data source. The application
// layer (currency::application::*) is written against this interface only,
// never against FrankfurterClient or Poco::Net directly -- a concrete,
// visible instance of Dependency Inversion.
class IExchangeRateProvider {
public:
    virtual ~IExchangeRateProvider() = default;

    virtual std::vector<domain::Currency> listCurrencies() = 0;

    virtual domain::ExchangeRate latestRate(const std::string& sourceCurrency,
                                             const std::string& targetCurrency) = 0;

    virtual std::vector<domain::ExchangeRate> history(const std::string& sourceCurrency,
                                                        const std::string& targetCurrency,
                                                        const domain::DateRange& range) = 0;
};

} // namespace currency::external
