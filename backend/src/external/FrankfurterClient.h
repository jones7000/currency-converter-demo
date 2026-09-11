#pragma once

#include <chrono>
#include <string>
#include <vector>

#include <Poco/Logger.h>

#include "domain/Currency.h"
#include "domain/DateRange.h"
#include "domain/ExchangeRate.h"
#include "external/IExchangeRateProvider.h"

namespace currency::external {

// HTTP client for https://api.frankfurter.dev, built on Poco::Net + Poco::JSON
// (POCO was chosen over Boost.Beast for this role). Owns every HTTP/JSON
// detail; callers only ever see currency::domain types or a
// currency::domain::DomainException.
class FrankfurterClient : public IExchangeRateProvider {
public:
    FrankfurterClient(std::string baseUrl, std::chrono::milliseconds timeout);

    std::vector<domain::Currency> listCurrencies() override;

    domain::ExchangeRate latestRate(const std::string& sourceCurrency,
                                     const std::string& targetCurrency) override;

    std::vector<domain::ExchangeRate> history(const std::string& sourceCurrency,
                                               const std::string& targetCurrency,
                                               const domain::DateRange& range) override;

private:
    // Performs one GET request against baseUrl_ + path, returns the raw
    // response body. Logs the call (URL, duration, status) and translates
    // any network-level failure into ExternalApiUnavailableException.
    std::string get(const std::string& path) const;

    std::string baseUrl_;
    std::chrono::milliseconds timeout_;
    Poco::Logger& logger_;
};

} // namespace currency::external
